#include <stdio.h>
#include <varargs.h>
#include <ansidecl.h>
#undef vprintf
int
vprintf (format, ap)
     const char *format;
     va_list ap;
{
  return vfprintf (stdout, format, ap);
}
