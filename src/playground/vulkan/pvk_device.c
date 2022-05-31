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

static const struct debug_control pvk_debug_options[] = {
    {"output", PVK_DEBUG_OUTPUT}, {NULL, 0}};

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

  instance->debug_flags =
      parse_debug_string(getenv("PVK_DEBUG"), pvk_debug_options);

  if (instance->debug_flags & PVK_DEBUG_OUTPUT) {
    pvk_log("Initialize instance with {vk_instance_init}");
  }

  instance->physical_devices_enumerated = false;
  list_inithead(&instance->physical_devices);

  VG(VALGRIND_CREATE_MEMPOOL(instance, 0, false));

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

  list_for_each_entry_safe(struct pvk_physical_device, pdevice,
                           &instance->physical_devices, link) {

    if (pdevice->local_fd != -1) {
      close(pdevice->local_fd);
    }

    if (pdevice->master_fd != -1) {
      close(pdevice->master_fd);
    }

    vk_physical_device_finish(&pdevice->vk);
    vk_free(&pdevice->instance->vk.alloc, pdevice);
  }

  VG(VALGRIND_DESTROY_MEMPOOL(instance));

  vk_instance_finish(&instance->vk);
  vk_free(&instance->vk.alloc, instance);
}

// =========================================================================
static VkResult
pvk_physical_device_try_create(struct pvk_instance *instance,
                               drmDevicePtr drm_device,
                               struct pvk_physical_device **device_out) {

  VkResult result;
  int fd = -1;
  int master_fd = -1;

  char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

  if (drm_device) {
    const char *path = drm_device->nodes[DRM_NODE_RENDER];
    drmVersionPtr version;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                       "Could not open device %s: %m", path);
    }

    version = drmGetVersion(fd);
    if (!version) {
      close(fd);

      return vk_errorf(
          instance, VK_ERROR_INCOMPATIBLE_DRIVER,
          "Could not get the kernel driver version for device %s: %m", path);
    }

    if (strcmp(version->name, "amdgpu")) {
      drmFreeVersion(version);
      close(fd);

      return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                       "Device '%s' is not using the AMDGPU kernel driver: %m",
                       path);
    }

    strcpy(name, version->name);

    if (instance->debug_flags & PVK_DEBUG_OUTPUT) {
      pvk_log("Name: {%s} Date: {%s} Description: {%s}", version->name,
              version->date, version->desc);

      pvk_log("Found compatible drm device '%s'.", path);
    }

    drmFreeVersion(version);
  }

  struct pvk_physical_device *device =
      vk_zalloc2(&instance->vk.alloc, NULL, sizeof(*device), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

  if (!device) {
    result = vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
    goto fail_fd;
  }

  struct vk_physical_device_dispatch_table dispatch_table;
  vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &pvk_physical_device_entrypoints, true);

  result = vk_physical_device_init(&device->vk, &instance->vk, NULL,
                                   &dispatch_table);
  if (result != VK_SUCCESS) {
    goto fail_alloc;
  }

  if (instance->debug_flags & PVK_DEBUG_OUTPUT) {
    pvk_log("Initialize physical device with {vk_physical_device_init}");
  }

  strcpy(device->name, name);

  device->master_fd = master_fd;
  device->local_fd = fd;

  device->instance = instance;

  *device_out = device;

  return VK_SUCCESS;

fail_alloc:
  vk_free(&instance->vk.alloc, device);
fail_fd:
  if (fd != -1) {
    close(fd);
  }
  if (master_fd != -1) {
    close(master_fd);
  }
  return result;
}

// =========================================================================
static VkResult pvk_enumerate_physical_devices(struct pvk_instance *instance) {

  if (instance->physical_devices_enumerated) {
    return VK_SUCCESS;
  }

  instance->physical_devices_enumerated = true;

  VkResult result = VK_SUCCESS;

  drmDevicePtr devices[8];
  int max_devices = drmGetDevices2(0, devices, ARRAY_SIZE(devices));

  if (instance->debug_flags & PVK_DEBUG_OUTPUT) {
    pvk_log("Found %d drm nodes", max_devices);
  }

  if (max_devices < 1)
    return vk_error(instance, VK_SUCCESS);

  for (unsigned i = 0; i < (unsigned)max_devices; i++) {
    if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
        devices[i]->bustype == DRM_BUS_PCI) {

      struct pvk_physical_device *pdevice = NULL;
      result = pvk_physical_device_try_create(instance, devices[i], &pdevice);
      if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        result = VK_SUCCESS;
        continue;
      }

      if (result != VK_SUCCESS)
        break;

      list_addtail(&pdevice->link, &instance->physical_devices);
    }
  }
  drmFreeDevices(devices, max_devices);

  return result;
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumeratePhysicalDevices(
    VkInstance _instance, uint32_t *pPhysicalDeviceCount,
    VkPhysicalDevice *pPhysicalDevices) {

  VK_FROM_HANDLE(pvk_instance, instance, _instance);

  VK_OUTARRAY_MAKE_TYPED(VkPhysicalDevice, out, pPhysicalDevices,
                         pPhysicalDeviceCount);

  VkResult result = pvk_enumerate_physical_devices(instance);
  if (result != VK_SUCCESS) {
    return result;
  }

  list_for_each_entry(struct pvk_physical_device, pdevice,
                      &instance->physical_devices, link) {

    vk_outarray_append_typed(VkPhysicalDevice, &out, i) {
      *i = pvk_physical_device_to_handle(pdevice);
    }
  }

  return vk_outarray_status(&out);
}

// =========================================================================
VKAPI_ATTR void VKAPI_CALL pvk_GetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties) {

  VK_FROM_HANDLE(pvk_physical_device, pdevice, physicalDevice);

  *pProperties = (VkPhysicalDeviceProperties){
      .apiVersion = PVK_API_VERSION,
      .driverVersion = vk_get_driver_version(),
      .vendorID = 0,
      .deviceID = 0,
      .deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
      .limits = {},
      .sparseProperties = {}};

  strcpy(pProperties->deviceName, pdevice->name);
}

// =========================================================================
VKAPI_ATTR void VKAPI_CALL pvk_GetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties *pMemoryProperties) {}

// =========================================================================
VKAPI_ATTR void VKAPI_CALL
pvk_GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
    uint32_t *pCount, VkQueueFamilyProperties *pQueueFamilyProperties) {}

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

  *pApiVersion = PVK_API_VERSION;

  return VK_SUCCESS;
}

// =========================================================================
VKAPI_ATTR VkResult VKAPI_CALL pvk_EnumerateInstanceLayerProperties(
    uint32_t *pPropertyCount, VkLayerProperties *pProperties) {

  if (pProperties == NULL) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
  }

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
