/* memcmp.c
   Compare two memory buffers.  */

#include "uucp.h"

int
memcmp (p1arg, p2arg, c)
     constpointer p1arg;
     constpointer p2arg;
     size_t c;
{
  const char *p1 = (const char *) p1arg;
  const char *p2 = (const char *) p2arg;

  while (c-- != 0)
    if (*p1++ != *p2++)
      return BUCHAR (*--p1) - BUCHAR (*--p2);
  return 0;
}
