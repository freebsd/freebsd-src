/* sleep.c
   Sleep for a number of seconds.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

void
usysdep_sleep (c)
     int c;
{
#if HAVE_NAPMS || HAVE_NAP || HAVE_USLEEP || HAVE_SELECT || HAVE_POLL
  int i;

  /* In this case, usysdep_pause is accurate.  */
  for (i = 2 * c; i > 0; i--)
    usysdep_pause ();
#else
  /* On some system sleep (1) may not sleep at all.  Avoid this sort
     of problem by always doing at least sleep (2).  */
  if (c < 2)
    c = 2;
  (void) sleep (c);
#endif
}
