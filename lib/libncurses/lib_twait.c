/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for	*
*  details. If they are missing then this copy is in violation of       *
*  the copyright conditions.	                                        */

/*
**	lib_twait.c
**
**	The routine timed_wait().
**
*/

#include <string.h>
#include <sys/types.h>		/* some systems can't live without this */
#include <sys/time.h>
#include <unistd.h>
#if defined(SYS_SELECT)
#include <sys/select.h>
#endif
#include "curses.priv.h"

#if defined(NOUSLEEP)
void usleep(unsigned int usec)
{
struct timeval tval;

	tval.tv_sec = usec / 1000000;
	tval.tv_usec = usec % 1000000;
	select(0, NULL, NULL, NULL, &tval);

}
#endif

int timed_wait(int fd, int wait, int *timeleft)
{
int result;
struct timeval timeout;
static fd_set set;
#ifndef GOOD_SELECT
struct timeval starttime, returntime;

	 gettimeofday(&starttime, NULL);
#endif

	 FD_ZERO(&set);
	 FD_SET(fd, &set);

	 /* the units of wait are milliseconds */
	 timeout.tv_sec = wait / 1000;
	 timeout.tv_usec = (wait % 1000) * 1000;

	 T(("start twait: sec = %d, usec = %d", timeout.tv_sec, timeout.tv_usec));

	 result = select(fd+1, &set, NULL, NULL, &timeout);

#ifndef GOOD_SELECT
	 gettimeofday(&returntime, NULL);
	 timeout.tv_sec -= (returntime.tv_sec - starttime.tv_sec);
	 timeout.tv_usec -= (returntime.tv_usec - starttime.tv_usec);
	 if (timeout.tv_usec < 0 && timeout.tv_sec > 0) {
		timeout.tv_sec--;
		timeout.tv_usec += 1000000;
	 }
	 if (timeout.tv_sec < 0)
		timeout.tv_sec = timeout.tv_usec = 0;
#endif

	 /* return approximate time left on the timeout, in milliseconds */
	 if (timeleft)
		*timeleft = (timeout.tv_sec * 1000) + (timeout.tv_usec / 1000);

	 T(("end twait: returned %d, sec = %d, usec = %d (%d msec)",
		 result, timeout.tv_sec, timeout.tv_usec, *timeleft));

	 return(result);
}
