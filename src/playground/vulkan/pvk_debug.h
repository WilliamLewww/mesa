#ifndef PVK_DEBUG_H
#define PVK_DEBUG_H

#include "vulkan/runtime/vk_log.h"

#define pvk_printflike(a, b) __attribute__((__format__(__printf__, a, b)))

enum {
  PVK_DEBUG_OUTPUT = 1ull << 0,
};

void pvk_log(const char *format, ...) pvk_printflike(1, 2);
void pvk_log_v(const char *format, va_list va);

#endif /* PVK_DEBUG_H */
