/* bzero.c
   Zero out a buffer.  */

#include "uucp.h"

void
bzero (parg, c)
     pointer parg;
     int c;
{
  char *p = (char *) parg;

  while (c-- != 0)
    *p++ = 0;
}
