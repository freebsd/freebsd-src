/*-
 * SPDX-License-Identifier: (BSD-4-Clause AND BSD-2-Clause)
 *
 * Copyright (c) 1996, 1997
 *      HD Associates, Inc.  All rights reserved.
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
 *      This product includes software developed by HD Associates, Inc
 *      and Jukka Antero Ukkonen.
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
 */

/*-
 * Copyright (c) 2002-2008, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Olivier Certner
 * <olce@FreeBSD.org> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SCHED_H_
#define	_SCHED_H_

#ifdef _KERNEL

#include <sys/types.h>
#ifdef SCHED_STATS
#include <sys/pcpu.h>
#endif
#include <sys/linker_set.h>
#include <sys/sdt.h>

/*
 * Scheduler interface.
 *
 * The scheduler interface is the list of functions that external users can
 * call.  It consists of two parts: Functions that must be provided directly by
 * a scheduler implementation (called an "instance"), and functions with
 * a common implementation (which may indirectly call instance functions).
 *
 * The former are declared in an abstract manner with some of the SCHED_ITF_*()
 * macros (SCHED_ITF_INTERNAL() being a notable exception), described in detail
 * below.  A FUN() line declares a function, and the final interface function
 * name is the symbol in the third argument prefixed by 'sched_'.  E.g., such
 * lines:
 *
 * FUN((__VA_ARGS__), int, load)
 * FUN((__VA_ARGS__), void, switch, struct thread *, td, int, flags)
 *
 * lead to the C declarations (among others):
 *
 * int sched_load(void);
 * void sched_switch(struct thread *, int);
 *
 * and each implementation (instance) must implement corresponding functions,
 * e.g., for ULE:
 *
 * int sched_ule_load(void);
 * void sched_ule_switch(struct thread *td, int flags);
 *
 * Such interface functions are grouped and commented in the SCHED_ITF*()
 * macros listing them.
 *
 * Functions with a common implementation are declared under "Common functions"
 * below as commented plain C declarations.
 */

/*
 * Macros SCHED_ITF_*() define functions that must be provided by an instance.
 * Please see their herald comments for details about their differences.  They
 * are X macros to avoid repetitions in function signatures, slot names,
 * dispatch code, and scheduler instance declaration.  They take as an argument
 * the FUN() macro, which is expanded on each interface's function descriptor.
 *
 * The arguments passed to FUN() are:
 * - The variable arguments to the X macro, as the first argument.
 * - The function descriptor, consisting of:
 *   - Field name in 'struct sched_instance'.
 *   - Return type.
 *   - Function name; prefixed by 'sched_' for the publicly visible functions,
 *     and by 'sched_<instance_name>_' for instance's implementation.
 *   - Function arguments, each being represented as a pair of successive macro
 *     parameters, the first being the argument's type and the second its name.
 * Note that FUN() is passed a variable number of arguments for flexibility.
 *
 * In SCHED_ITF_*() macros, please keep the function's parameters on a separate
 * line for clarity, even if everything would fit on a single line.
 */

/* sched_add() arguments. */
#define	SRQ_BORING	0x0000		/* No special circumstances. */
#define	SRQ_YIELDING	0x0001		/* We are yielding (from mi_switch). */
#define	SRQ_OURSELF	0x0002		/* It is ourself (from mi_switch). */
#define	SRQ_INTR	0x0004		/* It is probably urgent. */
#define	SRQ_PREEMPTED	0x0008		/* has been preempted.. be kind */
#define	SRQ_BORROWING	0x0010		/* Priority updated due to prio_lend */
#define	SRQ_HOLD	0x0020		/* Return holding original td lock */
#define	SRQ_HOLDTD	0x0040		/* Return holding td lock */

/*
 * Functions to be directly dispatched to the active scheduler instance.
 *
 * These are implemented using ifuncs for zero-cost dispatch.
 */
