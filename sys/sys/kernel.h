/*-
 * Copyright (c) 1995 Terrence R. Lambert
 * All rights reserved.
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *
 *	@(#)kernel.h	8.3 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#ifndef _SYS_KERNEL_H_
#define	_SYS_KERNEL_H_

#include <sys/linker_set.h>

#ifdef _KERNEL

/* for intrhook below */
#include <sys/queue.h>

/* THIS MUST DIE! */
#include <sys/module.h>

/* Global variables for the kernel. */

/* 1.1 */
extern unsigned long hostid;
extern char hostname[MAXHOSTNAMELEN];
extern int hostnamelen;
extern char domainname[MAXHOSTNAMELEN];
extern int domainnamelen;
extern char kernelname[MAXPATHLEN];

/* 1.2 */
extern struct timeval boottime;

extern struct timezone tz;			/* XXX */

extern int tick;			/* usec per tick (1000000 / hz) */
extern int tickadj;			/* "standard" clock skew, us./tick */
extern int hz;				/* system clock's frequency */
extern int psratio;			/* ratio: prof / stat */
extern int stathz;			/* statistics clock's frequency */
extern int profhz;			/* profiling clock's frequency */
extern int ticks;
extern int lbolt;			/* once a second sleep address */
extern int tickdelta;
extern long timedelta;

#endif /* _KERNEL */

/*
 * Enumerated types for known system startup interfaces.
 *
 * Startup occurs in ascending numeric order; the list entries are
 * sorted prior to attempting startup to guarantee order.  Items
 * of the same level are arbitrated for order based on the 'order'
 * element.
 *
 * These numbers are arbitrary and are chosen ONLY for ordering; the
 * enumeration values are explicit rather than implicit to provide
 * for binary compatibility with inserted elements.
 *
 * The SI_SUB_RUN_SCHEDULER value must have the highest lexical value.
 *
 * The SI_SUB_CONSOLE and SI_SUB_SWAP values represent values used by
 * the BSD 4.4Lite but not by FreeBSD; they are maintained in dependent
 * order to support porting.
 *
 * The SI_SUB_PROTO_BEGIN and SI_SUB_PROTO_END bracket a range of
 * initializations to take place at splimp().  This is a historical
 * wart that should be removed -- probably running everything at
 * splimp() until the first init that doesn't want it is the correct
 * fix.  They are currently present to ensure historical behavior.
 */
