/* memcpy.c
   Copy one memory buffer to another.  */

#include "uucp.h"

pointer
memcpy (ptoarg, pfromarg, c)
     pointer ptoarg;
     constpointer pfromarg;
     size_t c;
{
  char *pto = (char *) ptoarg;
  const char *pfrom = (const char *) pfromarg;

  while (c-- != 0)
    *pto++ = *pfrom++;
  return ptoarg;
}
