/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_THREAD_H
#define	_THREAD_H

/*
 * thread.h:
 * definitions needed to use the thread interface except synchronization.
 * use <synch.h> for thread synchronization.
 */

#ifndef _ASM
#include <sys/signal.h>
#include <sys/time.h>
#include <synch.h>
#endif	/* _ASM */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM
typedef unsigned int thread_t;
typedef unsigned int thread_key_t;

extern int thr_create(void *, size_t, void *(*)(void *), void *, long,
			thread_t *);
extern int thr_join(thread_t, thread_t *, void **);
extern int thr_setconcurrency(int);
extern int thr_getconcurrency(void);
extern void thr_exit(void *) __NORETURN;
extern thread_t thr_self(void);

/*
 * the definition of thr_sigsetmask() is not strict ansi-c since sigset_t is
 * not in the strict ansi-c name space. Hence, include the prototype for
 * thr_sigsetmask() only if strict ansi-c conformance is not turned on.
 */
#if !defined(_STRICT_STDC) || defined(__EXTENSIONS__)
extern int thr_sigsetmask(int, const sigset_t *, sigset_t *);
#endif

/*
 * the definition of thr_stksegment() is not strict ansi-c since stack_t is
 * not in the strict ansi-c name space. Hence, include the prototype for
 * thr_stksegment() only if strict ansi-c conformance is not turned on.
 */
#if !defined(_STRICT_STDC) || defined(__EXTENSIONS__)
extern int thr_stksegment(stack_t *);
#endif

extern int thr_main(void);
extern int thr_kill(thread_t, int);
extern int thr_suspend(thread_t);
extern int thr_continue(thread_t);
extern void thr_yield(void);
extern int thr_setprio(thread_t, int);
extern int thr_getprio(thread_t, int *);
extern int thr_keycreate(thread_key_t *, void(*)(void *));
extern int thr_keycreate_once(thread_key_t *, void(*)(void *));
extern int thr_setspecific(thread_key_t, void *);
extern int thr_getspecific(thread_key_t, void **);
extern size_t thr_min_stack(void);

#endif /* _ASM */

#define	THR_MIN_STACK	thr_min_stack()
/*
 * thread flags (one word bit mask)
 */
/*
 * POSIX.1c Note:
 * THR_BOUND is defined same as PTHREAD_SCOPE_SYSTEM in <pthread.h>
 * THR_DETACHED is defined same as PTHREAD_CREATE_DETACHED in <pthread.h>
 * Any changes in these definitions should be reflected in <pthread.h>
 */
#define	THR_BOUND		0x00000001	/* = PTHREAD_SCOPE_SYSTEM */
#define	THR_NEW_LWP		0x00000002
#define	THR_DETACHED		0x00000040	/* = PTHREAD_CREATE_DETACHED */
#define	THR_SUSPENDED		0x00000080
#define	THR_DAEMON		0x00000100

/*
 * The key to be created by thr_keycreate_once()
 * must be statically initialized with THR_ONCE_KEY.
 * This must be the same as PTHREAD_ONCE_KEY_NP in <pthread.h>
 */
#define	THR_ONCE_KEY	(thread_key_t)(-1)

/*
 * The available register states returned by thr_getstate().
 */
#define	TRS_VALID	0
#define	TRS_NONVOLATILE	1
#define	TRS_LWPID	2
#define	TRS_INVALID	3

#ifdef __cplusplus
}
#endif

#endif	/* _THREAD_H */
