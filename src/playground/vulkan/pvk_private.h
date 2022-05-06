#ifndef PVK_PRIVATE_H
#define PVK_PRIVATE_H

#include "pvk_entrypoints.h"

#include "vulkan/runtime/vk_instance.h"
#include "vulkan/runtime/vk_physical_device.h"
#include "vulkan/util/vk_alloc.h"
#include "vulkan/util/vk_util.h"

#include "util/debug.h"

#include <xf86drm.h>

#include <fcntl.h>
#include <memcheck.h>
#include <valgrind.h>

#define VG(x) x

#define PVK_API_VERSION VK_MAKE_VERSION(1, 3, VK_HEADER_VERSION)

struct pvk_physical_device {
  struct vk_physical_device vk;

  struct list_head link;

  struct pvk_instance *instance;

  char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

  int local_fd;
  int master_fd;
};

struct pvk_instance {
  struct vk_instance vk;

  uint64_t debug_flags;

  bool physical_devices_enumerated;
  struct list_head physical_devices;
};

VK_DEFINE_HANDLE_CASTS(pvk_instance, vk.base, VkInstance,
                       VK_OBJECT_TYPE_INSTANCE)

VK_DEFINE_HANDLE_CASTS(pvk_physical_device, vk.base, VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

#endif /* PVK_PRIVATE_H */
