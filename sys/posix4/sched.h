#ifndef _SCHED_H_
#define _SCHED_H_

/* sched.h: POSIX 1003.1b Process Scheduling header */

/*-
 * Copyright (c) 1996, 1997
 *	HD Associates, Inc.  All rights reserved.
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
 *	This product includes software developed by HD Associates, Inc
 *	and Jukka Antero Ukkonen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>	/* For pid_t */

#ifndef _KERNEL
#include <time.h>		/* Per P1003.4 */
#endif

/* Scheduling policies
 */
#define SCHED_FIFO  1
#define SCHED_OTHER 2
#define SCHED_RR    3

struct sched_param
{
	int sched_priority;
};

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int sched_setparam __P((pid_t, const struct sched_param *));
int sched_getparam __P((pid_t, struct sched_param *));

int sched_setscheduler __P((pid_t, int, const struct sched_param *));
int sched_getscheduler __P((pid_t));

int sched_yield __P((void));
int sched_get_priority_max __P((int));
int sched_get_priority_min __P((int));
int sched_rr_get_interval __P((pid_t, struct timespec *));
__END_DECLS

#endif

#endif /* _SCHED_H_ */