#define SCHED_ITF_DISPATCH(FUN, ...)					\
	/*								\
	 * General scheduling info.					\
	 *								\
	 * sched_load:							\
	 *	Total runnable non-ithread threads in the system.	\
	 *								\
	 * sched_runnable:						\
	 *	Runnable threads for this processor.			\
	 */								\
	FUN((__VA_ARGS__), int, load)					\
	FUN((__VA_ARGS__), int, rr_interval)				\
	FUN((__VA_ARGS__), bool, runnable)				\
									\
	/*								\
	 * Proc related scheduling hooks.				\
	 */								\
	FUN((__VA_ARGS__), void, exit,					\
	    struct proc *, p, struct thread *, child)			\
	FUN((__VA_ARGS__), void, fork,					\
	    struct thread *, td, struct thread *, child)		\
	FUN((__VA_ARGS__), void, fork_exit,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, class,					\
	    struct thread *, td, int, class)				\
	FUN((__VA_ARGS__), void, nice,					\
	    struct proc *, p, int, nice)				\
									\
	/*								\
	 * Threads are switched in and out, block on resources,		\
	 * have temporary priorities inherited from their procs,	\
	 * and use up cpu time.						\
	 */								\
	FUN((__VA_ARGS__), void, ap_entry)				\
	FUN((__VA_ARGS__), void, exit_thread,				\
	    struct thread *, td, struct thread *, child)		\
	FUN((__VA_ARGS__), u_int, estcpu,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, fork_thread,				\
	    struct thread *, td, struct thread *, child)		\
	FUN((__VA_ARGS__), void, ithread_prio,				\
	    struct thread *, td, u_char, prio)				\
	FUN((__VA_ARGS__), void, lend_prio,				\
	    struct thread *, td, u_char, prio)				\
	FUN((__VA_ARGS__), void, lend_user_prio,			\
	    struct thread *, td, u_char, pri)				\
	FUN((__VA_ARGS__), void, lend_user_prio_cond,			\
	    struct thread *, td, u_char, pri)				\
	FUN((__VA_ARGS__), fixpt_t, pctcpu,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, prio,					\
	    struct thread *, td, u_char, prio)				\
	FUN((__VA_ARGS__), void, sleep,					\
	    struct thread *, td, int, prio)				\
	FUN((__VA_ARGS__), void, switch,				\
	    struct thread *, td, int, flags)				\
	FUN((__VA_ARGS__), void, throw,					\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, unlend_prio,				\
	    struct thread *, td, u_char, prio)				\
	FUN((__VA_ARGS__), void, user_prio,				\
	    struct thread *, td, u_char, prio)				\
	FUN((__VA_ARGS__), void, userret_slowpath,			\
	    struct thread *, td)					\
									\
	/*								\
	 * Threads are moved on and off of run queues			\
	 */								\
	FUN((__VA_ARGS__), void, add,					\
	    struct thread *, td, int, flags)				\
	FUN((__VA_ARGS__), struct thread *, choose)			\
	FUN((__VA_ARGS__), void, clock,					\
	    struct thread *, td, int, cnt)				\
	FUN((__VA_ARGS__), void, idletd,				\
	    void *, dummy)						\
	FUN((__VA_ARGS__), void, preempt,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, relinquish,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, rem,					\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, wakeup,				\
	    struct thread *, td, int, srqflags)				\
									\
	/*								\
	 * Binding makes cpu affinity permanent while pinning is used	\
	 * to temporarily hold a thread on a particular CPU.		\
	 */								\
	FUN((__VA_ARGS__), void, bind,					\
	    struct thread *, td, int, cpu)				\
	FUN((__VA_ARGS__), void, unbind,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), int, is_bound,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, affinity,				\
	    struct thread *, td)					\
									\
	/*								\
	 * These procedures tell the process data structure allocation  \
	 * code how many bytes to actually allocate.			\
	 */								\
	FUN((__VA_ARGS__), int, sizeof_proc)				\
	FUN((__VA_ARGS__), int, sizeof_thread)				\
									\
	/*								\
	 * This routine provides a consistent thread name for use with	\
	 * KTR graphing FUNctions.					\
	 */								\
	FUN((__VA_ARGS__), char *, tdname,				\
	    struct thread *, td)					\
	FUN((__VA_ARGS__), void, clear_tdname,				\
	    struct thread *, td)					\
									\
	/*								\
	 * Find an L2 neighbor of the given CPU or return -1 if none	\
	 * found.  This does not distinguish among multiple L2		\
	 * if the given CPU has more than one (it will always return	\
	 * the same result in that case).				\
	 */								\
	FUN((__VA_ARGS__), int,	find_l2_neighbor,			\
	    int, cpu)							\
									\
	/*								\
	 * Fixup scheduler state for secondary APs			\
	 */								\
	FUN((__VA_ARGS__), void, init_ap)

