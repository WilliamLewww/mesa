#ifndef PVK_DEBUG_H
#define PVK_DEBUG_H

#include "vulkan/runtime/vk_log.h"

#define PVK_LOGI(message, result)                                              \
  pvk_log(PVK_LOG_SEVERITY_INFO, message, result, NULL);

#define PVK_LOGIA(message, result, extraMessageArray)                          \
  pvk_log(PVK_LOG_SEVERITY_INFO, message, result, extraMessageArray);

#define PVK_LOGW(message, result)                                              \
  pvk_log(PVK_LOG_SEVERITY_WARNING, message, result, NULL);

#define PVK_LOGWA(message, result, extraMessageArray)                          \
  pvk_log(PVK_LOG_SEVERITY_WARNING, message, result, extraMessageArray);

#define PVK_LOGE(message, result)                                              \
  pvk_log(PVK_LOG_SEVERITY_ERROR, message, result, NULL);

#define PVK_LOGEA(message, result, extraMessageArray)                          \
  pvk_log(PVK_LOG_SEVERITY_ERROR, message, result, extraMessageArray);

enum pvk_log_severity {
  PVK_LOG_SEVERITY_INFO,
  PVK_LOG_SEVERITY_WARNING,
  PVK_LOG_SEVERITY_ERROR
};

void pvk_log(enum pvk_log_severity severity, const char *message,
             VkResult result, const char *extraMessageArray[]);

#endif /* PVK_DEBUG_H */
