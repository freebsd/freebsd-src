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
 *
 * $FreeBSD$
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

/* If non-zero, the hypervisor has been configured to use a direct vector */
extern int xen_vector_callback_enabled;

/* tunable for disabling PV disks */
extern int xen_disable_pv_disks;

/* tunable for disabling PV nics */
extern int xen_disable_pv_nics;

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

#endif /* !__ASSEMBLY__ */

#endif /* _MACHINE_X86_XEN_XEN_OS_H_ */
