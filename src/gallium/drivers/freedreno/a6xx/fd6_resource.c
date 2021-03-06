/*
 * Copyright (C) 2018 Rob Clark <robclark@freedesktop.org>
 * Copyright © 2018 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include "drm-uapi/drm_fourcc.h"

#include "fd6_resource.h"
#include "fd6_format.h"

#include "a6xx.xml.h"

/* A subset of the valid tiled formats can be compressed.  We do
 * already require tiled in order to be compressed, but just because
 * it can be tiled doesn't mean it can be compressed.
 */
static bool
ok_ubwc_format(struct pipe_screen *pscreen, enum pipe_format pfmt)
{
	switch (pfmt) {
	case PIPE_FORMAT_X24S8_UINT:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
		/* We can't sample stencil with UBWC on a630, and we may need to be able
		 * to sample stencil at some point.  We can't just use
		 * fd_resource_uncompress() at the point of stencil sampling because
		 * that itself uses stencil sampling in the fd_blitter_blit path.
		 */
		return fd_screen(pscreen)->info.a6xx.has_z24uint_s8uint;

	case PIPE_FORMAT_R8_G8B8_420_UNORM:
		return true;

	default:
		break;
	}

	switch (fd6_pipe2color(pfmt)) {
	case FMT6_10_10_10_2_UINT:
	case FMT6_10_10_10_2_UNORM_DEST:
	case FMT6_11_11_10_FLOAT:
	case FMT6_16_FLOAT:
	case FMT6_16_16_16_16_FLOAT:
	case FMT6_16_16_16_16_SINT:
	case FMT6_16_16_16_16_UINT:
	case FMT6_16_16_FLOAT:
	case FMT6_16_16_SINT:
	case FMT6_16_16_UINT:
	case FMT6_16_SINT:
	case FMT6_16_UINT:
	case FMT6_32_32_32_32_SINT:
	case FMT6_32_32_32_32_UINT:
	case FMT6_32_32_SINT:
	case FMT6_32_32_UINT:
	case FMT6_5_6_5_UNORM:
	case FMT6_8_8_8_8_SINT:
	case FMT6_8_8_8_8_UINT:
	case FMT6_8_8_8_8_UNORM:
	case FMT6_8_8_8_X8_UNORM:
	case FMT6_8_8_SINT:
	case FMT6_8_8_UINT:
	case FMT6_8_8_UNORM:
	case FMT6_8_UNORM:
	case FMT6_Z24_UNORM_S8_UINT:
	case FMT6_Z24_UNORM_S8_UINT_AS_R8G8B8A8:
		return true;
	default:
		return false;
	}
}

static bool
can_do_ubwc(struct pipe_resource *prsc)
{
	/* limit things to simple single level 2d for now: */
	if ((prsc->depth0 != 1) || (prsc->array_size != 1) || (prsc->last_level != 0))
		return false;
	if (prsc->target != PIPE_TEXTURE_2D)
		return false;
	if (!ok_ubwc_format(prsc->screen, prsc->format))
		return false;
	return true;
}

/**
 * Ensure the rsc is in an ok state to be used with the specified format.
 * This handles the case of UBWC buffers used with non-UBWC compatible
 * formats, by triggering an uncompress.
 */
void
fd6_validate_format(struct fd_context *ctx, struct fd_resource *rsc,
		enum pipe_format format)
	in_dt  /* TODO this will be re-worked with threaded-ctx, this is just temporary */
{
	if (!rsc->layout.ubwc)
		return;

	if (ok_ubwc_format(rsc->base.screen, format))
		return;

	perf_debug_ctx(ctx, "%"PRSC_FMT": demoted to uncompressed due to use as %s",
		PRSC_ARGS(&rsc->base), util_format_short_name(format));

	fd_resource_uncompress(ctx, rsc);
}

