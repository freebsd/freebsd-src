/*-
 * Copyright (c) 2008-2009 Stacey Son <sson@FreeBSD.org> 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
 
/*
 * DTrace lockstat provider definitions
 *
 */

#ifndef	_SYS_LOCKSTAT_H
#define	_SYS_LOCKSTAT_H

#ifdef	_KERNEL

/*
 * Spin Locks
 */
#define	LS_MTX_SPIN_LOCK_ACQUIRE	0
#define	LS_MTX_SPIN_UNLOCK_RELEASE	1
#define	LS_MTX_SPIN_LOCK_SPIN		2

/*
 * Adaptive Locks
 */
#define	LS_MTX_LOCK_ACQUIRE		3
#define	LS_MTX_UNLOCK_RELEASE		4
#define	LS_MTX_LOCK_SPIN		5
#define	LS_MTX_LOCK_BLOCK		6
#define	LS_MTX_TRYLOCK_ACQUIRE		7

/*
 * Reader/Writer Locks
 */
#define	LS_RW_RLOCK_ACQUIRE		8
#define	LS_RW_RUNLOCK_RELEASE		9	
#define	LS_RW_WLOCK_ACQUIRE		10
#define	LS_RW_WUNLOCK_RELEASE		11
#define	LS_RW_RLOCK_SPIN		12
#define	LS_RW_RLOCK_BLOCK		13
#define	LS_RW_WLOCK_SPIN		14
#define	LS_RW_WLOCK_BLOCK		15
#define	LS_RW_TRYUPGRADE_UPGRADE	16
#define	LS_RW_DOWNGRADE_DOWNGRADE	17

/*
 * Shared/Exclusive Locks
 */
#define	LS_SX_SLOCK_ACQUIRE		18
#define	LS_SX_SUNLOCK_RELEASE		19
#define	LS_SX_XLOCK_ACQUIRE		20
#define	LS_SX_XUNLOCK_RELEASE		21
#define	LS_SX_SLOCK_SPIN		22
#define	LS_SX_SLOCK_BLOCK		23
#define	LS_SX_XLOCK_SPIN		24
#define	LS_SX_XLOCK_BLOCK		25
#define	LS_SX_TRYUPGRADE_UPGRADE	26
#define	LS_SX_DOWNGRADE_DOWNGRADE	27

/* 
 * Thread Locks
 */
#define	LS_THREAD_LOCK_SPIN		28

/*
 * Lockmanager Locks 
 *  According to locking(9) Lockmgr locks are "Largely deprecated"
 *  so no support for these have been added in the lockstat provider.
 */

#define	LS_NPROBES			29

#define	LS_MTX_LOCK			"mtx_lock"
#define	LS_MTX_UNLOCK			"mtx_unlock"
#define	LS_MTX_SPIN_LOCK		"mtx_lock_spin"
#define	LS_MTX_SPIN_UNLOCK		"mtx_unlock_spin"
#define	LS_MTX_TRYLOCK			"mtx_trylock"
#define	LS_RW_RLOCK			"rw_rlock"
#define	LS_RW_WLOCK			"rw_wlock"
#define	LS_RW_RUNLOCK			"rw_runlock"
#define	LS_RW_WUNLOCK			"rw_wunlock"
#define	LS_RW_TRYUPGRADE		"rw_try_upgrade"
#define	LS_RW_DOWNGRADE			"rw_downgrade"
#define	LS_SX_SLOCK			"sx_slock"
#define	LS_SX_XLOCK			"sx_xlock"
#define	LS_SX_SUNLOCK			"sx_sunlock"
#define	LS_SX_XUNLOCK			"sx_xunlock"
#define	LS_SX_TRYUPGRADE		"sx_try_upgrade"
#define	LS_SX_DOWNGRADE			"sx_downgrade"
#define	LS_THREAD_LOCK			"thread_lock"

#define	LS_ACQUIRE			"acquire"
#define	LS_RELEASE			"release"
#define	LS_SPIN				"spin"
#define	LS_BLOCK			"block"
#define	LS_UPGRADE			"upgrade"
#define	LS_DOWNGRADE			"downgrade"

#define	LS_TYPE_ADAPTIVE		"adaptive"
#define	LS_TYPE_SPIN			"spin"
#define	LS_TYPE_THREAD			"thread"
#define	LS_TYPE_RW			"rw"
#define	LS_TYPE_SX			"sx"

