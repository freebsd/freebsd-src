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
 *
 * $FreeBSD$
 */

#ifndef _XEN_XEN_OS_H_
#define _XEN_XEN_OS_H_

#define  __XEN_INTERFACE_VERSION__ 0x00030208

#define GRANT_REF_INVALID   0xffffffff

#ifdef LOCORE
#define __ASSEMBLY__
#endif

#include <contrib/xen/xen.h>

#ifndef __ASSEMBLY__
#include <xen/hvm.h>
#include <contrib/xen/event_channel.h>

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

enum xen_domain_type {
	XEN_NATIVE,             /* running on bare hardware    */
	XEN_PV_DOMAIN,          /* running in a PV domain      */
	XEN_HVM_DOMAIN,         /* running in a Xen hvm domain */
};

extern enum xen_domain_type xen_domain_type;

static inline int
xen_domain(void)
{
	return (xen_domain_type != XEN_NATIVE);
}

static inline int
xen_pv_domain(void)
{
	return (xen_domain_type == XEN_PV_DOMAIN);
}

static inline int
xen_hvm_domain(void)
{
	return (xen_domain_type == XEN_HVM_DOMAIN);
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
xen_test_bit(int bit, volatile long *addr)
{
	unsigned long mask = 1UL << (bit % NBPL);

	return !!(atomic_load_acq_long(&addr[bit / NBPL]) & mask);
}

static inline void
xen_set_bit(int bit, volatile long *addr)
{
	atomic_set_long(&addr[bit / NBPL], 1UL << (bit % NBPL));
}

static inline void
xen_clear_bit(int bit, volatile long *addr)
{
	atomic_clear_long(&addr[bit / NBPL], 1UL << (bit % NBPL));
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
