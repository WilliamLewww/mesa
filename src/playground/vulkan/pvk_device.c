#include "pvk_private.h"

static const struct vk_instance_extension_table
    pvk_instance_extensions_supported = {
        .KHR_device_group_creation = true,
        .KHR_external_fence_capabilities = true,
        .KHR_external_memory_capabilities = true,
        .KHR_external_semaphore_capabilities = true,
        .KHR_get_physical_device_properties2 = true,
        .EXT_debug_report = true,
        .EXT_debug_utils = true,
};

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_CreateInstance(
    const VkInstanceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) {

  struct pvk_instance *instance;
  VkResult result;

  if (pAllocator == NULL) {
    pAllocator = vk_default_allocator();
  }

  instance = vk_alloc(pAllocator, sizeof(*instance), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

  if (!instance) {
    return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);
  }

  struct vk_instance_dispatch_table dispatch_table;

  vk_instance_dispatch_table_from_entrypoints(&dispatch_table,
                                              &pvk_instance_entrypoints, true);

  result = vk_instance_init(&instance->vk, &pvk_instance_extensions_supported,
                            &dispatch_table, pCreateInfo, pAllocator);

  if (result != VK_SUCCESS) {
    vk_free(pAllocator, instance);
    return result;
  }

  /* Initialize driver-specific stuff */

  *pInstance = pvk_instance_to_handle(instance);

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR void VKAPI_CALL pvk_DestroyInstance(
    VkInstance _instance, const VkAllocationCallbacks *pAllocator) {

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  if (!instance) {
    return;
  }

  vk_instance_finish(&instance->vk);
  vk_free(&instance->vk.alloc, instance);
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumeratePhysicalDevices(
    VkInstance _instance, uint32_t *pPhysicalDeviceCount,
    VkPhysicalDevice *pPhysicalDevices) {

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
pvk_GetInstanceProcAddr(VkInstance _instance, const char *pName) {

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  /* The Vulkan 1.0 spec for vkGetInstanceProcAddr has a table of exactly
   * when we have to return valid function pointers, NULL, or it's left
   * undefined.  See the table for exact details.
   */
  if (pName == NULL) {
    return NULL;
  }

#define LOOKUP_PVK_ENTRYPOINT(entrypoint)                                      \
  if (strcmp(pName, "vk" #entrypoint) == 0)                                    \
  return (PFN_vkVoidFunction)pvk_##entrypoint

  LOOKUP_PVK_ENTRYPOINT(EnumerateInstanceExtensionProperties);
  LOOKUP_PVK_ENTRYPOINT(EnumerateInstanceLayerProperties);
  LOOKUP_PVK_ENTRYPOINT(EnumerateInstanceVersion);
  LOOKUP_PVK_ENTRYPOINT(CreateInstance);

  /* GetInstanceProcAddr() can also be called with a NULL instance.
   * See https://gitlab.khronos.org/vulkan/vulkan/issues/2057
   */
  LOOKUP_PVK_ENTRYPOINT(GetInstanceProcAddr);

#undef LOOKUP_PVK_ENTRYPOINT

  if (instance == NULL) {
    return NULL;
  }

  return vk_instance_get_proc_addr(&instance->vk, &pvk_instance_entrypoints,
                                   pName);
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL
pvk_EnumerateInstanceVersion(uint32_t *pApiVersion) {

  *pApiVersion = VK_MAKE_VERSION(1, 3, VK_HEADER_VERSION);

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumerateInstanceLayerProperties(
    uint32_t *pPropertyCount, VkLayerProperties *pProperties) {

  if (pProperties == NULL) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
  }

  /* None supported at this time */
  return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumerateInstanceExtensionProperties(
    const char *pLayerName, uint32_t *pPropertyCount,
    VkExtensionProperties *pProperties) {

  if (pLayerName) {
    return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);
  }

  return vk_enumerate_instance_extension_properties(
      &pvk_instance_extensions_supported, pPropertyCount, pProperties);
}

// =========================================================================
PUBLIC VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName) {

  return pvk_GetInstanceProcAddr(instance, pName);
}

// =========================================================================
PUBLIC
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetPhysicalDeviceProcAddr(VkInstance _instance, const char *pName) {

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  return vk_instance_get_physical_device_proc_addr(&instance->vk, pName);
}
