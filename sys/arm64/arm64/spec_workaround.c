/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Arm Ltd
 * Copyright (c) 2018 Andrew Turner
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpu_feat.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

static enum {
	SSBD_FORCE_ON,
	SSBD_FORCE_OFF,
	SSBD_KERNEL,
} ssbd_method = SSBD_KERNEL;

struct psci_bp_hardening_impl {
	u_int		midr_mask;
	u_int		midr_value;
};

static struct psci_bp_hardening_impl psci_bp_hardening_impl[] = {
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A57,0,0),
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A72,0,0),
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A73,0,0),
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A75,0,0),
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX2, 0,0),
	}
};

static cpu_feat_en
psci_bp_hardening_check(const struct cpu_feat *feat __unused, u_int midr)
{
	size_t i;

	for (i = 0; i < nitems(psci_bp_hardening_impl); i++) {
		if ((midr & psci_bp_hardening_impl[i].midr_mask) ==
		    psci_bp_hardening_impl[i].midr_value) {
			/* SMCCC depends on PSCI. If PSCI is missing so is SMCCC */
			if (!psci_present)
				return (FEAT_ALWAYS_DISABLE);

			if (smccc_arch_features(SMCCC_ARCH_WORKAROUND_1) !=
			    SMCCC_RET_SUCCESS)
				return (FEAT_ALWAYS_DISABLE);

			return (FEAT_DEFAULT_ENABLE);
		}
	}

	return (FEAT_ALWAYS_DISABLE);
}

static bool
psci_bp_hardening_enable(const struct cpu_feat *feat __unused,
    cpu_feat_errata errata_status __unused, u_int *errata_list __unused,
    u_int errata_count __unused)
{
	PCPU_SET(bp_harden, smccc_arch_workaround_1);

	return (true);
}

CPU_FEAT(feat_csv2_missing, "Branch Predictor Hardening",
	psci_bp_hardening_check, NULL, psci_bp_hardening_enable, NULL,
        CPU_FEAT_AFTER_DEV | CPU_FEAT_PER_CPU);

static cpu_feat_en
ssbd_workaround_check(const struct cpu_feat *feat __unused, u_int midr __unused)
{
	char *env;

	if (PCPU_GET(cpuid) == 0) {
		env = kern_getenv("kern.cfg.ssbd");
		if (env != NULL) {
			if (strcmp(env, "force-on") == 0) {
				ssbd_method = SSBD_FORCE_ON;
			} else if (strcmp(env, "force-off") == 0) {
				ssbd_method = SSBD_FORCE_OFF;
			}
		}
	}

	/* SMCCC depends on PSCI. If PSCI is missing so is SMCCC */
	if (!psci_present)
		return (FEAT_ALWAYS_DISABLE);

	/* Enable the workaround on this CPU if it's enabled in the firmware */
	if (smccc_arch_features(SMCCC_ARCH_WORKAROUND_2) != SMCCC_RET_SUCCESS)
		return (FEAT_ALWAYS_DISABLE);

	return (FEAT_DEFAULT_ENABLE);
}

static bool
ssbd_workaround_enable(const struct cpu_feat *feat __unused,
    cpu_feat_errata errata_status __unused, u_int *errata_list __unused,
    u_int errata_count __unused)
{
	switch(ssbd_method) {
	case SSBD_FORCE_ON:
		smccc_arch_workaround_2(1);
		break;
	case SSBD_FORCE_OFF:
		smccc_arch_workaround_2(0);
		break;
	case SSBD_KERNEL:
	default:
		PCPU_SET(ssbd, smccc_arch_workaround_2);
		break;
	}

	return (true);
}

CPU_FEAT(feat_ssbs_missing, "Speculator Store Bypass Disable Workaround",
	ssbd_workaround_check, NULL, ssbd_workaround_enable, NULL,
        CPU_FEAT_AFTER_DEV | CPU_FEAT_PER_CPU);
