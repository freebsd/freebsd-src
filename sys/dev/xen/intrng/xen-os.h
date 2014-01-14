/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2014 Julien Grall
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
 */

#ifndef __MACHINE_ARM64_XEN_XEN_OS_H__
#define __MACHINE_ARM64_XEN_XEN_OS_H__

#ifndef _XEN_XEN_OS_H_
#error "do not #include machine/xen/xen-os.h, #include xen/xen-os.h instead"
#endif

/* Xen/ARM *requires* write-back/cached, so this is the correct setting */
#define VM_MEMATTR_XEN VM_MEMATTR_WRITE_BACK

#ifndef __ASSEMBLY__

/* Right now the device-tree is the only implemented detection method */
#define xen_domain_early() xen_dt_probe()

/* Early initializer, returns success/failure identical to xen_domain() */
extern int xen_dt_probe(void);

#define	XEN_CPUID_TO_VCPUID(cpu)	(cpu)

#define	XEN_VCPUID()			PCPU_GET(cpuid)

static inline bool
xen_pv_shutdown_handler(void)
{

	/* PV shutdown handler are always supported on ARM */
	return (true);
}

static inline bool
xen_has_percpu_evtchn(void)
{

	/* It's always possible to rebind event channel on ARM */
	return (true);
}

static inline bool
xen_pv_disks_disabled(void)
{

	/* It's not possible to disable PV disks on ARM */
	return (false);
}

static inline bool
xen_pv_nics_disabled(void)
{

	/* It's not possible to disable PV nics on ARM */
	return (false);
}

static inline bool
xen_has_iommu_maps(void)
{

	/* the Xen bug predates ARM support */
	return (true);
}

#endif

#endif /* __MACHINE_ARM64_XEN_XEN_OS__ */
