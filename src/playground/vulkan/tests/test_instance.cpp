#include "vulkan/vulkan.h"

int main(int argc, char **argv) {
  VkInstanceCreateInfo instanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .pApplicationInfo = NULL,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = NULL,
      .enabledExtensionCount = 0,
      .ppEnabledExtensionNames = NULL,
  };

  VkInstance instanceHandle = VK_NULL_HANDLE;
  VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &instanceHandle);

  if (result != VK_SUCCESS) {
    return -1;
  }

  vkDestroyInstance(instanceHandle, NULL);

  return 0;
}
