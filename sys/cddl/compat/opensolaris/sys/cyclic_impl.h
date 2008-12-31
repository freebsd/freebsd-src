/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/sys/cyclic_impl.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _COMPAT_OPENSOLARIS_SYS_CYCLIC_IMPL_H_
#define _COMPAT_OPENSOLARIS_SYS_CYCLIC_IMPL_H_

#include <sys/cyclic.h>

/*
 *  Cyclic Subsystem Backend-supplied Interfaces
 *  --------------------------------------------
 *
 *  0  Background
 *
 *    The design, implementation and interfaces of the cyclic subsystem are
 *    covered in detail in block comments in the implementation.  This
 *    comment covers the interface from the cyclic subsystem into the cyclic
 *    backend.  The backend is specified by a structure of function pointers
 *    defined below.
 *
 *  1  Overview
 *
 *      cyb_configure()      <-- Configures the backend on the specified CPU
 *      cyb_unconfigure()    <-- Unconfigures the backend
 *      cyb_enable()         <-- Enables the CY_HIGH_LEVEL interrupt source
 *      cyb_disable()        <-- Disables the CY_HIGH_LEVEL interrupt source
 *      cyb_reprogram()      <-- Reprograms the CY_HIGH_LEVEL interrupt source
 *      cyb_xcall()          <-- Cross calls to the specified CPU
 *
 *  2  cyb_arg_t cyb_configure(cpu_t *)
 *
 *  2.1  Overview
 *
 *    cyb_configure() should configure the specified CPU for cyclic operation.
 *
 *  2.2  Arguments and notes
 *
 *    cyb_configure() should initialize any backend-specific per-CPU
 *    structures for the specified CPU.  cyb_configure() will be called for
 *    each CPU (including the boot CPU) during boot.  If the platform
 *    supports dynamic reconfiguration, cyb_configure() will be called for
 *    new CPUs as they are configured into the system.
 *
 *  2.3  Return value
 *
 *    cyb_configure() is expected to return a cookie (a cyb_arg_t, which is
 *    of type void *) which will be used as the first argument for all future
 *    cyclic calls into the backend on the specified CPU.
 *
 *  2.4  Caller's context
 *
 *    cpu_lock will be held.  The caller's CPU is unspecified, and may or
 *    may not be the CPU specified to cyb_configure().
 *
 *  3  void cyb_unconfigure(cyb_arg_t arg)
 *
 *  3.1  Overview
 *
 *    cyb_unconfigure() should unconfigure the specified backend.
 *
 *  3.2  Arguments and notes
 *
 *    The only argument to cyb_unconfigure() is a cookie as returned from
 *    cyb_configure().
 *
 *    cyb_unconfigure() should free any backend-specific per-CPU structures
 *    for the specified backend.  cyb_unconfigure() will _only_ be called on
 *    platforms which support dynamic reconfiguration.  If the platform does
 *    not support dynamic reconfiguration, cyb_unconfigure() may panic.
 *
 *    After cyb_unconfigure() returns, the backend must not call cyclic_fire()
 *    on the corresponding CPU; doing so will result in a bad trap.
 *
 *  3.3  Return value
 *
 *    None.
 *
 *  3.4  Caller's context
 *
 *    cpu_lock will be held.  The caller's CPU is unspecified, and may or
 *    may not be the CPU specified to cyb_unconfigure().  The specified
 *    CPU is guaranteed to exist at the time cyb_unconfigure() is called.
 *    The cyclic subsystem is guaranteed to be suspended when cyb_unconfigure()
 *    is called, and interrupts are guaranteed to be disabled.
 *
 *  4  void cyb_enable(cyb_arg_t arg)
 *
 *  4.1  Overview
 *
 *    cyb_enable() should enable the CY_HIGH_LEVEL interrupt source on
 *    the specified backend.
 *
 *  4.2  Arguments and notes
 *
 *    The only argument to cyb_enable() is a backend cookie as returned from
 *    cyb_configure().
 *
 *    cyb_enable() will only be called if a) the specified backend has never
 *    been enabled or b) the specified backend has been explicitly disabled with
 *    cyb_disable().  In either case, cyb_enable() will only be called if
 *    the cyclic subsystem wishes to add a cyclic to the CPU corresponding
 *    to the specified backend.  cyb_enable() will be called before
 *    cyb_reprogram() for a given backend.
 *
 *    cyclic_fire() should not be called on a CPU which has not had its backend
 *    explicitly cyb_enable()'d, but to do so does not constitute fatal error.
 *
 *  4.3  Return value
 *
 *    None.
 *
 *  4.4  Caller's context
 *
 *    cyb_enable() will only be called from CY_HIGH_LEVEL context on the CPU
 *    corresponding to the specified backend.
 *
 *  5  void cyb_disable(cyb_arg_t arg)
 *
 *  5.1  Overview
 *
 *    cyb_disable() should disable the CY_HIGH_LEVEL interrupt source on
 *    the specified backend.
 *
 *  5.2  Arguments and notes
 *
 *    The only argument to cyb_disable() is a backend cookie as returned from
 *    cyb_configure().
 *
 *    cyb_disable() will only be called on backends which have been previously
 *    been cyb_enable()'d.  cyb_disable() will be called when all cyclics have
 *    been juggled away or removed from a cyb_enable()'d CPU.
 *
 *    cyclic_fire() should not be called on a CPU which has had its backend
 *    explicitly cyb_disable()'d, but to do so does not constitute fatal
 *    error.  cyb_disable() is thus not required to check for a pending
 *    CY_HIGH_LEVEL interrupt.
 *
 *  5.3  Return value
 *
 *    None.
 *
 *  5.4  Caller's context
 *
 *    cyb_disable() will only be called from CY_HIGH_LEVEL context on the CPU
 *    corresponding to the specified backend.
 *
 *  6  void cyb_reprogram(cyb_arg_t arg, hrtime_t time)
 *
 *  6.1  Overview
 *
 *    cyb_reprogram() should reprogram the CY_HIGH_LEVEL interrupt source
 *    to fire at the absolute time specified.
 *
 *  6.2  Arguments and notes
 *
 *    The first argument to cyb_reprogram() is a backend cookie as returned from
 *    cyb_configure().
 *
 *    The second argument is an absolute time at which the CY_HIGH_LEVEL
 *    interrupt should fire.  The specified time _may_ be in the past (albeit
 *    the very recent past).  If this is the case, the backend should generate
 *    a CY_HIGH_LEVEL interrupt as soon as possible.
 *
 *    The platform should not assume that cyb_reprogram() will be called with
 *    monotonically increasing values.
 *
 *    If the platform does not allow for interrupts at arbitrary times in the
 *    future, cyb_reprogram() may do nothing -- as long as cyclic_fire() is
 *    called periodically at CY_HIGH_LEVEL.  While this is clearly suboptimal
 *    (cyclic granularity will be bounded by the length of the period between
 *    cyclic_fire()'s), it allows the cyclic subsystem to be implemented on
 *    inferior hardware.
 *
 *  6.3  Return value
 *
 *     None.
 *
 *  6.4  Caller's context
 *
 *    cyb_reprogram() will only be called from CY_HIGH_LEVEL context on the CPU
 *    corresponding to the specified backend.
 *
 *  10  cyb_xcall(cyb_arg_t arg, cpu_t *, void(*func)(void *), void *farg)
 *
 *  10.1  Overview
 *
 *    cyb_xcall() should execute the specified function on the specified CPU.
 *
 *  10.2  Arguments and notes
 *
 *    The first argument to cyb_restore_level() is a backend cookie as returned
 *    from cyb_configure().  The second argument is a CPU on which the third
 *    argument, a function pointer, should be executed.  The fourth argument,
 *    a void *, should be passed as the argument to the specified function.
 *
 *    cyb_xcall() must provide exactly-once semantics.  If the specified
 *    function is called more than once, or not at all, the cyclic subsystem
 *    will become internally inconsistent.  The specified function must be
 *    be executed on the specified CPU, but may be executed in any context
 *    (any interrupt context or kernel context).
 *
 *    cyb_xcall() cannot block.  Any resources which cyb_xcall() needs to
 *    acquire must thus be protected by synchronization primitives which
 *    never require the caller to block.
 *
 *  10.3  Return value
 *
 *    None.
 *
 *  10.4  Caller's context
 *
 *    cpu_lock will be held and kernel preemption may be disabled.  The caller
 *    may be unable to block, giving rise to the constraint outlined in
 *    10.2, above.
 *
 */
