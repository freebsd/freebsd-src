/* loctim.c
   Turn a time epoch into a struct tm.  This is trivial on Unix.  */

#include "uucp.h"

#if TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "system.h"

#ifndef localtime
extern struct tm *localtime ();
#endif

void
usysdep_localtime (itime, q)
     long itime;
     struct tm *q;
{
  time_t i;

  i = (time_t) itime;
  *q = *localtime (&i);
}
