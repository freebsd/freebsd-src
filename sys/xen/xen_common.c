/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2008, 2013 Citrix Systems, Inc.
 * Copyright © 2012 Spectra Logic Corporation
 * Copyright © 2021 Elliott Mitchell
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
 */

#include <sys/cdefs.h>
#include <sys/param.h> /* required by xen/xen-os.h */

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h> /* required by xen/xen-os.h */

#include <xen/xen-os.h>
#include <xen/hvm.h>

#include <contrib/xen/vcpu.h>

/*-------------------------------- Global Data -------------------------------*/
enum xen_domain_type xen_domain_type = XEN_NATIVE;

/**
 * Start info flags. ATM this only used to store the initial domain flag for
 * PVHv2, and it's always empty for HVM guests.
 */
uint32_t hvm_start_flags;

/*------------------ Hypervisor Access Shared Memory Regions -----------------*/
shared_info_t *HYPERVISOR_shared_info;

/*------------------------------- Per-CPU Data -------------------------------*/
DPCPU_DEFINE(struct vcpu_info *, vcpu_info);

void
xen_setup_vcpu_info(void)
{
	/* This isn't directly accessed outside this function */
	DPCPU_DEFINE_STATIC(vcpu_info_t, vcpu_local_info)
	    __attribute__((aligned(64)));

	vcpu_info_t *vcpu_info = DPCPU_PTR(vcpu_local_info);
	vcpu_register_vcpu_info_t info = {
		.mfn = vtophys(vcpu_info) >> PAGE_SHIFT,
		.offset = vtophys(vcpu_info) & PAGE_MASK,
	};
	unsigned int cpu = XEN_VCPUID();
	int rc;

	KASSERT(xen_domain(), ("%s(): invoked when not on Xen?", __func__));

	_Static_assert(sizeof(struct vcpu_info) <= 64,
	    "struct vcpu_info is larger than supported limit of 64 bytes");

	/*
	 * Set the vCPU info.
	 *
	 * NB: the vCPU info for some vCPUs can be fetched from the shared info
	 * page, but in order to make sure the mapping code is correct always
	 * attempt to map the vCPU info at a custom place.
	 */
	rc = HYPERVISOR_vcpu_op(VCPUOP_register_vcpu_info, cpu, &info);
	if (rc == 0)
		DPCPU_SET(vcpu_info, vcpu_info);
	else if (cpu < nitems(HYPERVISOR_shared_info->vcpu_info)) {
		static bool warned = false;

		DPCPU_SET(vcpu_info, &HYPERVISOR_shared_info->vcpu_info[cpu]);

		if (bootverbose && !warned) {
			warned = true;
			printf(
		"WARNING: Xen vCPU %u failed to setup vcpu_info rc = %d\n",
			    cpu, rc);
		}
	} else
		panic("Unable to register vCPU %u, rc=%d\n", cpu, rc);
}