static void
setup_lrz(struct fd_resource *rsc)
{
	struct fd_screen *screen = fd_screen(rsc->base.screen);
	const uint32_t flags = DRM_FREEDRENO_GEM_CACHE_WCOMBINE |
			DRM_FREEDRENO_GEM_TYPE_KMEM; /* TODO */
	unsigned width0 = rsc->base.width0;
	unsigned height0 = rsc->base.height0;

	/* LRZ buffer is super-sampled: */
	switch (rsc->base.nr_samples) {
	case 4:
		width0 *= 2;
		FALLTHROUGH;
	case 2:
		height0 *= 2;
	}

	unsigned lrz_pitch  = align(DIV_ROUND_UP(width0, 8), 32);
	unsigned lrz_height = align(DIV_ROUND_UP(height0, 8), 16);

	unsigned size = lrz_pitch * lrz_height * 2;

	rsc->lrz_height = lrz_height;
	rsc->lrz_width = lrz_pitch;
	rsc->lrz_pitch = lrz_pitch;
	rsc->lrz = fd_bo_new(screen->dev, size, flags, "lrz");
}

static uint32_t
fd6_setup_slices(struct fd_resource *rsc)
{
	struct pipe_resource *prsc = &rsc->base;

	if (!FD_DBG(NOLRZ) && has_depth(rsc->base.format))
		setup_lrz(rsc);

	if (rsc->layout.ubwc && !ok_ubwc_format(rsc->base.screen, rsc->base.format))
		rsc->layout.ubwc = false;

	fdl6_layout(&rsc->layout, prsc->format, fd_resource_nr_samples(prsc),
			prsc->width0, prsc->height0, prsc->depth0,
			prsc->last_level + 1, prsc->array_size,
			prsc->target == PIPE_TEXTURE_3D,
			NULL);

	return rsc->layout.size;
}

static int
fill_ubwc_buffer_sizes(struct fd_resource *rsc)
{
	struct pipe_resource *prsc = &rsc->base;
	struct fdl_explicit_layout explicit = {
		.offset = rsc->layout.slices[0].offset,
		.pitch = rsc->layout.pitch0,
	};

	if (!can_do_ubwc(prsc))
		return -1;

	rsc->layout.ubwc = true;
	rsc->layout.tile_mode = TILE6_3;

	if (!fdl6_layout(&rsc->layout, prsc->format, fd_resource_nr_samples(prsc),
			prsc->width0, prsc->height0, prsc->depth0,
			prsc->last_level + 1, prsc->array_size, false, &explicit))
		return -1;

	if (rsc->layout.size > fd_bo_size(rsc->bo))
		return -1;

	return 0;
}

static int
fd6_layout_resource_for_modifier(struct fd_resource *rsc, uint64_t modifier)
{
	switch (modifier) {
	case DRM_FORMAT_MOD_QCOM_COMPRESSED:
		return fill_ubwc_buffer_sizes(rsc);
	case DRM_FORMAT_MOD_LINEAR:
		if (can_do_ubwc(&rsc->base)) {
			perf_debug("%"PRSC_FMT": not UBWC: imported with DRM_FORMAT_MOD_LINEAR!",
					PRSC_ARGS(&rsc->base));
		}
		return 0;
	case DRM_FORMAT_MOD_INVALID:
		if (can_do_ubwc(&rsc->base)) {
			perf_debug("%"PRSC_FMT": not UBWC: imported with DRM_FORMAT_MOD_INVALID!",
					PRSC_ARGS(&rsc->base));
		}
		return 0;
	default:
		return -1;
	}
}

static const uint64_t supported_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_QCOM_COMPRESSED,
};

void
fd6_resource_screen_init(struct pipe_screen *pscreen)
{
	struct fd_screen *screen = fd_screen(pscreen);

	screen->setup_slices = fd6_setup_slices;
	screen->layout_resource_for_modifier = fd6_layout_resource_for_modifier;
	screen->supported_modifiers = supported_modifiers;
	screen->num_supported_modifiers = ARRAY_SIZE(supported_modifiers);
}
