/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sean Eric Fagan of Cygnus Support.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)sysconf.c	8.2 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/_posix.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <errno.h>
#include <time.h>
#include <unistd.h>

/*
 * sysconf --
 *	get configurable system variables.
 *
 * XXX
 * POSIX 1003.1 (ISO/IEC 9945-1, 4.8.1.3) states that the variable values
 * not change during the lifetime of the calling process.  This would seem
 * to require that any change to system limits kill all running processes.
 * A workaround might be to cache the values when they are first retrieved
 * and then simply return the cached value on subsequent calls.  This is
 * less useful than returning up-to-date values, however.
 */
long
sysconf(name)
	int name;
{
	struct rlimit rl;
	size_t len;
	int mib[2], value;
	long defaultresult;

	len = sizeof(value);
	defaultresult = -1;

	switch (name) {
/* 1003.1 */
	case _SC_ARG_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;
		break;
	case _SC_CHILD_MAX:
		return (getrlimit(RLIMIT_NPROC, &rl) ? -1 : rl.rlim_cur);
	case _SC_CLK_TCK:
		return (CLK_TCK);
	case _SC_JOB_CONTROL:
		mib[0] = CTL_KERN;
		mib[1] = KERN_JOB_CONTROL;
		goto yesno;
	case _SC_NGROUPS_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_NGROUPS;
		break;
	case _SC_OPEN_MAX:
		return (getrlimit(RLIMIT_NOFILE, &rl) ? -1 : rl.rlim_cur);
	case _SC_STREAM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_STREAM_MAX;
		break;
	case _SC_TZNAME_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_TZNAME_MAX;
		break;
	case _SC_SAVED_IDS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SAVED_IDS;
		goto yesno;
	case _SC_VERSION:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX1;
		break;

/* 1003.2 */
	case _SC_BC_BASE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_BASE_MAX;
		break;
	case _SC_BC_DIM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_DIM_MAX;
		break;
	case _SC_BC_SCALE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_SCALE_MAX;
		break;
	case _SC_BC_STRING_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_STRING_MAX;
		break;
	case _SC_COLL_WEIGHTS_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_COLL_WEIGHTS_MAX;
		break;
	case _SC_EXPR_NEST_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_EXPR_NEST_MAX;
		break;
	case _SC_LINE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_LINE_MAX;
		break;
	case _SC_RE_DUP_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_RE_DUP_MAX;
		break;
	case _SC_2_VERSION:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_VERSION;
		break;
	case _SC_2_C_BIND:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_BIND;
		goto yesno;
	case _SC_2_C_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_DEV;
		goto yesno;
	case _SC_2_CHAR_TERM:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_CHAR_TERM;
		goto yesno;
	case _SC_2_FORT_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_DEV;
		goto yesno;
	case _SC_2_FORT_RUN:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_RUN;
		goto yesno;
	case _SC_2_LOCALEDEF:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_LOCALEDEF;
		goto yesno;
	case _SC_2_SW_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_SW_DEV;
		goto yesno;
	case _SC_2_UPE:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_UPE;
		goto yesno;

#ifdef _P1003_1B_VISIBLE
	/* POSIX.1B */

	case _SC_ASYNCHRONOUS_IO:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_ASYNCHRONOUS_IO;
		goto yesno;
	case _SC_MAPPED_FILES:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MAPPED_FILES;
		goto yesno;
	case _SC_MEMLOCK:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MEMLOCK;
		goto yesno;
	case _SC_MEMLOCK_RANGE:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MEMLOCK_RANGE;
		goto yesno;
	case _SC_MEMORY_PROTECTION:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MEMORY_PROTECTION;
		goto yesno;
	case _SC_MESSAGE_PASSING:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MESSAGE_PASSING;
		goto yesno;
	case _SC_PRIORITIZED_IO:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_PRIORITIZED_IO;
		goto yesno;
	case _SC_PRIORITY_SCHEDULING:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_PRIORITY_SCHEDULING;
		goto yesno;
	case _SC_REALTIME_SIGNALS:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_REALTIME_SIGNALS;
		goto yesno;
	case _SC_SEMAPHORES:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SEMAPHORES;
		goto yesno;
	case _SC_FSYNC:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_FSYNC;
		goto yesno;
	case _SC_SHARED_MEMORY_OBJECTS:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SHARED_MEMORY_OBJECTS;
		goto yesno;
	case _SC_SYNCHRONIZED_IO:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SYNCHRONIZED_IO;
		goto yesno;
	case _SC_TIMERS:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_TIMERS;
		goto yesno;
	case _SC_AIO_LISTIO_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_AIO_LISTIO_MAX;
		goto yesno;
	case _SC_AIO_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_AIO_MAX;
		goto yesno;
	case _SC_AIO_PRIO_DELTA_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_AIO_PRIO_DELTA_MAX;
		goto yesno;
	case _SC_DELAYTIMER_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_DELAYTIMER_MAX;
		goto yesno;
	case _SC_MQ_OPEN_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_MQ_OPEN_MAX;
		goto yesno;
	case _SC_PAGESIZE:
		defaultresult = getpagesize();
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_PAGESIZE;
		goto yesno;
	case _SC_RTSIG_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_RTSIG_MAX;
		goto yesno;
	case _SC_SEM_NSEMS_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SEM_NSEMS_MAX;
		goto yesno;
	case _SC_SEM_VALUE_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SEM_VALUE_MAX;
		goto yesno;
	case _SC_SIGQUEUE_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_SIGQUEUE_MAX;
		goto yesno;
	case _SC_TIMER_MAX:
		mib[0] = CTL_P1003_1B;
		mib[1] = CTL_P1003_1B_TIMER_MAX;
		goto yesno;
#endif /* _P1003_1B_VISIBLE */

yesno:		if (sysctl(mib, 2, &value, &len, NULL, 0) == -1)
			return (-1);
		if (value == 0)
			return (defaultresult);
		return (value);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (sysctl(mib, 2, &value, &len, NULL, 0) == -1 ? -1 : value);
}
