#pragma once
#include <stdarg.h>

/** Print to allocated string 
 *  
 *  @param strp[out] pointer to allocated string 
 *  @param fmt[in] format string
 *  @returns number of bytes written, not including null terminator.
 *  @returns -1 on error and sets `errno` (see exceptions)
 *
 *  @exception ENOMEM
 *
 *  See man pages for asprintf(3) and vasprintf(3). Uses malloc() to
 *  assign strp. The returned pointer should be passed to free().
 */
int asprintf(char **restrict strp, char const *restrict fmt, ...);

/** Print to allocated string 
 *
 * @sa asprintf() */
int vasprintf(char **restrict strp, char const *restrict fmt, va_list ap);
