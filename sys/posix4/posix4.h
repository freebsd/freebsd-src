#ifndef _POSIX4_POSIX4_H_
#define _POSIX4_POSIX4_H_
/*-
 * Copyright (c) 1996, 1997, 1998
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
 */

#include <sys/_posix.h>

#ifdef _POSIX4_VISIBLE

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sched.h>

/* 
 *
 * March 1, 1998: Details from here on change and this header file
 * is volatile.
 *
 * Locally I've got PRIORITY SCHEDULING
 * set as a system call available only to root 
 * and I'm still using a pseudo device to gate everything else.
 *
 * This interface vectors us into the kernel through a
 * POSIX4 pseudo device with some user privilege authorization along
 * the way.
 *
 * XXX I'm going with option 3.
 *
 * This has drawbacks from the point of view of ktrace.  There
 * are (at least) three ways to do this:
 *
 * 1. As it is being done, which is bad for ktrace and is hokey
 *    but is easy to extend during development;
 * 2. Add a system call for every POSIX4 entry point, which
 *    will result in many more system calls (on the order of 64)
 * 3. Add a system call for each POSIX4 option, which is a bit more
 *    useful for ktrace and will add only about 14 new system calls.
 * 
 */

#define POSIX4_FACILITIES 16
#define POSIX4_ONE_ONLY

/* 
 * All facility request structures have a posix4_dispatch header
 * at the front.  Return values are always an indication of
 * success or failure and are automatically converted into an errno
 * by the kernel.  "Non-errno" return codes are handled via ret.
 */
struct posix4_dispatch {
	int op;
	int ret;
};

#if defined(_POSIX_PRIORITY_SCHEDULING)

/* 
 * KSCHED_OP_RW is a vector of read/write flags for each entry indexed
 * by the enum ksched_op.
 *
 * 1 means you need write access, 0 means read is sufficient.
 */

enum ksched_op {

#define KSCHED_OP_RW { 1, 0, 1, 0, 0, 0, 0, 0 }

	SCHED_SETPARAM,
	SCHED_GETPARAM,
	SCHED_SETSCHEDULER,
	SCHED_GETSCHEDULER,
	SCHED_YIELD,
	SCHED_GET_PRIORITY_MAX,
	SCHED_GET_PRIORITY_MIN,
	SCHED_RR_GET_INTERVAL,
	SCHED_OP_MAX
};

struct ksched
{
	struct posix4_dispatch dispatch;
	pid_t pid;
	int policy;
	struct sched_param param;
	struct timespec interval;
};

#endif /* _POSIX_PRIORITY_SCHEDULING */

#if defined(_POSIX_MEMLOCK) ^ defined(_POSIX_MEMLOCK_RANGE)
/* This implementation expects these two options to always be together.
 * If one isn't handled it should be disabled at
 * run time.
 */
#error _POSIX_MEMLOCK and _POSIX_MEMLOCK_RANGE should always be together
#endif

#if defined(_POSIX_MEMLOCK) && defined(_POSIX_MEMLOCK_RANGE)

enum kmemlock_op {

#define KMEMLOCK_OP_RW { 1, 1, 1, 1 }

	MEMLOCK_MLOCKALL,
	MEMLOCK_MUNLOCKALL,
	MEMLOCK_MLOCK,
	MEMLOCK_MUNLOCK,
	MEMLOCK_OP_MAX
};

struct kmemlock
{
	struct posix4_dispatch dispatch;
	int flags;
	void *addr;
	size_t len;
};

#endif /* _POSIX_MEMLOCK && _POSIX_MEMLOCK_RANGE */

#if defined(KERNEL)

struct proc;

void *posix4malloc __P((int *, size_t));
void posix4free __P((int *, void *));
int posix4proc __P((struct proc *, pid_t, struct proc **));
int posix4ioctl __P((dev_t, int, caddr_t, int, struct proc *));
void posix4attach __P((int));
void posix4_facility __P((int, int));

struct lkm_table;
int posix4_init __P((struct lkm_table *, int , int ));

#ifdef _POSIX_PRIORITY_SCHEDULING

int ksched_attach(int, int, void **);
int ksched_detach(void *);

int ksched_setparam(int *, void *,
	struct proc *, const struct sched_param *);
int ksched_getparam(int *, void *,
	struct proc *, struct sched_param *);

int ksched_setscheduler(int *, void *,
	struct proc *, int, const struct sched_param *);
int ksched_getscheduler(int *, void *, struct proc *);

int ksched_yield(int *, void *);

int ksched_get_priority_max(int *, void *, int);
int ksched_get_priority_min(int *, void *, int);

int ksched_rr_get_interval(int *, void *, struct proc *, struct timespec *);

#endif /* _POSIX_PRIORITY_SCHEDULING */

#if defined(_POSIX_MEMLOCK) && defined(_POSIX_MEMLOCK_RANGE)

int kmemlock_attach(int, int, void **);
int kmemlock_detach(void *);
int kmlockall(int *, void *, int);
int kmunlockall(int *, void *);
int kmlock(int *, void *, const void *, size_t);
int kmunlock(int *, void *, const void *, size_t );

