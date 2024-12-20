#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "asprintf.h"

int asprintf(char **restrict strp, char const *restrict fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int res = vasprintf(strp, fmt, ap);
  va_end(ap);
  return res;
}

int vasprintf(char **restrict strp, char const *restrict fmt, va_list ap)
{
  va_list ap_copy;
  va_copy(ap_copy, ap);
  int sz = vsnprintf(0, 0, fmt, ap_copy);
  va_end(ap_copy);

  void *tmp = malloc(sz + 1);
  if (!tmp) return -1;
  *strp = tmp;

  return vsprintf(*strp, fmt, ap);
}
