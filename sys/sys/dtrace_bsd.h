/*-
 * Copyright (c) 2007-2008 John Birrell (jb@freebsd.org)
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
 * $FreeBSD: src/sys/sys/dtrace_bsd.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 * This file contains BSD shims for Sun's DTrace code.
 */

#ifndef _SYS_DTRACE_BSD_H
#define	_SYS_DTRACE_BSD_H

/* Forward definitions: */
struct trapframe;
struct thread;

/*
 * Cyclic clock function type definition used to hook the cyclic
 * subsystem into the appropriate timer interrupt.
 */
typedef	void (*cyclic_clock_func_t)(struct trapframe *);

/*
 * These external variables are actually machine-dependent, so
 * they might not actually exist.
 *
 * Defining them here avoids a proliferation of header files.
 */
extern cyclic_clock_func_t     lapic_cyclic_clock_func[];

/*
 * The dtrace module handles traps that occur during a DTrace probe.
 * This type definition is used in the trap handler to provide a
 * hook for the dtrace module to register it's handler with.
 */
typedef int (*dtrace_trap_func_t)(struct trapframe *, u_int);

int	dtrace_trap(struct trapframe *, u_int);

extern dtrace_trap_func_t	dtrace_trap_func;

/* Used by the machine dependent trap() code. */
typedef	int (*dtrace_invop_func_t)(uintptr_t, uintptr_t *, uintptr_t);
typedef void (*dtrace_doubletrap_func_t)(void);

/* Global variables in trap.c */
extern	dtrace_invop_func_t	dtrace_invop_func;
extern	dtrace_doubletrap_func_t	dtrace_doubletrap_func;

/* Virtual time hook function type. */
typedef	void (*dtrace_vtime_switch_func_t)(struct thread *);

extern int			dtrace_vtime_active;
extern dtrace_vtime_switch_func_t	dtrace_vtime_switch_func;

/* The fasttrap module hooks into the fork, exit and exit. */
typedef void (*dtrace_fork_func_t)(struct proc *, struct proc *);
typedef void (*dtrace_execexit_func_t)(struct proc *);

/* Global variable in kern_fork.c */
extern dtrace_fork_func_t	dtrace_fasttrap_fork;

/* Global variable in kern_exec.c */
extern dtrace_execexit_func_t	dtrace_fasttrap_exec;

/* Global variable in kern_exit.c */
extern dtrace_execexit_func_t	dtrace_fasttrap_exit;

/* The dtmalloc provider hooks into malloc. */
typedef	void (*dtrace_malloc_probe_func_t)(u_int32_t, uintptr_t arg0,
    uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

extern dtrace_malloc_probe_func_t   dtrace_malloc_probe;

/*
 * Functions which allow the dtrace module to check that the kernel 
 * hooks have been compiled with sufficient space for it's private
 * structures.
 */
size_t	kdtrace_proc_size(void);
size_t	kdtrace_thread_size(void);

/*
 * OpenSolaris compatible time functions returning nanoseconds.
 * On OpenSolaris these return hrtime_t which we define as uint64_t.
 */
uint64_t	dtrace_gethrtime(void);
uint64_t	dtrace_gethrestime(void);

#endif /* _SYS_DTRACE_BSD_H */
