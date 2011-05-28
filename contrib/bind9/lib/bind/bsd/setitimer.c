#ifndef LINT
static const char rcsid[] = "$Id: setitimer.c,v 1.1.352.1 2005-04-27 05:00:44 sra Exp $";
#endif

#include "port_before.h"

#include <sys/time.h>

#include "port_after.h"

/*%
 * Setitimer emulation routine.
 */
#ifndef NEED_SETITIMER
int __bindcompat_setitimer;
#else

int
__setitimer(int which, const struct itimerval *value,
	    struct itimerval *ovalue)
{
	if (alarm(value->it_value.tv_sec) >= 0)
		return (0);
	else
		return (-1);
}
#endif

/*! \file */
