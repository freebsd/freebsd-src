/* sleep.c
   Sleep for a number of seconds.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

void
usysdep_sleep (c)
     int c;
{
  (void) sleep (c);
}