enum sysinit_sub_id {
	SI_SUB_DUMMY		= 0x0000000,	/* not executed; for linker*/
	SI_SUB_DONE		= 0x0000001,	/* processed*/
	SI_SUB_TUNABLES		= 0x0700000,	/* establish tunable values */
	SI_SUB_CONSOLE		= 0x0800000,	/* console*/
	SI_SUB_COPYRIGHT	= 0x0800001,	/* first use of console*/
	SI_SUB_MTX_POOL		= 0x0900000,	/* mutex pool */
	SI_SUB_VM		= 0x1000000,	/* virtual memory system init*/
	SI_SUB_KMEM		= 0x1800000,	/* kernel memory*/
	SI_SUB_KVM_RSRC		= 0x1A00000,	/* kvm operational limits*/
	SI_SUB_WITNESS		= 0x1A80000,	/* witness initialization */
	SI_SUB_LOCK		= 0x1B00000,	/* lockmgr locks */
	SI_SUB_EVENTHANDLER	= 0x1C00000,	/* eventhandler init */
	SI_SUB_KLD		= 0x2000000,	/* KLD and module setup */
	SI_SUB_CPU		= 0x2100000,	/* CPU resource(s)*/
	SI_SUB_MAC		= 0x2180000,	/* TrustedBSD MAC subsystem */
	SI_SUB_MAC_POLICY	= 0x21C0000,	/* TrustedBSD MAC policies */
	SI_SUB_INTRINSIC	= 0x2200000,	/* proc 0*/
	SI_SUB_VM_CONF		= 0x2300000,	/* config VM, set limits*/
	SI_SUB_RUN_QUEUE	= 0x2400000,	/* set up run queue*/
	SI_SUB_CREATE_INIT	= 0x2500000,	/* create init process*/
	SI_SUB_SCHED_IDLE	= 0x2600000,	/* required idle procs */
	SI_SUB_MBUF		= 0x2700000,	/* mbuf subsystem */
	SI_SUB_INTR		= 0x2800000,	/* interrupt threads */
	SI_SUB_SOFTINTR		= 0x2800001,	/* start soft interrupt thread */
	SI_SUB_DEVFS		= 0x2F00000,	/* devfs ready for devices */
	SI_SUB_INIT_IF		= 0x3000000,	/* prep for net interfaces */
	SI_SUB_DRIVERS		= 0x3100000,	/* Let Drivers initialize */
	SI_SUB_CONFIGURE	= 0x3800000,	/* Configure devices */
	SI_SUB_VFS		= 0x4000000,	/* virtual file system*/
	SI_SUB_CLOCKS		= 0x4800000,	/* real time and stat clocks*/
	SI_SUB_CLIST		= 0x5800000,	/* clists*/
	SI_SUB_SYSV_SHM		= 0x6400000,	/* System V shared memory*/
	SI_SUB_SYSV_SEM		= 0x6800000,	/* System V semaphores*/
	SI_SUB_SYSV_MSG		= 0x6C00000,	/* System V message queues*/
	SI_SUB_P1003_1B		= 0x6E00000,	/* P1003.1B realtime */
	SI_SUB_PSEUDO		= 0x7000000,	/* pseudo devices*/
	SI_SUB_EXEC		= 0x7400000,	/* execve() handlers */
	SI_SUB_PROTO_BEGIN	= 0x8000000,	/* XXX: set splimp (kludge)*/
	SI_SUB_PROTO_IF		= 0x8400000,	/* interfaces*/
	SI_SUB_PROTO_DOMAIN	= 0x8800000,	/* domains (address families?)*/
	SI_SUB_PROTO_END	= 0x8ffffff,	/* XXX: set splx (kludge)*/
	SI_SUB_KPROF		= 0x9000000,	/* kernel profiling*/
	SI_SUB_KICK_SCHEDULER	= 0xa000000,	/* start the timeout events*/
	SI_SUB_INT_CONFIG_HOOKS	= 0xa800000,	/* Interrupts enabled config */
	SI_SUB_ROOT_CONF	= 0xb000000,	/* Find root devices */
	SI_SUB_DUMP_CONF	= 0xb200000,	/* Find dump devices */
	SI_SUB_VINUM		= 0xb300000,	/* Configure vinum */
	SI_SUB_MOUNT_ROOT	= 0xb400000,	/* root mount*/
	SI_SUB_SWAP		= 0xc000000,	/* swap*/
	SI_SUB_INTRINSIC_POST	= 0xd000000,	/* proc 0 cleanup*/
	SI_SUB_KTHREAD_INIT	= 0xe000000,	/* init process*/
	SI_SUB_KTHREAD_PAGE	= 0xe400000,	/* pageout daemon*/
	SI_SUB_KTHREAD_VM	= 0xe800000,	/* vm daemon*/
	SI_SUB_KTHREAD_BUF	= 0xea00000,	/* buffer daemon*/
	SI_SUB_KTHREAD_UPDATE	= 0xec00000,	/* update daemon*/
	SI_SUB_KTHREAD_IDLE	= 0xee00000,	/* idle procs*/
	SI_SUB_SMP		= 0xf000000,	/* start the APs*/
	SI_SUB_RUN_SCHEDULER	= 0xfffffff	/* scheduler*/
};


/*
 * Some enumerated orders; "ANY" sorts last.
 */
enum sysinit_elem_order {
	SI_ORDER_FIRST		= 0x0000000,	/* first*/
	SI_ORDER_SECOND		= 0x0000001,	/* second*/
	SI_ORDER_THIRD		= 0x0000002,	/* third*/
	SI_ORDER_MIDDLE		= 0x1000000,	/* somewhere in the middle */
	SI_ORDER_ANY		= 0xfffffff	/* last*/
};


/*
 * A system initialization call instance
 *
 * At the moment there is one instance of sysinit.  We probably do not
 * want two which is why this code is if'd out, but we definitely want
 * to discern SYSINIT's which take non-constant data pointers and
 * SYSINIT's which take constant data pointers,
 *
 * The C_* macros take functions expecting const void * arguments 
 * while the non-C_* macros take functions expecting just void * arguments.
 *
 * With -Wcast-qual on, the compiler issues warnings:
 *	- if we pass non-const data or functions taking non-const data
 *	  to a C_* macro.
 *
 *	- if we pass const data to the normal macros
 *
 * However, no warning is issued if we pass a function taking const data
 * through a normal non-const macro.  This is ok because the function is
 * saying it won't modify the data so we don't care whether the data is
 * modifiable or not.
 */

typedef void (*sysinit_nfunc_t) __P((void *));
typedef void (*sysinit_cfunc_t) __P((const void *));

struct sysinit {
	enum sysinit_sub_id	subsystem;	/* subsystem identifier*/
	enum sysinit_elem_order	order;		/* init order within subsystem*/
	sysinit_cfunc_t func;			/* function		*/
	const void	*udata;			/* multiplexer/argument */
};

