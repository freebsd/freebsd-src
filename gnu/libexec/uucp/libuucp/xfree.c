/* xfree.c
   Some versions of free (like the one in SCO Unix 3.2.2) don't handle
   null pointers correctly, so we go through our own routine.  */

#include "uucp.h"

#include "uudefs.h"

void
xfree (p)
     pointer p;
{
  if (p != NULL)
    free (p);
}
