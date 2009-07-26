/*-
 * Copyright (c) 2006-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef	_SYS_VIMAGE_H_
#define	_SYS_VIMAGE_H_

/*
 * struct vnet describes a virtualized network stack, and is primarily a
 * pointer to storage for virtualized global variables.  Expose to userspace
 * as required for libkvm.
 */
#if defined(_KERNEL) || defined(_WANT_VNET)
#include <sys/queue.h>

struct vnet {
	LIST_ENTRY(vnet)	 vnet_le;	/* all vnets list */
	u_int			 vnet_magic_n;
	u_int			 vnet_ifcnt;
	u_int			 vnet_sockcnt;
	void			*vnet_data_mem;
	uintptr_t		 vnet_data_base;
};

#define	VNET_MAGIC_N 0x3e0d8f29
#endif

#ifdef _KERNEL

#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/proc.h>

#ifdef INVARIANTS
#define	VNET_DEBUG
#endif

#ifdef VIMAGE

struct vnet;
struct ifnet;
struct vnet *vnet_alloc(void);
void	vnet_destroy(struct vnet *);
void	vnet_foreach(void (*vnet_foreach_fn)(struct vnet *, void *),
	    void *arg);

#endif /* VIMAGE */

#define	curvnet curthread->td_vnet

#ifdef VIMAGE
#ifdef VNET_DEBUG
#define	VNET_ASSERT(condition)						\
	if (!(condition)) {						\
		printf("VNET_ASSERT @ %s:%d %s():\n",			\
			__FILE__, __LINE__, __FUNCTION__);		\
		panic(#condition);					\
	}

#define	CURVNET_SET_QUIET(arg)						\
	VNET_ASSERT((arg)->vnet_magic_n == VNET_MAGIC_N);		\
	struct vnet *saved_vnet = curvnet;				\
	const char *saved_vnet_lpush = curthread->td_vnet_lpush;	\
	curvnet = arg;							\
	curthread->td_vnet_lpush = __FUNCTION__;
 
#define	CURVNET_SET_VERBOSE(arg)					\
	CURVNET_SET_QUIET(arg)						\
	if (saved_vnet)							\
		printf("CURVNET_SET(%p) in %s() on cpu %d, prev %p in %s()\n", \
		       curvnet,	curthread->td_vnet_lpush, curcpu,	\
		       saved_vnet, saved_vnet_lpush);

#define	CURVNET_SET(arg)	CURVNET_SET_VERBOSE(arg)
 
#define	CURVNET_RESTORE()						\
	VNET_ASSERT(saved_vnet == NULL ||				\
		    saved_vnet->vnet_magic_n == VNET_MAGIC_N);		\
	curvnet = saved_vnet;						\
	curthread->td_vnet_lpush = saved_vnet_lpush;
#else /* !VNET_DEBUG */
#define	VNET_ASSERT(condition)

#define	CURVNET_SET(arg)						\
	struct vnet *saved_vnet = curvnet;				\
	curvnet = arg;	
 
#define	CURVNET_SET_VERBOSE(arg)	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)		CURVNET_SET(arg)
 
#define	CURVNET_RESTORE()						\
	curvnet = saved_vnet;
#endif /* VNET_DEBUG */
#else /* !VIMAGE */
#define	VNET_ASSERT(condition)
#define	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)
#define	CURVNET_RESTORE()
#endif /* !VIMAGE */

#ifdef VIMAGE
/*
 * Global linked list of all virtual network stacks, along with read locks to
 * access it.  If a caller may sleep while accessing the list, it must use
 * the sleepable lock macros.
 */
LIST_HEAD(vnet_list_head, vnet);
extern struct vnet_list_head vnet_head;
extern struct rwlock vnet_rwlock;
extern struct sx vnet_sxlock;

#define	VNET_LIST_RLOCK()		sx_slock(&vnet_sxlock)
#define	VNET_LIST_RLOCK_NOSLEEP()	rw_rlock(&vnet_rwlock)
#define	VNET_LIST_RUNLOCK()		sx_sunlock(&vnet_sxlock)
#define	VNET_LIST_RUNLOCK_NOSLEEP()	rw_runlock(&vnet_rwlock)

/*
 * Iteration macros to walk the global list of virtual network stacks.
 */
#define	VNET_ITERATOR_DECL(arg) struct vnet *arg
#define	VNET_FOREACH(arg)	LIST_FOREACH((arg), &vnet_head, vnet_le)

#else /* !VIMAGE */
/*
 * No-op macros for the !VIMAGE case.
 */
#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RLOCK_NOSLEEP()
#define	VNET_LIST_RUNLOCK()
#define	VNET_LIST_RUNLOCK_NOSLEEP()
#define	VNET_ITERATOR_DECL(arg)
#define	VNET_FOREACH(arg)

#endif /* VIMAGE */

#ifdef VIMAGE
extern struct vnet *vnet0;
#define	IS_DEFAULT_VNET(arg)	((arg) == vnet0)
#else
#define	IS_DEFAULT_VNET(arg)	1
#endif

#ifdef VIMAGE
#define	CRED_TO_VNET(cr)	(cr)->cr_prison->pr_vnet
#define	TD_TO_VNET(td)		CRED_TO_VNET((td)->td_ucred)
#define	P_TO_VNET(p)		CRED_TO_VNET((p)->p_ucred)
#else /* !VIMAGE */
#define	CRED_TO_VNET(cr)	NULL
#define	TD_TO_VNET(td)		NULL
#define	P_TO_VNET(p)		NULL
#endif /* VIMAGE */

#endif /* _KERNEL */

#endif /* !_SYS_VIMAGE_H_ */