/*
 * Default: no special processing
 *
 * The C_ version of SYSINIT is for data pointers to const
 * data ( and functions taking data pointers to const data ).
 * At the moment it is no different from SYSINIT and thus
 * still results in warnings.
 *
 * The casts are necessary to have the compiler produce the
 * correct warnings when -Wcast-qual is used.
 *
 */
#define	C_SYSINIT(uniquifier, subsystem, order, func, ident)	\
	static struct sysinit uniquifier ## _sys_init = {	\
		subsystem,					\
		order,						\
		func,						\
		ident						\
	};							\
	DATA_SET(sysinit_set,uniquifier ## _sys_init);

#define	SYSINIT(uniquifier, subsystem, order, func, ident)	\
	C_SYSINIT(uniquifier, subsystem, order,			\
	(sysinit_cfunc_t)(sysinit_nfunc_t)func, (void *)ident)

/*
 * Called on module unload: no special processing
 */
#define	C_SYSUNINIT(uniquifier, subsystem, order, func, ident)	\
	static struct sysinit uniquifier ## _sys_uninit = {	\
		subsystem,					\
		order,						\
		func,						\
		ident						\
	};							\
	DATA_SET(sysuninit_set,uniquifier ## _sys_uninit)

#define	SYSUNINIT(uniquifier, subsystem, order, func, ident)	\
	C_SYSUNINIT(uniquifier, subsystem, order,		\
	(sysinit_cfunc_t)(sysinit_nfunc_t)func, (void *)ident)

void	sysinit_add __P((struct sysinit **set, struct sysinit **set_end));

/*
 * Infrastructure for tunable 'constants'.  Value may be specified at compile
 * time or kernel load time.  Rules relating tunables together can be placed
 * in a SYSINIT function at SI_SUB_TUNABLES with SI_ORDER_LAST.
 */

extern void tunable_int_init(void *);
struct tunable_int {
	const char *path;
	int *var;
};
#define	TUNABLE_INT(path, var)					\
	_TUNABLE_INT((path), (var), __LINE__)
#define _TUNABLE_INT(path, var, line)				\
	__TUNABLE_INT((path), (var), line)

#define	__TUNABLE_INT(path, var, line)				\
	static struct tunable_int __tunable_int_ ## line = {	\
		path,						\
		var,						\
	};							\
	SYSINIT(__Tunable_init_ ## line, SI_SUB_TUNABLES, SI_ORDER_MIDDLE, \
	     tunable_int_init, &__tunable_int_ ## line)

#define	TUNABLE_INT_FETCH(path, var)	getenv_int((path), (var))

extern void tunable_quad_init(void *);
struct tunable_quad {
	const char *path;
	quad_t *var;
};
#define	TUNABLE_QUAD(path, var)					\
	_TUNABLE_QUAD((path), (var), __LINE__)
#define	_TUNABLE_QUAD(path, var, line)				\
	__TUNABLE_QUAD((path), (var), line)

#define	__TUNABLE_QUAD(path, var, line)			\
	static struct tunable_quad __tunable_quad_ ## line = {	\
		path,						\
		var,						\
	};							\
	SYSINIT(__Tunable_init_ ## line, SI_SUB_TUNABLES, SI_ORDER_MIDDLE, \
	    tunable_quad_init, &__tunable_quad_ ## line)

#define	TUNABLE_QUAD_FETCH(path, var)	getenv_quad((path), (var))

extern void tunable_str_init(void *);
struct tunable_str {
	const char *path;
	char *var;
	int size;
};
#define	TUNABLE_STR(path, var, size)				\
	_TUNABLE_STR((path), (var), (size), __LINE__)
#define	_TUNABLE_STR(path, var, size, line)			\
	__TUNABLE_STR((path), (var), (size), line)

#define	__TUNABLE_STR(path, var, size, line)			\
	static struct tunable_str __tunable_str_ ## line = {	\
		path,						\
		var,						\
		size,						\
	};							\
	SYSINIT(__Tunable_init_ ## line, SI_SUB_TUNABLES, SI_ORDER_MIDDLE, \
	     tunable_str_init, &__tunable_str_ ## line)

#define	TUNABLE_STR_FETCH(path, var, size)			\
	getenv_string((path), (var), (size))

struct intr_config_hook {
	TAILQ_ENTRY(intr_config_hook) ich_links;
	void	(*ich_func) __P((void *arg));
	void	*ich_arg;
};

int	config_intrhook_establish __P((struct intr_config_hook *hook));
void	config_intrhook_disestablish __P((struct intr_config_hook *hook));

#endif /* !_SYS_KERNEL_H_*/