/*
 * Instance functions to be specifically wrapped.
 *
 * An explicit common implementation must be provided for these, and it is
 * expected to call the instance's implementation.
 */
#define SCHED_ITF_WRAP(FUN, ...)					\
	/*								\
	 * Initialize scheduler state for proc0 and thread0.		\
	 */								\
	FUN((__VA_ARGS__), void, init)

/*
 * Instance functions used internally.
 *
 * Functions here are not made available for external callers.
 */
#define SCHED_ITF_INTERNAL(FUN, ...)					\
	/*								\
	 * Setup run queues ('cpu_top' filled by the shim machinery).   \
	 */								\
	FUN((__VA_ARGS__), void, setup)					\
									\
	/*								\
	 * Determines time constants after stathz and hz are setup.	\
	 */								\
	FUN((__VA_ARGS__), void, initticks)				\
									\
	/*								\
	 * Final steps after all the rest has been initialized.		\
	 */								\
	FUN((__VA_ARGS__), void, sysinit)

/* All parts at once, but common functions. */
#define SCHED_ITF_NOCOMMON(FUN, ...)					\
	SCHED_ITF_DISPATCH(FUN, __VA_ARGS__)				\
	SCHED_ITF_WRAP(FUN, __VA_ARGS__)				\
	SCHED_ITF_INTERNAL(FUN, __VA_ARGS__)

/*
 * Machinery to help define FUN() macros.
 */
#define _SCHED_ITF_ARG1(a0, ...)					\
	a0
#define _SCHED_ITF_ARG9(a0, a1, a2, a3, a4, a5, a6, a7, a8, ...)	\
	a8
#define _SCHED_ITF_NARGS(...)						\
	_SCHED_ITF_ARG9(__VA_ARGS__ __VA_OPT__(,) 4, ERR, 3, ERR, 2,	\
	    ERR, 1, ERR, 0)
#define _SCHED_ITF_NARGS_DISP(PREFIX, disp_args, full_args)		\
	__CONCAT(__CONCAT(PREFIX, _), _SCHED_ITF_NARGS disp_args)	\
		full_args
#define _SCHED_ITF_ARGS_PAIR_ERR					\
	"Two macro arguments per interface function argument expected."
#define _SCHED_ITF_ARGS_PROTO_NAME_0()					\
	void
#define _SCHED_ITF_ARGS_PROTO_NAME_1(t1, a1)				\
	t1 a1
#define _SCHED_ITF_ARGS_PROTO_NAME_2(t1, a1, t2, a2)			\
	t1 a1, t2 a2
#define _SCHED_ITF_ARGS_PROTO_NAME_3(t1, a1, t2, a2, t3, a3)		\
	t1 a1, t2 a2, t3 a3
#define _SCHED_ITF_ARGS_PROTO_NAME_4(t1, a1, t2, a2, t3, a3, t4, a4)	\
	t1 a1, t2 a2, t3 a3, t4 a4
#define _SCHED_ITF_ARGS_PROTO_NAME_ERR(...)				\
	_SCHED_ITF_ARGS_PAIR_ERR
#define SCHED_ITF_ARGS_PROTO_NAME(...)					\
	_SCHED_ITF_NARGS_DISP(_SCHED_ITF_ARGS_PROTO_NAME,		\
	    (__VA_ARGS__), (__VA_ARGS__))
#define _SCHED_ITF_ARGS_PROTO_NONAME_0()				\
	void
#define _SCHED_ITF_ARGS_PROTO_NONAME_1(t1, a1)				\
	t1
