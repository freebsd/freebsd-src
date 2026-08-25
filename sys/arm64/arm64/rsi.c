/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Arm Ltd
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/physmem.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/rsi.h>
#include <machine/vmparam.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

static struct realm_config config;
static bool rsi_present = false;

#define	PHYSMAP_SIZE (2 * (VM_PHYSSEG_MAX - 1))

static vm_paddr_t physmap[PHYSMAP_SIZE];

static unsigned long
rsi_request_version(unsigned long req, unsigned long *out_lower,
    unsigned long *out_higher)
{
	struct arm_smccc_res res;

	arm_smccc_invoke_smc(SMC_RSI_ABI_VERSION, req, &res);

	if (out_lower != NULL)
		*out_lower = res.a1;
	if (out_higher != NULL)
		*out_higher = res.a2;

	return (res.a0);
}

static inline unsigned long
rsi_get_realm_config(struct realm_config *cfg)
{
	struct arm_smccc_res res;

	arm_smccc_invoke_smc(SMC_RSI_REALM_CONFIG, vtophys(cfg), &res);
	return (res.a0);
}

static bool
rsi_version_matches(void)
{
	unsigned long ver_lower, ver_higher;
	unsigned long ret;

	ret = rsi_request_version(RSI_ABI_VERSION, &ver_lower, &ver_higher);

	if (ret == SMCCC_RET_NOT_SUPPORTED)
		return (false);

	if (ret != RSI_SUCCESS) {
		printf("RME: RMM doesn't support RSI version %lu.%lu. Supported range: %lu.%lu-%lu.%lu\n",
		    RSI_ABI_VERSION_MAJOR, RSI_ABI_VERSION_MINOR,
		    RSI_ABI_VERSION_GET_MAJOR(ver_lower),
		    RSI_ABI_VERSION_GET_MINOR(ver_lower),
		    RSI_ABI_VERSION_GET_MAJOR(ver_higher),
		    RSI_ABI_VERSION_GET_MINOR(ver_higher));
		return (false);
	}

	printf("RME: Using RSI version %lu.%lu\n",
	    RSI_ABI_VERSION_GET_MAJOR(ver_lower),
	    RSI_ABI_VERSION_GET_MINOR(ver_lower));

	return (true);
}


unsigned long
rsi_set_addr_range_state(vm_paddr_t start, vm_paddr_t end, enum ripas state,
    unsigned long flags, vm_paddr_t *top)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SMC_RSI_IPA_STATE_SET, start, end, state, flags, 0, 0, 0,
	    &res);

	if (top)
		*top = res.a1;

	return (res.a0);
}

static int
rsi_set_memory_range(vm_paddr_t start, vm_paddr_t end, enum ripas state,
    unsigned long flags)
{
	unsigned long ret;
	vm_paddr_t top;

	while (start != end) {
		ret = rsi_set_addr_range_state(start, end, state, flags, &top);
		if (ret || top < start || top > end)
			return (-EINVAL);
		start = top;
	}

	return (0);
}

/*
 * Convert the specified range to RAM. Do not convert any pages that may have
 * been DESTROYED, without our permission.
 */
static int
rsi_set_memory_range_protected_safe(vm_paddr_t start, vm_paddr_t end)
{
	return (rsi_set_memory_range(start, end, RSI_RIPAS_RAM,
	    RSI_NO_CHANGE_DESTROYED));
}

void
arm64_rsi_setup_memory(void)
{
	int physmap_idx;
	int i;

	if (!psci_conduit_is_smc())
		return;
	if (!rsi_version_matches())
		return;
	if (rsi_get_realm_config(&config))
		return;

	prot_ns_shared_pa = 1ul << (config.ipa_bits - 1);
	if (bootverbose)
		printf("arm64_rsi_setup_memory: rsi_present, ipa_bits=%lu prot_ns_shared_pa=%lx\n",
		    config.ipa_bits, prot_ns_shared_pa);
	rsi_present = true;

	physmap_idx = physmem_all(physmap, nitems(physmap));

	if (bootverbose)
		printf("physmap:\n");

	for (i = 0; i < physmap_idx; i += 2) {
		if (bootverbose)
			printf("  %lx %lx\n", physmap[i], physmap[i + 1]);

		if (rsi_set_memory_range_protected_safe(physmap[i],
		    physmap[i + 1]))
			panic("rsi_set_memory_range_protected_safe failed");
	}
}

bool
in_realm(void)
{
	return (rsi_present);
}
