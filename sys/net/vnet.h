/*-
 * Copyright (c) 2006-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
 *
 * Copyright (c) 2009 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2009 Robert N. M. Watson
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

/*-
 * This header file defines several sets of interfaces supporting virtualized
 * network stacks:
 *
 * - Definition of 'struct vnet' and functions and macros to allocate/free/
 *   manipulate it.
 *
 * - A virtual network stack memory allocator, which provides support for
 *   virtualized global variables via a special linker set, set_vnet.
 *
 * - Virtualized sysinits/sysuninits, which allow constructors and
 *   destructors to be run for each network stack subsystem as virtual
 *   instances are created and destroyed.
 *
 * If VIMAGE isn't compiled into the kernel, virtualized global variables
 * compile to normal global variables, and virtualized sysinits to regular
 * sysinits.
 */

#ifndef _NET_VNET_H_
#define	_NET_VNET_H_

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
#define	VNET_MAGIC_N	0x3e0d8f29

/*
 * These two virtual network stack allocator definitions are also required
 * for libkvm so that it can evaluate virtualized global variables.
 */
#define	VNET_SETNAME		"set_vnet"
#define	VNET_SYMPREFIX		"vnet_entry_"
#endif

#ifdef _KERNEL

#ifdef VIMAGE
#include <sys/lock.h>
#include <sys/proc.h>			/* for struct thread */
#include <sys/rwlock.h>
#include <sys/sx.h>

/*
 * Location of the kernel's 'set_vnet' linker set.
 */
extern uintptr_t	*__start_set_vnet;
__GLOBL(__start_set_vnet);
extern uintptr_t	*__stop_set_vnet;
__GLOBL(__stop_set_vnet);

#define	VNET_START	(uintptr_t)&__start_set_vnet
#define	VNET_STOP	(uintptr_t)&__stop_set_vnet

/*
 * Functions to allocate and destroy virtual network stacks.
 */
struct vnet *vnet_alloc(void);
void	vnet_destroy(struct vnet *vnet);

/*
 * The current virtual network stack -- we may wish to move this to struct
 * pcpu in the future.
 */
#define	curvnet	curthread->td_vnet

/*
 * Various macros -- get and set the current network stack, but also
 * assertions.
 */
#ifdef VNET_DEBUG
void vnet_log_recursion(struct vnet *, const char *, int);

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
		vnet_log_recursion(saved_vnet, saved_vnet_lpush, __LINE__);

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

extern struct vnet *vnet0;
#define	IS_DEFAULT_VNET(arg)	((arg) == vnet0)

#define	CRED_TO_VNET(cr)	(cr)->cr_prison->pr_vnet
#define	TD_TO_VNET(td)		CRED_TO_VNET((td)->td_ucred)
#define	P_TO_VNET(p)		CRED_TO_VNET((p)->p_ucred)

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
#define	VNET_ITERATOR_DECL(arg)	struct vnet *arg
#define	VNET_FOREACH(arg)	LIST_FOREACH((arg), &vnet_head, vnet_le)

/*
 * Virtual network stack memory allocator, which allows global variables to
 * be automatically instantiated for each network stack instance.
 */
#define	VNET_NAME(n)		vnet_entry_##n
#define	VNET_DECLARE(t, n)	extern t VNET_NAME(n)
#define	VNET_DEFINE(t, n)	t VNET_NAME(n) __section(VNET_SETNAME) __used
#define	_VNET_PTR(b, n)		(__typeof(VNET_NAME(n))*)		\
				    ((b) + (uintptr_t)&VNET_NAME(n))

#define	_VNET(b, n)		(*_VNET_PTR(b, n))

/*
 * Virtualized global variable accessor macros.
 */
#define	VNET_VNET_PTR(vnet, n)		_VNET_PTR((vnet)->vnet_data_base, n)
#define	VNET_VNET(vnet, n)		(*VNET_VNET_PTR((vnet), n))

#define	VNET_PTR(n)		VNET_VNET_PTR(curvnet, n)
#define	VNET(n)			VNET_VNET(curvnet, n)

/*
 * Virtual network stack allocator interfaces from the kernel linker.
 */
void	*vnet_data_alloc(int size);
void	 vnet_data_copy(void *start, int size);
void	 vnet_data_free(void *start_arg, int size);

/*
 * Sysctl variants for vnet-virtualized global variables.  Include
 * <sys/sysctl.h> to expose these definitions.
 *
 * Note: SYSCTL_PROC() handler functions will need to resolve pointer
 * arguments themselves, if required.
 */