#define _SCHED_ITF_ARGS_PROTO_NONAME_2(t1, a1, t2, a2)			\
	t1, t2
#define _SCHED_ITF_ARGS_PROTO_NONAME_3(t1, a1, t2, a2, t3, a3)		\
	t1, t2, t3
#define _SCHED_ITF_ARGS_PROTO_NONAME_4(t1, a1, t2, a2, t3, a3, t4, a4)	\
	t1, t2, t3, t4
#define _SCHED_ITF_ARGS_PROTO_NONAME_ERR(...)				\
	_SCHED_ITF_ARGS_PAIR_ERR
#define SCHED_ITF_ARGS_PROTO_NONAME(...)				\
	_SCHED_ITF_NARGS_DISP(_SCHED_ITF_ARGS_PROTO_NONAME,		\
	    (__VA_ARGS__), (__VA_ARGS__))

#define SCHED_ITF_FIELD_NAME(fn)					\
	__CONCAT(fn, _impl)
#define SCHED_ITF_FUNCTION_NAME(fn)					\
	__CONCAT(sched_, fn)
#define _SCHED_ITF_IMPL_FUNCTION_NAME(sched_name, fn)			\
	SCHED_ITF_FUNCTION_NAME(__CONCAT(__CONCAT(sched_name, _),	\
	    fn))

/*
 * C declaration of first part of interface functions.
 */
struct proc;
struct thread;

/* Eliding argument names to avoid collisions with C++ keywords. */
#define _SCHED_ITF_FUN(args, ret_type, fn, ...)				\
	ret_type							\
	SCHED_ITF_FUNCTION_NAME(fn)					\
		(SCHED_ITF_ARGS_PROTO_NONAME(__VA_ARGS__));
SCHED_ITF_DISPATCH(_SCHED_ITF_FUN, );
SCHED_ITF_WRAP(_SCHED_ITF_FUN, );
#undef _SCHED_ITF_FUN

/*
 * Common functions.
 */

static inline void
sched_userret(struct thread *td)
{

	/*
	 * XXX we cheat slightly on the locking here to avoid locking in
	 * the usual case.  Setting td_priority here is essentially an
	 * incomplete workaround for not setting it properly elsewhere.
	 * Now that some interrupt handlers are threads, not setting it
	 * properly elsewhere can clobber it in the window between setting
	 * it here and returning to user mode, so don't waste time setting
	 * it perfectly here.
	 */
	KASSERT((td->td_flags & TDF_BORROWING) == 0,
	    ("thread with borrowed priority returning to userland"));
	if (__predict_false(td->td_priority != td->td_user_pri))
		sched_userret_slowpath(td);
}

static __inline void
sched_pin(void)
{
	curthread->td_pinned++;
	atomic_interrupt_fence();
}

static __inline void
sched_unpin(void)
{
	atomic_interrupt_fence();
	MPASS(curthread->td_pinned > 0);
	curthread->td_pinned--;
}

/*
 * Scheduler statistics.
 */
#ifdef SCHED_STATS
DPCPU_DECLARE(long, sched_switch_stats[SWT_COUNT]);

