/* strchr.c
   Look for a character in a string.  This works for a null byte.  */

#include "uucp.h"

char *
strchr (z, b)
     const char *z;
     int b;
{
  b = (char) b;
  while (*z != b)
    if (*z++ == '\0')
      return NULL;
  return (char *) z;
}
