/*- SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2009-2012,2016-2017, 2022 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Implements low-level interactions with Hyper-V/Azure
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timetc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/aarch64/hyperv_machdep.h>
#include <dev/hyperv/vmbus/aarch64/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>
#include <contrib/dev/acpica/include/acpi.h>

void hyperv_init_tc(void);
int hypercall_page_setup(vm_paddr_t);
void hypercall_disable(void);
bool hyperv_identify_features(void);

static bool is_hyperv(void);

u_int hyperv_ver_major;
u_int hyperv_features;
u_int hyperv_recommends;

hyperv_tc64_t hyperv_tc64;

void
hyperv_init_tc(void)
{
	hyperv_tc64 = NULL;
}

int
hypercall_page_setup(vm_paddr_t hc)
{
	return (0);
}

void
hypercall_disable(void)
{
	return;
}

/*
 * This function verifies if the platform is Hyper-V or not.
 * To do that we are using ACPI FADT and for that, acpi
 * fadt is mapped first.
 */
static bool
is_hyperv(void)
{
	ACPI_TABLE_FADT *fadt;
	vm_paddr_t physaddr;
	uint64_t hypervid;
	bool ret;

	physaddr = acpi_find_table(ACPI_SIG_FADT);
	if (physaddr == 0)
		return (false);

	fadt = acpi_map_table(physaddr, ACPI_SIG_FADT);
	if (fadt == NULL) {
		printf("hyperv: Unable to map the FADT\n");
		return (false);
	}

	hypervid = fadt->HypervisorId;
	acpi_unmap_table(fadt);
	ret = strncmp((char *)&hypervid, "MsHyperV", 8) == 0 ? true : false;
	return (ret);
}

bool
hyperv_identify_features(void)
{
	struct hv_get_vp_registers_output result;

	if (resource_disabled("acpi", 0))
		return (false);
	if (!is_hyperv())
		return (false);

	vm_guest = VM_GUEST_HV;
	/* use MSRs to get the hyperv specific
	 * attributes.
	 */
	hv_get_vpreg_128(CPUID_LEAF_HV_FEATURES, &result);
	hyperv_features = result.as32.a;
	hv_get_vpreg_128(CPUID_LEAF_HV_IDENTITY, &result);
	hyperv_ver_major = result.as32.b >> 16;
	hv_get_vpreg_128(CPUID_LEAF_HV_RECOMMENDS, &result);
	hyperv_recommends = result.as32.a;
	return (true);
}