#endif /* _POSIX_MEMLOCK && _POSIX_MEMLOCK_RANGE */

#endif	/* KERNEL */

/* A facility is an implementation of one of the optional portions of
 * POSIX4 as selected by the feature test macros, such as the fixed
 * priority scheduler or the realtime signals.
 */

/* Each facility has a facility code, an opcode, and r-w attributes.
 * To exercise the operation associated with an opcode you need the
 * appropriate privileges on the POSIX4 device with the facility
 * bit set in the minor number.  This means that every facility has
 * a protection bit: Probably more than we need, but it may have
 * advantages.
 *
 */

#define posix4encode(FACILITY, RW) (FACILITY)
#define posix4decode(X, FACILITY_P) \
	do { \
		*(FACILITY_P) = ((X) & 0xff); \
	} while (0)

/*
 * The dispatch codes:
 */
#define IO_POSIX4_PRIORITY_SCHEDULING _IOWR('r', \
	CTL_POSIX4_PRIORITY_SCHEDULING, struct ksched)

#define IO_POSIX4_MEMLOCK _IOWR('r', \
	CTL_POSIX4_MEMLOCK, struct ksched)

/*
 * CTL_POSIX4 definitions for syscfg
 */

#define CTL_POSIX4_ASYNCHRONOUS_IO	1	/* boolean */
#define CTL_POSIX4_MAPPED_FILES		2	/* boolean */
#define CTL_POSIX4_MEMLOCK		3	/* boolean */
#define CTL_POSIX4_MEMLOCK_RANGE	4	/* boolean */
#define CTL_POSIX4_MEMORY_PROTECTION	5	/* boolean */
#define CTL_POSIX4_MESSAGE_PASSING	6	/* boolean */
#define CTL_POSIX4_PRIORITIZED_IO	7	/* boolean */
#define CTL_POSIX4_PRIORITY_SCHEDULING	8	/* boolean */
#define CTL_POSIX4_REALTIME_SIGNALS	9	/* boolean */
#define CTL_POSIX4_SEMAPHORES		10	/* boolean */
#define CTL_POSIX4_FSYNC		11	/* boolean */
#define CTL_POSIX4_SHARED_MEMORY_OBJECTS 12	/* boolean */
#define CTL_POSIX4_SYNCHRONIZED_IO	13	/* boolean */
#define CTL_POSIX4_TIMERS		14	/* boolean */
#define CTL_POSIX4_AIO_LISTIO_MAX	15	/* int */
#define CTL_POSIX4_AIO_MAX		16	/* int */
#define CTL_POSIX4_AIO_PRIO_DELTA_MAX	17	/* int */
#define CTL_POSIX4_DELAYTIMER_MAX	18	/* int */
#define CTL_POSIX4_MQ_OPEN_MAX		19	/* int */
#define CTL_POSIX4_PAGESIZE		20	/* int */
#define CTL_POSIX4_RTSIG_MAX		21	/* int */
#define CTL_POSIX4_SEM_NSEMS_MAX	22	/* int */
#define CTL_POSIX4_SEM_VALUE_MAX	23	/* int */
#define CTL_POSIX4_SIGQUEUE_MAX		24	/* int */
#define CTL_POSIX4_TIMER_MAX		25	/* int */

#define CTL_POSIX4_N_CTLS		25

#define	CTL_POSIX4_NAMES { \
	{ 0, 0 }, \
	{ "asynchronous_io", CTLTYPE_INT }, \
	{ "mapped_files", CTLTYPE_INT }, \
	{ "memlock", CTLTYPE_INT }, \
	{ "memlock_range", CTLTYPE_INT }, \
	{ "memory_protection", CTLTYPE_INT }, \
	{ "message_passing", CTLTYPE_INT }, \
	{ "prioritized_io", CTLTYPE_INT }, \
	{ "priority_scheduling", CTLTYPE_INT }, \
	{ "realtime_signals", CTLTYPE_INT }, \
	{ "semaphores", CTLTYPE_INT }, \
	{ "fsync", CTLTYPE_INT }, \
	{ "shared_memory_objects", CTLTYPE_INT }, \
	{ "synchronized_io", CTLTYPE_INT }, \
	{ "timers", CTLTYPE_INT }, \
	{ "aio_listio_max", CTLTYPE_INT }, \
	{ "aio_max", CTLTYPE_INT }, \
	{ "aio_prio_delta_max", CTLTYPE_INT }, \
	{ "delaytimer_max", CTLTYPE_INT }, \
	{ "mq_open_max", CTLTYPE_INT }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "rtsig_max", CTLTYPE_INT }, \
	{ "nsems_max", CTLTYPE_INT }, \
	{ "sem_value_max", CTLTYPE_INT }, \
	{ "sigqueue_max", CTLTYPE_INT }, \
	{ "timer_max", CTLTYPE_INT }, \
}

#endif /* _POSIX4_VISIBLE */
#endif /* _POSIX4_POSIX4_H_ */
