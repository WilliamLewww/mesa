#include "zink_batch.h"

#include "zink_context.h"
#include "zink_fence.h"
#include "zink_framebuffer.h"
#include "zink_query.h"
#include "zink_program.h"
#include "zink_render_pass.h"
#include "zink_resource.h"
#include "zink_screen.h"
#include "zink_surface.h"

#include "util/hash_table.h"
#include "util/u_debug.h"
#include "util/set.h"

void
zink_reset_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   batch->descs_used = 0;

   // cmdbuf hasn't been submitted before
   if (batch->submitted)
      zink_fence_finish(screen, &ctx->base, batch->fence, PIPE_TIMEOUT_INFINITE);

   zink_framebuffer_reference(screen, &batch->fb, NULL);

   /* unref all used resources */
   set_foreach(batch->resources, entry) {
      struct pipe_resource *pres = (struct pipe_resource *)entry->key;
      pipe_resource_reference(&pres, NULL);
   }
   _mesa_set_clear(batch->resources, NULL);

   /* unref all used sampler-views */
   set_foreach(batch->sampler_views, entry) {
      struct pipe_sampler_view *pres = (struct pipe_sampler_view *)entry->key;
      pipe_sampler_view_reference(&pres, NULL);
   }
   _mesa_set_clear(batch->sampler_views, NULL);

   set_foreach(batch->surfaces, entry) {
      struct pipe_surface *surf = (struct pipe_surface *)entry->key;
      pipe_surface_reference(&surf, NULL);
   }
   _mesa_set_clear(batch->surfaces, NULL);

   util_dynarray_foreach(&batch->zombie_samplers, VkSampler, samp) {
      vkDestroySampler(screen->dev, *samp, NULL);
   }
   util_dynarray_clear(&batch->zombie_samplers);
   util_dynarray_clear(&batch->persistent_resources);

   set_foreach(batch->programs, entry) {
      if (batch->batch_id == ZINK_COMPUTE_BATCH_ID) {
         struct zink_compute_program *comp = (struct zink_compute_program*)entry->key;
         zink_compute_program_reference(screen, &comp, NULL);
      } else {
         struct zink_gfx_program *prog = (struct zink_gfx_program*)entry->key;
         zink_gfx_program_reference(screen, &prog, NULL);
      }
   }
   _mesa_set_clear(batch->programs, NULL);

   if (vkResetDescriptorPool(screen->dev, batch->descpool, 0) != VK_SUCCESS)
      fprintf(stderr, "vkResetDescriptorPool failed\n");

   if (vkResetCommandPool(screen->dev, batch->cmdpool, 0) != VK_SUCCESS)
      fprintf(stderr, "vkResetCommandPool failed\n");
   batch->submitted = batch->has_work = false;
}

void
zink_start_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   zink_reset_batch(ctx, batch);

   VkCommandBufferBeginInfo cbbi = {};
   cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (vkBeginCommandBuffer(batch->cmdbuf, &cbbi) != VK_SUCCESS)
      debug_printf("vkBeginCommandBuffer failed\n");

   if (!ctx->queries_disabled)
      zink_resume_queries(ctx, batch);
}

void
zink_end_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   if (!ctx->queries_disabled)
      zink_suspend_queries(ctx, batch);

   if (vkEndCommandBuffer(batch->cmdbuf) != VK_SUCCESS) {
      debug_printf("vkEndCommandBuffer failed\n");
      return;
   }

   vkResetFences(zink_screen(ctx->base.screen)->dev, 1, &batch->fence->fence);
   zink_fence_init(batch->fence, batch);

   util_dynarray_foreach(&batch->persistent_resources, struct zink_resource*, res) {
       struct zink_screen *screen = zink_screen(ctx->base.screen);
       assert(!(*res)->offset);
       VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          NULL,
          (*res)->mem,
          (*res)->offset,
          VK_WHOLE_SIZE,
       };
       vkFlushMappedMemoryRanges(screen->dev, 1, &range);
   }

   VkSubmitInfo si = {};
   si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.waitSemaphoreCount = 0;
   si.pWaitSemaphores = NULL;
   si.signalSemaphoreCount = 0;
   si.pSignalSemaphores = NULL;
   si.pWaitDstStageMask = NULL;
   si.commandBufferCount = 1;
   si.pCommandBuffers = &batch->cmdbuf;

   if (vkQueueSubmit(ctx->queue, 1, &si, batch->fence->fence) != VK_SUCCESS) {
      debug_printf("ZINK: vkQueueSubmit() failed\n");
      ctx->is_device_lost = true;

      if (ctx->reset.reset) {
         ctx->reset.reset(ctx->reset.data, PIPE_GUILTY_CONTEXT_RESET);
      }
   }
   batch->submitted = true;
}