#ifdef SYSCTL_OID
int	vnet_sysctl_handle_int(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_opaque(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_string(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_uint(SYSCTL_HANDLER_ARGS);

#define	SYSCTL_VNET_INT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_OID(parent, nbr, name,					\
	    CTLTYPE_INT|CTLFLAG_MPSAFE|CTLFLAG_VNET|(access),		\
	    ptr, val, vnet_sysctl_handle_int, "I", descr)
#define	SYSCTL_VNET_PROC(parent, nbr, name, access, ptr, arg, handler,	\
	    fmt, descr)							\
	CTASSERT(((access) & CTLTYPE) != 0);				\
	SYSCTL_OID(parent, nbr, name, CTLFLAG_VNET|(access), ptr, arg, 	\
	    handler, fmt, descr)
#define	SYSCTL_VNET_OPAQUE(parent, nbr, name, access, ptr, len, fmt,    \
	    descr)							\
	SYSCTL_OID(parent, nbr, name,					\
	    CTLTYPE_OPAQUE|CTLFLAG_VNET|(access), ptr, len, 		\
	    vnet_sysctl_handle_opaque, fmt, descr)
#define	SYSCTL_VNET_STRING(parent, nbr, name, access, arg, len, descr)	\
	SYSCTL_OID(parent, nbr, name,					\
	    CTLTYPE_STRING|CTLFLAG_VNET|(access),			\
	    arg, len, vnet_sysctl_handle_string, "A", descr)
#define	SYSCTL_VNET_STRUCT(parent, nbr, name, access, ptr, type, descr)	\
	SYSCTL_OID(parent, nbr, name,					\
	    CTLTYPE_OPAQUE|CTLFLAG_VNET|(access), ptr,			\
	    sizeof(struct type), vnet_sysctl_handle_opaque, "S," #type,	\
	    descr)
#define	SYSCTL_VNET_UINT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_OID(parent, nbr, name,					\
	    CTLTYPE_UINT|CTLFLAG_MPSAFE|CTLFLAG_VNET|(access),		\
	    ptr, val, vnet_sysctl_handle_uint, "IU", descr)
#define	VNET_SYSCTL_ARG(req, arg1) do {					\
	if (arg1 != NULL)						\
		arg1 = (void *)(TD_TO_VNET((req)->td)->vnet_data_base +	\
		    (uintptr_t)(arg1));					\
} while (0)
#endif /* SYSCTL_OID */

/*
 * Virtual sysinit mechanism, allowing network stack components to declare
 * startup and shutdown methods to be run when virtual network stack
 * instances are created and destroyed.
 */
#include <sys/kernel.h>

/*
 * SYSINIT/SYSUNINIT variants that provide per-vnet constructors and
 * destructors.
 */
struct vnet_sysinit {
	enum sysinit_sub_id	subsystem;
	enum sysinit_elem_order	order;
	sysinit_cfunc_t		func;
	const void		*arg;
	TAILQ_ENTRY(vnet_sysinit) link;
};

