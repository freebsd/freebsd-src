/*****************************************************************************
 * x86/xen/xen-os.h
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

#ifndef _MACHINE_X86_XEN_XEN_OS_H_
#define _MACHINE_X86_XEN_XEN_OS_H_

#ifndef _XEN_XEN_OS_H_
#error "do not #include machine/xen/xen-os.h, #include xen/xen-os.h instead"
#endif

/* Shared memory needs write-back as its cache attribute for coherency. */
#define VM_MEMATTR_XEN VM_MEMATTR_WRITE_BACK

/* Everything below this point is not included by assembler (.S) files. */
#ifndef __ASSEMBLY__

#include <sys/pcpu.h>

/* If non-zero, the hypervisor has been configured to use a direct vector */
extern int xen_vector_callback_enabled;

/* Signal whether the event channel vector requires EOI at the lapic */
extern bool xen_evtchn_needs_ack;

/* tunable for disabling PV disks */
extern int xen_disable_pv_disks;

/* tunable for disabling PV nics */
extern int xen_disable_pv_nics;

/* compatibility for accessing xen_ulong_t with atomics */
#define	atomic_clear_xen_ulong		atomic_clear_long
#define	atomic_set_xen_ulong		atomic_set_long
#define	atomic_readandclear_xen_ulong	atomic_readandclear_long
#define	atomic_testandset_xen_ulong	atomic_testandset_long
#define	atomic_load_acq_xen_ulong	atomic_load_acq_long
#define	atomic_store_rel_xen_ulong	atomic_store_rel_long
#define	atomic_set_xen_ulong		atomic_set_long
#define	atomic_clear_xen_ulong		atomic_clear_long

static inline u_int
XEN_CPUID_TO_VCPUID(u_int cpuid)
{

	return (pcpu_find(cpuid)->pc_vcpu_id);
}

#define	XEN_VCPUID()	PCPU_GET(vcpu_id)

static inline bool
xen_has_percpu_evtchn(void)
{

	return (!xen_hvm_domain() || xen_vector_callback_enabled);
}

static inline bool
xen_pv_disks_disabled(void)
{

	return (xen_hvm_domain() && xen_disable_pv_disks != 0);
}

static inline bool
xen_pv_nics_disabled(void)
{

	return (xen_hvm_domain() && xen_disable_pv_nics != 0);
}

bool xen_has_iommu_maps(void);

/* (Very) early initialization. */
void xen_early_init(void);

#endif /* !__ASSEMBLY__ */

#endif /* _MACHINE_X86_XEN_XEN_OS_H_ */