#define	LSA_ACQUIRE			(LS_TYPE_ADAPTIVE "-" LS_ACQUIRE)
#define	LSA_RELEASE			(LS_TYPE_ADAPTIVE "-" LS_RELEASE)
#define	LSA_SPIN			(LS_TYPE_ADAPTIVE "-" LS_SPIN)
#define	LSA_BLOCK			(LS_TYPE_ADAPTIVE "-" LS_BLOCK)
#define	LSS_ACQUIRE			(LS_TYPE_SPIN "-" LS_ACQUIRE)
#define	LSS_RELEASE			(LS_TYPE_SPIN "-" LS_RELEASE)
#define	LSS_SPIN			(LS_TYPE_SPIN "-" LS_SPIN)
#define	LSR_ACQUIRE			(LS_TYPE_RW "-" LS_ACQUIRE)
#define	LSR_RELEASE			(LS_TYPE_RW "-" LS_RELEASE)
#define	LSR_BLOCK			(LS_TYPE_RW "-" LS_BLOCK)
#define	LSR_SPIN			(LS_TYPE_RW "-" LS_SPIN)
#define	LSR_UPGRADE			(LS_TYPE_RW "-" LS_UPGRADE)
#define	LSR_DOWNGRADE			(LS_TYPE_RW "-" LS_DOWNGRADE)
#define	LSX_ACQUIRE			(LS_TYPE_SX "-" LS_ACQUIRE)
#define	LSX_RELEASE			(LS_TYPE_SX "-" LS_RELEASE)
#define	LSX_BLOCK			(LS_TYPE_SX "-" LS_BLOCK)
#define	LSX_SPIN			(LS_TYPE_SX "-" LS_SPIN)
#define	LSX_UPGRADE			(LS_TYPE_SX "-" LS_UPGRADE)
#define	LSX_DOWNGRADE			(LS_TYPE_SX "-" LS_DOWNGRADE)
#define	LST_SPIN			(LS_TYPE_THREAD "-" LS_SPIN)

/*
 * The following must match the type definition of dtrace_probe.  It is
 * defined this way to avoid having to rely on CDDL code.
 */
struct lock_object;
extern uint32_t lockstat_probemap[LS_NPROBES];
typedef void (*lockstat_probe_func_t)(uint32_t, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);
extern lockstat_probe_func_t lockstat_probe_func;
extern uint64_t lockstat_nsecs(struct lock_object *);
extern int lockstat_enabled;

#ifdef	KDTRACE_HOOKS
/*
 * Macros to record lockstat probes.
 */
#define	LOCKSTAT_RECORD4(probe, lp, arg1, arg2, arg3, arg4)  do {	\
	uint32_t id;							\
									\
	if ((id = lockstat_probemap[(probe)])) 				\
	    (*lockstat_probe_func)(id, (uintptr_t)(lp), (arg1),	(arg2),	\
		(arg3), (arg4));					\
} while (0)

#define	LOCKSTAT_RECORD(probe, lp, arg1) \
	LOCKSTAT_RECORD4(probe, lp, arg1, 0, 0, 0)

#define	LOCKSTAT_RECORD0(probe, lp)     \
	LOCKSTAT_RECORD4(probe, lp, 0, 0, 0, 0)

#define	LOCKSTAT_RECORD1(probe, lp, arg1) \
	LOCKSTAT_RECORD4(probe, lp, arg1, 0, 0, 0)

#define	LOCKSTAT_RECORD2(probe, lp, arg1, arg2) \
	LOCKSTAT_RECORD4(probe, lp, arg1, arg2, 0, 0)

#define	LOCKSTAT_RECORD3(probe, lp, arg1, arg2, arg3) \
	LOCKSTAT_RECORD4(probe, lp, arg1, arg2, arg3, 0)

#define	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(probe, lp, c, wt, f, l)  do {   \
	uint32_t id;							     \
									     \
    	lock_profile_obtain_lock_success(&(lp)->lock_object, c, wt, f, l);   \
	if ((id = lockstat_probemap[(probe)])) 			     	     \
		(*lockstat_probe_func)(id, (uintptr_t)(lp), 0, 0, 0, 0);     \
} while (0)

#define	LOCKSTAT_PROFILE_RELEASE_LOCK(probe, lp)  do {			     \
	uint32_t id;							     \
									     \
	lock_profile_release_lock(&(lp)->lock_object);			     \
	if ((id = lockstat_probemap[(probe)])) 			     	     \
		(*lockstat_probe_func)(id, (uintptr_t)(lp), 0, 0, 0, 0);     \
} while (0)

#define	LOCKSTAT_WRITER		0
#define	LOCKSTAT_READER		1

#else	/* !KDTRACE_HOOKS */

#define	LOCKSTAT_RECORD(probe, lp, arg1)
#define	LOCKSTAT_RECORD0(probe, lp)
#define	LOCKSTAT_RECORD1(probe, lp, arg1)
#define	LOCKSTAT_RECORD2(probe, lp, arg1, arg2)
#define	LOCKSTAT_RECORD3(probe, lp, arg1, arg2, arg3)
#define	LOCKSTAT_RECORD4(probe, lp, arg1, arg2, arg3, arg4)

#define	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(probe, lp, c, wt, f, l)	\
	lock_profile_obtain_lock_success(&(lp)->lock_object, c, wt, f, l)

#define	LOCKSTAT_PROFILE_RELEASE_LOCK(probe, lp)  			\
	lock_profile_release_lock(&(lp)->lock_object)

#endif	/* !KDTRACE_HOOKS */

#endif	/* _KERNEL */

#endif	/* _SYS_LOCKSTAT_H */
