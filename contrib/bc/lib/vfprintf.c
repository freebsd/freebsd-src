/* vfprintf.c -- this was provided for minix.  It may not
   work on any other system. */

#include "config.h"
#ifndef HAVE_VPRINTF
#ifndef HAVE_DOPRINT
# error need vfprintf() or doprint()
#else

#ifdef HAVE_LIB_H
#include <lib.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

int vfprintf(file, format, argp)
FILE *file;
_CONST char *format;
va_list argp;
{
  _doprintf(file, format, argp);
  if (testflag(file, PERPRINTF)) fflush(file);
  return 0;
}

#endif /* HAVE_DOPRINT */
#endif /* !HAVE_VFPRINTF */
