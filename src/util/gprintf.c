#include <stdio.h>
#include <errno.h>
#include "gprintf.h"

void(vgprintf)(char const *prefix, char const *fmt, va_list ap)
{
  int e = errno;
  fputs(prefix, stderr);
  vfprintf(stderr, fmt, ap);
  putc('\n', stderr);
  errno = e;
}

void(gprintf)(char const *prefix, char const *fmt, ...)
{
  int e = errno;
  va_list ap;
  va_start(ap, fmt);
  (vgprintf)(prefix, fmt, ap);
  va_end(ap);
  errno = e;
}