typedef struct cyc_backend {
	cyb_arg_t (*cyb_configure)(cpu_t *);
	void (*cyb_unconfigure)(cyb_arg_t);
	void (*cyb_enable)(cyb_arg_t);
	void (*cyb_disable)(cyb_arg_t);
	void (*cyb_reprogram)(cyb_arg_t, hrtime_t);
	void (*cyb_xcall)(cyb_arg_t, cpu_t *, cyc_func_t, void *);
	cyb_arg_t cyb_arg;
} cyc_backend_t;

#define	CYF_FREE		0x0001

typedef struct cyclic {
	hrtime_t cy_expire;
	hrtime_t cy_interval;
	void (*cy_handler)(void *);
	void *cy_arg;
	uint16_t cy_flags;
} cyclic_t;

typedef struct cyc_cpu {
	cpu_t *cyp_cpu;
	cyc_index_t *cyp_heap;
	cyclic_t *cyp_cyclics;
	cyc_index_t cyp_nelems;
	cyc_index_t cyp_size;
	cyc_backend_t *cyp_backend;
	struct mtx cyp_mtx;
} cyc_cpu_t;

typedef struct cyc_omni_cpu {
	cyc_cpu_t *cyo_cpu;
	cyc_index_t cyo_ndx;
	void *cyo_arg;
	struct cyc_omni_cpu *cyo_next;
} cyc_omni_cpu_t;

typedef struct cyc_id {
	cyc_cpu_t *cyi_cpu;
	cyc_index_t cyi_ndx;
	struct cyc_id *cyi_prev;
	struct cyc_id *cyi_next;
	cyc_omni_handler_t cyi_omni_hdlr;
	cyc_omni_cpu_t *cyi_omni_list;
} cyc_id_t;

typedef struct cyc_xcallarg {
	cyc_cpu_t *cyx_cpu;
	hrtime_t cyx_exp;
} cyc_xcallarg_t;

#define	CY_DEFAULT_PERCPU	1
#define	CY_PASSIVE_LEVEL	-1

#define	CY_WAIT			0
#define	CY_NOWAIT		1

#define	CYC_HEAP_PARENT(ndx)		(((ndx) - 1) >> 1)
#define	CYC_HEAP_RIGHT(ndx)		(((ndx) + 1) << 1)
#define	CYC_HEAP_LEFT(ndx)		((((ndx) + 1) << 1) - 1)

#endif
