/*
 * Copyright (c) 2001 Daniel Eischen <deischen@FreeBSD.org>.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <signal.h>
#include <pthread.h>

#include "libc_private.h"

/*
 * Weak symbols: All libc internal usage of these functions should
 * use the weak symbol versions (_pthread_XXX).  If libpthread is
 * linked, it will override these functions with (non-weak) routines.
 * The _pthread_XXX functions are provided solely for internal libc
 * usage to avoid unwanted cancellation points and to differentiate
 * between application locks and libc locks (threads holding the
 * latter can't be allowed to exit/terminate).
 */

/* Define a null pthread structure just to satisfy _pthread_self. */
struct pthread {
};

static struct pthread	main_thread;

static int		stub_main(void);
static void 		*stub_null(void);
static struct pthread	*stub_self(void);
static int		stub_zero(void);

#define	PJT_DUAL_ENTRY(entry)	\
	(pthread_func_t)entry, (pthread_func_t)entry

pthread_func_entry_t __thr_jtable[PJT_MAX] = {
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_COND_BROADCAST */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_COND_DESTROY */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_COND_INIT */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_COND_SIGNAL */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_COND_WAIT */
	{PJT_DUAL_ENTRY(stub_null)},	/* PJT_GETSPECIFIC */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_KEY_CREATE */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_KEY_DELETE */
	{PJT_DUAL_ENTRY(stub_main)},	/* PJT_MAIN_NP */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEX_DESTROY */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEX_INIT */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEX_LOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEX_TRYLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEX_UNLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEXATTR_DESTROY */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEXATTR_INIT */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_MUTEXATTR_SETTYPE */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_ONCE */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_DESTROY */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_INIT */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_RDLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_TRYRDLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_TRYWRLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_UNLOCK */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_RWLOCK_WRLOCK */
	{PJT_DUAL_ENTRY(stub_self)},	/* PJT_SELF */
	{PJT_DUAL_ENTRY(stub_zero)},	/* PJT_SETSPECIFIC */
	{PJT_DUAL_ENTRY(stub_zero)}	/* PJT_SIGMASK */
};

/*
 * Weak aliases for exported (pthread_*) and internal (_pthread_*) routines.
 */
#define	WEAK_REF(sym, alias)	__weak_reference(sym, alias)

#define	FUNC_TYPE(name)		__CONCAT(name, _func_t)
#define	FUNC_INT(name)		__CONCAT(name, _int)
#define	FUNC_EXP(name)		__CONCAT(name, _exp)

#define	STUB_FUNC(name, idx, ret)				\
	static ret FUNC_EXP(name)(void) __used;			\
	static ret FUNC_INT(name)(void) __used;			\
	WEAK_REF(FUNC_EXP(name), name);				\
	WEAK_REF(FUNC_INT(name), __CONCAT(_, name));		\
	typedef ret (*FUNC_TYPE(name))(void);			\
	static ret FUNC_EXP(name)(void)				\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][0];	\
		return (func());				\
	}							\
	static ret FUNC_INT(name)(void)				\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][1];	\
		return (func());				\
	}

#define	STUB_FUNC1(name, idx, ret, p0_type)			\
	static ret FUNC_EXP(name)(p0_type) __used;		\
	static ret FUNC_INT(name)(p0_type) __used;		\
	WEAK_REF(FUNC_EXP(name), name);				\
	WEAK_REF(FUNC_INT(name), __CONCAT(_, name));		\
	typedef ret (*FUNC_TYPE(name))(p0_type);		\
	static ret FUNC_EXP(name)(p0_type p0)			\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][0];	\
		return (func(p0));				\
	}							\
	static ret FUNC_INT(name)(p0_type p0)			\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][1];	\
		return (func(p0));				\
	}

#define	STUB_FUNC2(name, idx, ret, p0_type, p1_type)		\
	static ret FUNC_EXP(name)(p0_type, p1_type) __used;	\
	static ret FUNC_INT(name)(p0_type, p1_type) __used;	\
	WEAK_REF(FUNC_EXP(name), name);				\
	WEAK_REF(FUNC_INT(name), __CONCAT(_, name));		\
	typedef ret (*FUNC_TYPE(name))(p0_type, p1_type);	\
	static ret FUNC_EXP(name)(p0_type p0, p1_type p1)	\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][0];	\
		return (func(p0, p1));				\
	}							\
	static ret FUNC_INT(name)(p0_type p0, p1_type p1)	\
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][1];	\
		return (func(p0, p1));				\
	}

