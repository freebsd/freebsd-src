/* time.c
   Get the current time.  */

#include "uucp.h"

#if HAVE_TIME_H
#include <time.h>
#endif

#include "system.h"

#ifndef time
extern time_t time ();
#endif

/* Get the time in seconds since the epoch, with optional
   microseconds.  We use ixsysdep_process_time to get the microseconds
   if it will work (it won't if it uses times, since that returns a
   time based only on the process).  */

long
ixsysdep_time (pimicros)
     long *pimicros;
{
#if HAVE_GETTIMEOFDAY || HAVE_FTIME
  return ixsysdep_process_time (pimicros);
#else
  if (pimicros != NULL)
    *pimicros = 0;
  return (long) time ((time_t *) NULL);
#endif
}
