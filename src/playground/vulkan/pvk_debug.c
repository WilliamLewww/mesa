#include "pvk_debug.h"

void pvk_log(enum pvk_log_severity severity, const char *message,
             VkResult result, const char *extraMessageArray[]) {

  const char *severityString = "";

  if (severity == PVK_LOG_SEVERITY_INFO) {
    severityString = "Info";
  } else if (severity == PVK_LOG_SEVERITY_WARNING) {
    severityString = "Warning";
  } else if (severity == PVK_LOG_SEVERITY_ERROR) {
    severityString = "Error";
  }

  if (extraMessageArray == NULL) {
    printf("[PVK_ICD %s] %s (return code %d)\n", severityString, message,
           result);
  } else {
    printf("[PVK_ICD %s] %s (return code %d) {", severityString, message,
           result);

    uint32_t extraMessageIndex = 0;
    while (strlen(extraMessageArray[extraMessageIndex + 1]) > 0) {
      printf("%s, ", extraMessageArray[extraMessageIndex]);
      extraMessageIndex += 1;
    }
    printf("%s}\n", extraMessageArray[extraMessageIndex]);
  }
}