#define	STUB_FUNC3(name, idx, ret, p0_type, p1_type, p2_type)	\
	static ret FUNC_EXP(name)(p0_type, p1_type, p2_type) __used; \
	static ret FUNC_INT(name)(p0_type, p1_type, p2_type) __used; \
	WEAK_REF(FUNC_EXP(name), name);				\
	WEAK_REF(FUNC_INT(name), __CONCAT(_, name));		\
	typedef ret (*FUNC_TYPE(name))(p0_type, p1_type, p2_type); \
	static ret FUNC_EXP(name)(p0_type p0, p1_type p1, p2_type p2) \
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][0];	\
		return (func(p0, p1, p2));			\
	}							\
	static ret FUNC_INT(name)(p0_type p0, p1_type p1, p2_type p2) \
	{							\
		FUNC_TYPE(name) func;				\
		func = (FUNC_TYPE(name))__thr_jtable[idx][1];	\
		return (func(p0, p1, p2));			\
	}

STUB_FUNC1(pthread_cond_broadcast, PJT_COND_BROADCAST, int, void *)
STUB_FUNC1(pthread_cond_destroy, PJT_COND_DESTROY, int, void *)
STUB_FUNC2(pthread_cond_init,	PJT_COND_INIT, int, void *, void *)
STUB_FUNC1(pthread_cond_signal,	PJT_COND_SIGNAL, int, void *)
STUB_FUNC2(pthread_cond_wait,	PJT_COND_WAIT, int, void *, void *)
STUB_FUNC1(pthread_getspecific,	PJT_GETSPECIFIC, void *, pthread_key_t)
STUB_FUNC2(pthread_key_create,	PJT_KEY_CREATE, int, void *, void *)
STUB_FUNC1(pthread_key_delete,	PJT_KEY_DELETE, int, pthread_key_t)
STUB_FUNC(pthread_main_np,	PJT_MAIN_NP, int)
STUB_FUNC1(pthread_mutex_destroy, PJT_MUTEX_DESTROY, int, void *)
STUB_FUNC2(pthread_mutex_init,	PJT_MUTEX_INIT, int, void *, void *)
STUB_FUNC1(pthread_mutex_lock,	PJT_MUTEX_LOCK, int, void *)
STUB_FUNC1(pthread_mutex_trylock, PJT_MUTEX_TRYLOCK, int, void *)
STUB_FUNC1(pthread_mutex_unlock, PJT_MUTEX_UNLOCK, int, void *)
STUB_FUNC1(pthread_mutexattr_destroy, PJT_MUTEXATTR_DESTROY, int, void *)
STUB_FUNC1(pthread_mutexattr_init, PJT_MUTEXATTR_INIT, int, void *)
STUB_FUNC2(pthread_mutexattr_settype, PJT_MUTEXATTR_SETTYPE, int, void *, int)
STUB_FUNC2(pthread_once, 	PJT_ONCE, int, void *, void *)
STUB_FUNC1(pthread_rwlock_destroy, PJT_RWLOCK_DESTROY, int, void *)
STUB_FUNC2(pthread_rwlock_init,	PJT_RWLOCK_INIT, int, void *, void *)
STUB_FUNC1(pthread_rwlock_rdlock, PJT_RWLOCK_RDLOCK, int, void *)
STUB_FUNC1(pthread_rwlock_tryrdlock, PJT_RWLOCK_TRYRDLOCK, int, void *)
STUB_FUNC1(pthread_rwlock_trywrlock, PJT_RWLOCK_TRYWRLOCK, int, void *)
STUB_FUNC1(pthread_rwlock_unlock, PJT_RWLOCK_UNLOCK, int, void *)
STUB_FUNC1(pthread_rwlock_wrlock, PJT_RWLOCK_WRLOCK, int, void *)
STUB_FUNC(pthread_self,		PJT_SELF, pthread_t)
STUB_FUNC2(pthread_setspecific, PJT_SETSPECIFIC, int, pthread_key_t, void *)
STUB_FUNC3(pthread_sigmask, PJT_SIGMASK, int, int, void *, void *)

static int
stub_zero(void)
{
	return (0);
}

static void *
stub_null(void)
{
	return (NULL);
}

static struct pthread *
stub_self(void)
{
	return (&main_thread);
}

static int
stub_main(void)
{
	return (-1);
}