#define	SCHED_STAT_DEFINE_VAR(name, ptr, descr)				\
static void name ## _add_proc(void *dummy __unused)			\
{									\
									\
	SYSCTL_ADD_PROC(NULL,						\
	    SYSCTL_STATIC_CHILDREN(_kern_sched_stats), OID_AUTO,	\
	    #name, CTLTYPE_LONG|CTLFLAG_RD|CTLFLAG_MPSAFE,		\
	    ptr, 0, sysctl_dpcpu_long, "LU", descr);			\
}									\
SYSINIT(name, SI_SUB_LAST, SI_ORDER_MIDDLE, name ## _add_proc, NULL);

#define	SCHED_STAT_DEFINE(name, descr)					\
    DPCPU_DEFINE(unsigned long, name);					\
    SCHED_STAT_DEFINE_VAR(name, &DPCPU_NAME(name), descr)

#define	SCHED_STAT_DECLARE(name)					\
    DPCPU_DECLARE(unsigned long, name);

/*
 * Sched stats are always incremented in critical sections so no atomic
 * is necessary to increment them.
 */
#define SCHED_STAT_INC(var)     DPCPU_GET(var)++;
#else
#define	SCHED_STAT_DEFINE_VAR(name, descr, ptr)
#define	SCHED_STAT_DEFINE(name, descr)
#define	SCHED_STAT_DECLARE(name)
#define SCHED_STAT_INC(var)			(void)0
#endif

SCHED_STAT_DECLARE(ithread_demotions);
SCHED_STAT_DECLARE(ithread_preemptions);

/* Dtrace. */
SDT_PROBE_DECLARE(sched, , , change__pri);
SDT_PROBE_DECLARE(sched, , , dequeue);
SDT_PROBE_DECLARE(sched, , , enqueue);
SDT_PROBE_DECLARE(sched, , , lend__pri);
SDT_PROBE_DECLARE(sched, , , load__change);
SDT_PROBE_DECLARE(sched, , , off__cpu);
SDT_PROBE_DECLARE(sched, , , on__cpu);
SDT_PROBE_DECLARE(sched, , , remain__cpu);
SDT_PROBE_DECLARE(sched, , , surrender);

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
extern int dtrace_vtime_active;
extern dtrace_vtime_switch_func_t dtrace_vtime_switch_func;
#endif

/*
 * Scheduler instance structure.
 */
#define _SCHED_ITF_FUN(args, ret_type, fn, ...)				\
	ret_type (*SCHED_ITF_FIELD_NAME(fn))				\
	(SCHED_ITF_ARGS_PROTO_NONAME(__VA_ARGS__));
struct sched_instance {
	SCHED_ITF_NOCOMMON(_SCHED_ITF_FUN, )
};
#undef _SCHED_ITF_FUN

/*
 * Scheduler instance declaration.
 */
struct sched_selection {
	const char *name;
	const struct sched_instance *instance;
};

#define _SCHED_DECLARE_FIELD_FUN(args, ret_type, fn, ...)		\
	.SCHED_ITF_FIELD_NAME(fn) = &_SCHED_ITF_IMPL_FUNCTION_NAME(	\
	    _SCHED_ITF_ARG1 args, fn),

#define	DECLARE_SCHEDULER(var_name, string_name)			\
	static const struct sched_instance				\
	__CONCAT(_sched_instance_, var_name) = {			\
		SCHED_ITF_NOCOMMON(_SCHED_DECLARE_FIELD_FUN,		\
		    var_name)						\
	};								\
	static const struct sched_selection				\
	__CONCAT(_sched_selector_, var_name) = {			\
		.name = string_name,					\
		.instance = &__CONCAT(_sched_instance_, var_name),	\
	};								\
	DATA_SET(sched_instance_set,					\
	    __CONCAT(_sched_selector_, var_name));

void sched_instance_select(void);

/*
 * Miscellaneous.
 */

void ast_scheduler(struct thread *td, int tda);

#endif /* _KERNEL */

/* POSIX 1003.1b Process Scheduling */

/*
 * POSIX scheduling policies
 */
#define SCHED_FIFO      1
#define SCHED_OTHER     2
#define SCHED_RR        3

struct sched_param {
        int     sched_priority;
};

#ifndef _KERNEL
/*
 * POSIX scheduling declarations for userland.
 */
#include <sys/cdefs.h>
#include <sys/_timespec.h>
#include <sys/_types.h>

#ifndef _PID_T_DECLARED
typedef __pid_t         pid_t;
#define _PID_T_DECLARED
#endif /* !_PID_T_DECLARED */

__BEGIN_DECLS
int     sched_get_priority_max(int);
int     sched_get_priority_min(int);
int     sched_getparam(pid_t, struct sched_param *);
int     sched_getscheduler(pid_t);
int     sched_rr_get_interval(pid_t, struct timespec *);
int     sched_setparam(pid_t, const struct sched_param *);
int     sched_setscheduler(pid_t, int, const struct sched_param *);
int     sched_yield(void);
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SCHED_H_ */
