/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

#define	SMCCC_VERSION_1_0	0x10000

/* Assume 1.0 until we detect a later version */
static uint32_t	smccc_version;

void
smccc_init(void)
{
	int32_t features;
	uint32_t ret;

	smccc_version = SMCCC_VERSION_1_0;
	features = psci_features(SMCCC_VERSION);
	if (features != PSCI_RETVAL_NOT_SUPPORTED) {
		ret = psci_call(SMCCC_VERSION, 0, 0, 0);
		/* This should always be the case as we checked it above */
		if (ret > 0)
			smccc_version = ret;
	}

	if (bootverbose) {
		printf("Found SMCCC version %u.%u\n",
		    SMCCC_VERSION_MAJOR(smccc_version),
		    SMCCC_VERSION_MINOR(smccc_version));
	}
}

uint32_t
smccc_get_version(void)
{
	MPASS(smccc_version != 0);
	return (smccc_version);
}

int32_t
smccc_arch_features(uint32_t smccc_func_id)
{

	MPASS(smccc_version != 0);
	if (smccc_version == SMCCC_VERSION_1_0)
		return (PSCI_RETVAL_NOT_SUPPORTED);

	return (psci_call(SMCCC_ARCH_FEATURES, smccc_func_id, 0, 0));
}

/*
 * The SMCCC handler for Spectre variant 2: Branch target injection.
 * (CVE-2017-5715)
 */
int
smccc_arch_workaround_1(void)
{

	MPASS(smccc_version != 0);
	KASSERT(smccc_version != SMCCC_VERSION_1_0,
	    ("SMCCC arch workaround 1 called with an invalid SMCCC interface"));
	return (psci_call(SMCCC_ARCH_WORKAROUND_1, 0, 0, 0));
}

int
smccc_arch_workaround_2(int enable)
{

	MPASS(smccc_version != 0);
	KASSERT(smccc_version != SMCCC_VERSION_1_0,
	    ("SMCCC arch workaround 2 called with an invalid SMCCC interface"));
	return (psci_call(SMCCC_ARCH_WORKAROUND_2, enable, 0, 0));
}
