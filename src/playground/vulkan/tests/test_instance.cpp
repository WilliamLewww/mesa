#include "vulkan/vulkan.h"

#include <iostream>

void throwExceptionVulkanAPI(VkResult result, std::string functionName) {
  std::string message = "Vulkan API exception: return code " +
                        std::to_string(result) + " (" + functionName + ")";

  throw std::runtime_error(message);
}

int main(int argc, char **argv) {
  VkResult result = VK_SUCCESS;

  uint32_t apiVersion = 0;
  result = vkEnumerateInstanceVersion(&apiVersion);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEnumerateInstanceVersion");
  }

  std::cout << "Vulkan ICD: " << VK_API_VERSION_MAJOR(apiVersion) << "."
            << VK_API_VERSION_MINOR(apiVersion) << "."
            << VK_API_VERSION_PATCH(apiVersion) << std::endl;

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
  result = vkCreateInstance(&instanceCreateInfo, NULL, &instanceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateInstance");
  }

  vkDestroyInstance(instanceHandle, NULL);

  return 0;
}
