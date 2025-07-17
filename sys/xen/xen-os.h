/******************************************************************************
 * xen/xen-os.h
 * 
 * Random collection of macros and definition
 *
 * Copyright (c) 2003, 2004 Keir Fraser (on behalf of the Xen team)
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _XEN_XEN_OS_H_
#define _XEN_XEN_OS_H_

#define  __XEN_INTERFACE_VERSION__ 0x00040d00

#define GRANT_REF_INVALID   0xffffffff

#ifdef LOCORE
#define __ASSEMBLY__
#endif

#include <contrib/xen/xen.h>

#ifndef __ASSEMBLY__
#include <sys/rman.h>

#include <xen/hvm.h>
#include <contrib/xen/event_channel.h>

/*
 * Setup function which needs to be called on each processor by architecture
 */
extern void xen_setup_vcpu_info(void);

static inline vm_paddr_t
xen_get_xenstore_mfn(void)
{

	return (hvm_get_parameter(HVM_PARAM_STORE_PFN));
}

static inline evtchn_port_t
xen_get_xenstore_evtchn(void)
{

	return (hvm_get_parameter(HVM_PARAM_STORE_EVTCHN));
}

static inline vm_paddr_t
xen_get_console_mfn(void)
{

	return (hvm_get_parameter(HVM_PARAM_CONSOLE_PFN));
}

static inline evtchn_port_t
xen_get_console_evtchn(void)
{

	return (hvm_get_parameter(HVM_PARAM_CONSOLE_EVTCHN));
}

extern shared_info_t *HYPERVISOR_shared_info;

extern bool xen_suspend_cancelled;

static inline bool
xen_domain(void)
{
	return (vm_guest == VM_GUEST_XEN);
}

static inline bool
xen_pv_domain(void)
{
	return (false);
}

static inline bool
xen_hvm_domain(void)
{
	return (vm_guest == VM_GUEST_XEN);
}

static inline bool
xen_initial_domain(void)
{

	return (xen_domain() && (hvm_start_flags & SIF_INITDOMAIN) != 0);
}
#endif

#include <machine/xen/xen-os.h>

/* Everything below this point is not included by assembler (.S) files. */
#ifndef __ASSEMBLY__

/*
 * Based on ofed/include/linux/bitops.h
 *
 * Those helpers are prefixed by xen_ because xen-os.h is widely included
 * and we don't want the other drivers using them.
 *
 */
#define NBPL (NBBY * sizeof(long))

static inline bool
xen_test_bit(int bit, volatile xen_ulong_t *addr)
{
	unsigned long mask = 1UL << (bit % NBPL);

	return !!(atomic_load_acq_xen_ulong(&addr[bit / NBPL]) & mask);
}

static inline void
xen_set_bit(int bit, volatile xen_ulong_t *addr)
{
	atomic_set_xen_ulong(&addr[bit / NBPL], 1UL << (bit % NBPL));
}

static inline void
xen_clear_bit(int bit, volatile xen_ulong_t *addr)
{
	atomic_clear_xen_ulong(&addr[bit / NBPL], 1UL << (bit % NBPL));
}

#undef NBPL

/*
 * Functions to allocate/free unused memory in order
 * to map memory from other domains.
 */
struct resource *xenmem_alloc(device_t dev, int *res_id, size_t size);
int xenmem_free(device_t dev, int res_id, struct resource *res);

/* Debug/emergency function, prints directly to hypervisor console */
void xc_printf(const char *, ...) __printflike(1, 2);

/*
 * Emergency print function, can be defined per-arch, otherwise defaults to
 * HYPERVISOR_console_write.  Should not be called directly, use xc_printf
 * instead.
 */
void xen_emergency_print(const char *str, size_t size);

/* Arch-specific helper to init scratch mapping space. */
int xen_arch_init_physmem(device_t dev, struct rman *mem);

#ifndef xen_mb
#define xen_mb() mb()
#endif
#ifndef xen_rmb
#define xen_rmb() rmb()
#endif
#ifndef xen_wmb
#define xen_wmb() wmb()
#endif

#endif /* !__ASSEMBLY__ */

#endif /* _XEN_XEN_OS_H_ */
