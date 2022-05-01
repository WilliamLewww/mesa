#include "pvk_debug.h"
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

  pvk_log("pvk_CreateInstance entry");

  struct pvk_instance *instance;
  VkResult result;

  if (pAllocator == NULL) {
    pAllocator = vk_default_allocator();
  }

  instance = vk_alloc(pAllocator, sizeof(*instance), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

  if (!instance) {
    pvk_log("vk_alloc: Error allocating instance memory");

    return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);
  }

  struct vk_instance_dispatch_table dispatch_table;

  vk_instance_dispatch_table_from_entrypoints(&dispatch_table,
                                              &pvk_instance_entrypoints, true);

  result = vk_instance_init(&instance->vk, &pvk_instance_extensions_supported,
                            &dispatch_table, pCreateInfo, pAllocator);

  pvk_log("pvk_CreateInstance called (return code %d)", result);

  if (result != VK_SUCCESS) {
    pvk_log("pvk_CreateInstance failed (return code %d)", result);
    vk_free(pAllocator, instance);
    return result;
  }

  *pInstance = pvk_instance_to_handle(instance);

  pvk_log("pvk_CreateInstance exit");

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR void VKAPI_CALL pvk_DestroyInstance(
    VkInstance _instance, const VkAllocationCallbacks *pAllocator) {

  pvk_log("pvk_DestroyInstance entry");

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  if (!instance) {
    return;
  }

  vk_instance_finish(&instance->vk);
  vk_free(&instance->vk.alloc, instance);

  pvk_log("pvk_DestroyInstance exit");
}

// =========================================================================
static VkResult
radv_enumerate_physical_devices(struct radv_instance *instance) {
//  if (instance->physical_devices_enumerated)
//    return VK_SUCCESS;
//  
//  instance->physical_devices_enumerated = true;
//  
//  VkResult result = VK_SUCCESS;
//  
//  drmDevicePtr devices[8];
//  int max_devices = drmGetDevices2(0, devices, ARRAY_SIZE(devices));
//  
//  if (instance->debug_flags & RADV_DEBUG_STARTUP)
//    radv_logi("Found %d drm nodes", max_devices);
//  
//  if (max_devices < 1)
//    return vk_error(instance, VK_SUCCESS);
//  
//  for (unsigned i = 0; i < (unsigned)max_devices; i++) {
//    if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
//        devices[i]->bustype == DRM_BUS_PCI &&
//        devices[i]->deviceinfo.pci->vendor_id == ATI_VENDOR_ID) {
//  
//      struct radv_physical_device *pdevice;
//      result = radv_physical_device_try_create(instance, devices[i],
//      &pdevice); if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
//        result = VK_SUCCESS;
//        continue;
//      }
//  
//      if (result != VK_SUCCESS)
//        break;
//  
//      list_addtail(&pdevice->link, &instance->physical_devices);
//    }
//  }
//  drmFreeDevices(devices, max_devices);
//  
//  return result;
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumeratePhysicalDevices(
    VkInstance _instance, uint32_t *pPhysicalDeviceCount,
    VkPhysicalDevice *pPhysicalDevices) {

  pvk_log("pvk_EnumeratePhysicalDevices entry");

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  VK_OUTARRAY_MAKE_TYPED(VkPhysicalDevice, out, pPhysicalDevices,
                         pPhysicalDeviceCount);

  pvk_log("pvk_EnumeratePhysicalDevices exit");

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
pvk_GetInstanceProcAddr(VkInstance _instance, const char *pName) {

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

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

  pvk_log("pvk_EnumerateInstanceVersion entry");

  *pApiVersion = VK_MAKE_VERSION(1, 3, VK_HEADER_VERSION);

  pvk_log("pvk_EnumerateInstanceVersion exit");

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumerateInstanceLayerProperties(
    uint32_t *pPropertyCount, VkLayerProperties *pProperties) {

  pvk_log("pvk_EnumerateInstanceLayerProperties entry");

  if (pProperties == NULL) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
  }

  pvk_log("pvk_EnumerateInstanceLayerProperties exit");

  return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumerateInstanceExtensionProperties(
    const char *pLayerName, uint32_t *pPropertyCount,
    VkExtensionProperties *pProperties) {

  pvk_log("pvk_EnumerateInstanceExtensionProperties entry");

  if (pLayerName) {
    return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);
  }

  pvk_log("pvk_EnumerateInstanceExtensionProperties exit");

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