#define	VNET_SYSINIT(ident, subsystem, order, func, arg)		\
	static struct vnet_sysinit ident ## _vnet_init = {		\
		subsystem,						\
		order,							\
		(sysinit_cfunc_t)(sysinit_nfunc_t)func,			\
		(arg)							\
	};								\
	SYSINIT(vnet_init_ ## ident, subsystem, order,			\
	    vnet_register_sysinit, &ident ## _vnet_init);		\
	SYSUNINIT(vnet_init_ ## ident, subsystem, order,		\
	    vnet_deregister_sysinit, &ident ## _vnet_init)

#define	VNET_SYSUNINIT(ident, subsystem, order, func, arg)		\
	static struct vnet_sysinit ident ## _vnet_uninit = {		\
		subsystem,						\
		order,							\
		(sysinit_cfunc_t)(sysinit_nfunc_t)func,			\
		(arg)							\
	};								\
	SYSINIT(vnet_uninit_ ## ident, subsystem, order,		\
	    vnet_register_sysuninit, &ident ## _vnet_uninit);		\
	SYSUNINIT(vnet_uninit_ ## ident, subsystem, order,		\
	    vnet_deregister_sysuninit, &ident ## _vnet_uninit)

/*
 * Run per-vnet sysinits or sysuninits during vnet creation/destruction.
 */
void	 vnet_sysinit(void);
void	 vnet_sysuninit(void);

/*
 * Interfaces for managing per-vnet constructors and destructors.
 */
void	vnet_register_sysinit(void *arg);
void	vnet_register_sysuninit(void *arg);
void	vnet_deregister_sysinit(void *arg);
void	vnet_deregister_sysuninit(void *arg);

/*
 * EVENTHANDLER(9) extensions.
 */
#include <sys/eventhandler.h>

void	vnet_global_eventhandler_iterator_func(void *, ...);
#define VNET_GLOBAL_EVENTHANDLER_REGISTER_TAG(tag, name, func, arg, priority) \
do {									\
	if (IS_DEFAULT_VNET(curvnet)) {					\
		(tag) = vimage_eventhandler_register(NULL, #name, func,	\
		    arg, priority,					\
		    vnet_global_eventhandler_iterator_func);		\
	}								\
} while(0)
#define VNET_GLOBAL_EVENTHANDLER_REGISTER(name, func, arg, priority)	\
do {									\
	if (IS_DEFAULT_VNET(curvnet)) {					\
		vimage_eventhandler_register(NULL, #name, func,		\
		    arg, priority,					\
		    vnet_global_eventhandler_iterator_func);		\
	}								\
} while(0)

#else /* !VIMAGE */

/*
 * Various virtual network stack macros compile to no-ops without VIMAGE.
 */
#define	curvnet			NULL

#define	VNET_ASSERT(condition)
#define	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)
#define	CURVNET_RESTORE()

#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RLOCK_NOSLEEP()
#define	VNET_LIST_RUNLOCK()
#define	VNET_LIST_RUNLOCK_NOSLEEP()
#define	VNET_ITERATOR_DECL(arg)
#define	VNET_FOREACH(arg)

#define	IS_DEFAULT_VNET(arg)	1
#define	CRED_TO_VNET(cr)	NULL
#define	TD_TO_VNET(td)		NULL
#define	P_TO_VNET(p)		NULL

/*
 * Versions of the VNET macros that compile to normal global variables and
 * standard sysctl definitions.
 */
#define	VNET_NAME(n)		n
#define	VNET_DECLARE(t, n)	extern t n
#define	VNET_DEFINE(t, n)	t n
#define	_VNET_PTR(b, n)		&VNET_NAME(n)

/*
 * Virtualized global variable accessor macros.
 */
#define	VNET_VNET_PTR(vnet, n)		(&(n))
#define	VNET_VNET(vnet, n)		(n)

#define	VNET_PTR(n)		(&(n))
#define	VNET(n)			(n)

/*
 * When VIMAGE isn't compiled into the kernel, virtaulized SYSCTLs simply
 * become normal SYSCTLs.
 */
#ifdef SYSCTL_OID
#define	SYSCTL_VNET_INT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_INT(parent, nbr, name, access, ptr, val, descr)
#define	SYSCTL_VNET_PROC(parent, nbr, name, access, ptr, arg, handler,	\
	    fmt, descr)							\
	SYSCTL_PROC(parent, nbr, name, access, ptr, arg, handler, fmt,	\
	    descr)
#define	SYSCTL_VNET_OPAQUE(parent, nbr, name, access, ptr, len, fmt,    \
	    descr)							\
	SYSCTL_OPAQUE(parent, nbr, name, access, ptr, len, fmt, descr)
#define	SYSCTL_VNET_STRING(parent, nbr, name, access, arg, len, descr)	\
	SYSCTL_STRING(parent, nbr, name, access, arg, len, descr)
#define	SYSCTL_VNET_STRUCT(parent, nbr, name, access, ptr, type, descr)	\
	SYSCTL_STRUCT(parent, nbr, name, access, ptr, type, descr)
#define	SYSCTL_VNET_UINT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr)
#define	VNET_SYSCTL_ARG(req, arg1)
#endif /* SYSCTL_OID */

/*
 * When VIMAGE isn't compiled into the kernel, VNET_SYSINIT/VNET_SYSUNINIT
 * map into normal sysinits, which have the same ordering properties.
 */
#define	VNET_SYSINIT(ident, subsystem, order, func, arg)		\
	SYSINIT(ident, subsystem, order, func, arg)
#define	VNET_SYSUNINIT(ident, subsystem, order, func, arg)		\
	SYSUNINIT(ident, subsystem, order, func, arg)

/*
 * Without VIMAGE revert to the default implementation.
 */
#define VNET_GLOBAL_EVENTHANDLER_REGISTER_TAG(tag, name, func, arg, priority) \
	(tag) = eventhandler_register(NULL, #name, func, arg, priority)
#define VNET_GLOBAL_EVENTHANDLER_REGISTER(name, func, arg, priority)	\
	eventhandler_register(NULL, #name, func, arg, priority)
#endif /* VIMAGE */
#endif /* _KERNEL */

#endif /* !_NET_VNET_H_ */