/* returns either the compute batch id or 0 (gfx batch id) based on whether a resource
   has usage on a different queue than 'batch' belongs to
 */
int
zink_batch_reference_resource_rw(struct zink_batch *batch, struct zink_resource *res, bool write)
{
   unsigned mask = write ? ZINK_RESOURCE_ACCESS_WRITE : ZINK_RESOURCE_ACCESS_READ;
   int batch_to_flush = -1;

   /* u_transfer_helper unrefs the stencil buffer when the depth buffer is unrefed,
    * so we add an extra ref here to the stencil buffer to compensate
    */
   struct zink_resource *stencil;

   zink_get_depth_stencil_resources((struct pipe_resource*)res, NULL, &stencil);

   uint32_t cur_uses = zink_get_resource_usage(res);
   cur_uses &= ~(ZINK_RESOURCE_ACCESS_READ << batch->batch_id);
   cur_uses &= ~(ZINK_RESOURCE_ACCESS_WRITE << batch->batch_id);
   if (batch->batch_id == ZINK_COMPUTE_BATCH_ID) {
      if (cur_uses >= ZINK_RESOURCE_ACCESS_WRITE || (write && cur_uses))
         batch_to_flush = 0;
   } else {
      if (cur_uses & (ZINK_RESOURCE_ACCESS_WRITE << ZINK_COMPUTE_BATCH_ID) ||
          (write && cur_uses & (ZINK_RESOURCE_ACCESS_READ << ZINK_COMPUTE_BATCH_ID)))
         batch_to_flush = ZINK_COMPUTE_BATCH_ID;
   }

   struct set_entry *entry = _mesa_set_search(batch->resources, res);
   if (!entry) {
      entry = _mesa_set_add(batch->resources, res);
      pipe_reference(NULL, &res->base.reference);
      if (stencil)
         pipe_reference(NULL, &stencil->base.reference);
   }
   /* multiple array entries are fine */
   if (res->persistent_maps)
      util_dynarray_append(&batch->persistent_resources, struct zink_resource*, res);
   /* the batch_uses value for this batch is guaranteed to not be in use now because
    * zink_reset_batch() waits on the fence and removes access before resetting
    */
   res->batch_uses[batch->batch_id] |= mask;

   if (stencil)
      stencil->batch_uses[batch->batch_id] |= mask;

   batch->has_work = true;
   return batch_to_flush;
}

void
zink_batch_reference_sampler_view(struct zink_batch *batch,
                                  struct zink_sampler_view *sv)
{
   struct set_entry *entry = _mesa_set_search(batch->sampler_views, sv);
   if (!entry) {
      entry = _mesa_set_add(batch->sampler_views, sv);
      pipe_reference(NULL, &sv->base.reference);
   }
   batch->has_work = true;
}

void
zink_batch_reference_program(struct zink_batch *batch,
                             struct zink_program *pg)
{
   struct set_entry *entry = _mesa_set_search(batch->programs, pg);
   if (!entry) {
      entry = _mesa_set_add(batch->programs, pg);
      pipe_reference(NULL, &pg->reference);
   }
   batch->has_work = true;
}

void
zink_batch_reference_surface(struct zink_batch *batch,
                             struct zink_surface *surface)
{
   struct pipe_surface *surf = &surface->base;
   struct set_entry *entry = _mesa_set_search(batch->surfaces, surf);
   if (!entry) {
      entry = _mesa_set_add(batch->surfaces, surf);
      pipe_reference(NULL, &surf->reference);
   }
   batch->has_work = true;
}
