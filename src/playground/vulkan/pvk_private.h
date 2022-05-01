#ifndef PVK_PRIVATE_H
#define PVK_PRIVATE_H

#include "pvk_entrypoints.h"

#include "vulkan/runtime/vk_instance.h"
#include "vulkan/runtime/vk_physical_device.h"
#include "vulkan/util/vk_alloc.h"
#include "vulkan/util/vk_util.h"

struct pvk_instance {
  struct vk_instance vk;

  bool physical_devices_enumerated;
  struct list_head physical_devices;
};

VK_DEFINE_HANDLE_CASTS(pvk_instance, vk.base, VkInstance,
                       VK_OBJECT_TYPE_INSTANCE)

#endif /* PVK_PRIVATE_H */
