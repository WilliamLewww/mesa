#include "pvk_debug.h"

void pvk_printflike(1, 2) pvk_log(const char *format, ...) {
  va_list va;

  va_start(va, format);
  pvk_log_v(format, va);
  va_end(va);
}

void pvk_log_v(const char *format, va_list va) {
  fprintf(stdout, "[PVK_ICD] ");
  vfprintf(stdout, format, va);
  fprintf(stdout, "\n");
}
