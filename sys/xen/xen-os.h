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

#if !defined(__XEN_INTERFACE_VERSION__)  
#define  __XEN_INTERFACE_VERSION__ 0x00030208
#endif  

#define GRANT_REF_INVALID   0xffffffff

#ifdef LOCORE
#define __ASSEMBLY__
#endif

#include <machine/xen/xen-os.h>

#include <xen/interface/xen.h>

/* Everything below this point is not included by assembler (.S) files. */
#ifndef __ASSEMBLY__

/* Force a proper event-channel callback from Xen. */
void force_evtchn_callback(void);

extern shared_info_t *HYPERVISOR_shared_info;

#ifdef XENHVM
extern int xen_disable_pv_disks;
extern int xen_disable_pv_nics;
#endif

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
