/*	$FreeBSD$ */
/*	$NetBSD: rf_threadstuff.h,v 1.10 2001/01/27 20:42:21 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * threadstuff.h -- definitions for threads, locks, and synchronization
 *
 * The purpose of this file is provide some illusion of portability.
 * If the functions below can be implemented with the same semantics on
 * some new system, then at least the synchronization and thread control
 * part of the code should not require modification to port to a new machine.
 * the only other place where the pthread package is explicitly used is
 * threadid.h
 *
 * this file should be included above stdio.h to get some necessary defines.
 *
 */

#ifndef _RF__RF_THREADSTUFF_H_
#define _RF__RF_THREADSTUFF_H_

#include <dev/raidframe/rf_types.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#endif

#define rf_create_managed_mutex(a,b) _rf_create_managed_mutex(a,b,__FILE__,__LINE__)
#define rf_create_managed_cond(a,b) _rf_create_managed_cond(a,b,__FILE__,__LINE__)
#define rf_init_managed_threadgroup(a,b) _rf_init_managed_threadgroup(a,b,__FILE__,__LINE__)
#define rf_init_threadgroup(a) _rf_init_threadgroup(a,__FILE__,__LINE__)
#define rf_destroy_threadgroup(a) _rf_destroy_threadgroup(a,__FILE__,__LINE__)

int     _rf_init_threadgroup(RF_ThreadGroup_t * g, char *file, int line);
int     _rf_destroy_threadgroup(RF_ThreadGroup_t * g, char *file, int line);
int 
_rf_init_managed_threadgroup(RF_ShutdownList_t ** listp,
    RF_ThreadGroup_t * g, char *file, int line);

#include <sys/lock.h>
#if defined(__FreeBSD__ ) && __FreeBSD_version > 500005
#include <sys/mutex.h>
#define decl_simple_lock_data(a,b) a struct mtx b;
#define simple_lock_addr(a) ((struct mtx *)&(a))

typedef struct thread *RF_Thread_t;
typedef void *RF_ThreadArg_t;

#ifdef _KERNEL
static __inline struct ucred *
rf_getucred(RF_Thread_t td)
{
	return (((struct thread *)td)->td_proc->p_ucred);
}
#endif

#define RF_LOCK_MUTEX(_m_)              mtx_lock(&(_m_))
#define RF_UNLOCK_MUTEX(_m_)            mtx_unlock(&(_m_))
#else
#define decl_simple_lock_data(a,b) a struct simplelock b;
#define simple_lock_addr(a) ((struct simplelock *)&(a))

typedef struct proc *RF_Thread_t;
typedef void *RF_ThreadArg_t;

static __inline struct ucred *
rf_getucred(RF_Thread_t td)
{
	return (((struct proc *)td)->p_ucred);
}

#define RF_LOCK_MUTEX(_m_)              simple_lock(&(_m_))
#define RF_UNLOCK_MUTEX(_m_)            simple_unlock(&(_m_))
#endif

#define RF_DECLARE_MUTEX(_m_)           decl_simple_lock_data(,(_m_))
#define RF_DECLARE_STATIC_MUTEX(_m_)    decl_simple_lock_data(static,(_m_))
#define RF_DECLARE_EXTERN_MUTEX(_m_)    decl_simple_lock_data(extern,(_m_))

#define RF_DECLARE_COND(_c_)            int _c_;
#define RF_DECLARE_STATIC_COND(_c_)     static int _c_;
#define RF_DECLARE_EXTERN_COND(_c_)     extern int _c_;

/*
 * In NetBSD, kernel threads are simply processes which share several
 * substructures and never run in userspace.
 */
#define RF_WAIT_COND(_c_,_m_)		\
	RF_LTSLEEP(&(_c_), PRIBIO, "rfwcond", 0, &(_m_))
#define RF_SIGNAL_COND(_c_)            wakeup_one(&(_c_))
#define RF_BROADCAST_COND(_c_)         wakeup(&(_c_))
#if defined(__NetBSD__)
#define	RF_CREATE_THREAD(_handle_, _func_, _arg_, _name_) \
	kthread_create1((void (*)(void *))(_func_), (void *)(_arg_), \
	    (struct proc **)&(_handle_), _name_)
#define RF_THREAD_EXIT(ret)	\
	kthread_exit(ret)
#elif defined(__FreeBSD__)
#if __FreeBSD_version > 500005
#define RF_CREATE_THREAD(_handle_, _func_, _arg_, _name_) \
	kthread_create((void (*)(void *))(_func_), (void *)(_arg_), \
	    (struct proc **)&(_handle_), 0, 4, _name_)
#define RF_THREAD_EXIT(ret)	\
	kthread_exit(ret)
#else
#define RF_CREATE_THREAD(_handle_, _func_, _arg_, _name_) \
	kthread_create((void (*)(void *))(_func_), (void *)(_arg_), \
	    (struct proc **)&(_handle_), _name_)
#define RF_THREAD_EXIT(ret)	\
	kthread_exit(ret);
#endif
#endif

struct RF_ThreadGroup_s {
	int     created;
	int     running;
	int     shutdown;
	        RF_DECLARE_MUTEX(mutex)
	        RF_DECLARE_COND(cond)
};
/*
 * Someone has started a thread in the group
 */
#define RF_THREADGROUP_STARTED(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->created++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}

/*
 * Thread announcing that it is now running
 */
#define RF_THREADGROUP_RUNNING(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->running++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
	RF_SIGNAL_COND((_g_)->cond); \
}

/*
 * Thread announcing that it is now done
 */
#define RF_THREADGROUP_DONE(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->shutdown++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
	RF_SIGNAL_COND((_g_)->cond); \
}

/*
 * Wait for all threads to start running
 */
#define RF_THREADGROUP_WAIT_START(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	while((_g_)->running < (_g_)->created) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}

/*
 * Wait for all threads to stop running
 */
#ifndef __NetBSD__
#define RF_THREADGROUP_WAIT_STOP(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	RF_ASSERT((_g_)->running == (_g_)->created); \
	while((_g_)->shutdown < (_g_)->running) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}
#else
 /* XXX Note that we've removed the assert.  That should get put back in once
  * we actually get something like a kernel thread running */
#define RF_THREADGROUP_WAIT_STOP(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	while((_g_)->shutdown < (_g_)->running) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}
#endif

#if defined(__FreeBSD__) && __FreeBSD_version > 500005
int     rf_mutex_init(struct mtx *, const char *);
int     rf_mutex_destroy(struct mtx *);
int	_rf_create_managed_mutex(RF_ShutdownList_t **, struct mtx *,
				 char *, int);
#else
int     rf_mutex_init(struct simplelock *, const char *);
int     rf_mutex_destroy(struct simplelock *);
int	_rf_create_managed_mutex(RF_ShutdownList_t **, struct simplelock *,
				 char *, int);
#endif
int	_rf_create_managed_cond(RF_ShutdownList_t ** listp, int *,
				char *file, int line);

int     rf_cond_init(int *c);
int     rf_cond_destroy(int *c);
#endif				/* !_RF__RF_THREADSTUFF_H_ */
