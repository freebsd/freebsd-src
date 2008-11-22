/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* This file simply performs the include magic necessary for using time
 * functions
 */

#ifdef vms
# include <time.h>
#else /* vms */

# if TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
# else /* TIME_WITH_SYS_TIME */
#   if HAVE_SYS_TIME_H
#     include <sys/time.h>
#   else /* HAVE_SYS_TIME_H */
#     include <time.h>
#   endif /* !HAVE_SYS_TIME_H */
# endif /* !TIME_WITH_SYS_TIME */

# ifdef HAVE_SYS_TIMEB_H
#   include <sys/timeb.h>
# else /* HAVE_SYS_TIMEB_H */
/*
 * We use the obsolete `struct timeb' as part of our interface!
 * Since the system doesn't have it, we define it here;
 * our callers must do likewise.
 *
 * At the least we were using this in lib/getdate.y, but lib/system.h used to
 * define it too, so maybe CVS is using it elsewhere.
 */
struct timeb {
    time_t		time;		/* Seconds since the epoch	*/
    unsigned short	millitm;	/* Field not used		*/
    short		timezone;	/* Minutes west of GMT		*/
    short		dstflag;	/* Field not used		*/
};
# endif /* !HAVE_SYS_TIMEB_H */

# ifdef timezone
#   undef timezone /* needed for sgi */
# endif /* timezone */

# if !defined(HAVE_FTIME) && !defined(HAVE_TIMEZONE)
extern long timezone;
# endif /* !defined(HAVE_FTIME) && !defined(HAVE_TIMEZONE) */

#endif /* !vms */
