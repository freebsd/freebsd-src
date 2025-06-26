/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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

/*
 * A driver for the Arm Errata Management Firmware Interface (Errata ABI).
 * This queries into the SMCCC firmware for the status of errata using the
 * interface documented in den0100 [1].
 *
 * [1] https://developer.arm.com/documentation/den0100/latest
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

#include <machine/cpu_feat.h>

#define	ERRATA_HIGHER_EL_MITIGATION	3
#define	ERRATA_NOT_AFFECTED		2
#define	ERRATA_AFFECTED			1

#define	EM_VERSION		SMCCC_FUNC_ID(SMCCC_FAST_CALL,		\
    SMCCC_32BIT_CALL, SMCCC_STD_SECURE_SERVICE_CALLS, 0xf0u)
#define	 EM_VERSION_MIN	0x10000L
#define	EM_FEATURES		SMCCC_FUNC_ID(SMCCC_FAST_CALL,		\
    SMCCC_32BIT_CALL, SMCCC_STD_SECURE_SERVICE_CALLS, 0xf1u)
#define	EM_CPU_ERRATUM_FEATURES	SMCCC_FUNC_ID(SMCCC_FAST_CALL,		\
    SMCCC_32BIT_CALL, SMCCC_STD_SECURE_SERVICE_CALLS, 0xf2u)

static device_identify_t errata_identify;
static device_probe_t errata_probe;
static device_attach_t errata_attach;
static cpu_feat_errata errata_cpu_feat_errata_check(const struct cpu_feat *,
    u_int);

static void
errata_identify(driver_t *driver, device_t parent)
{
	int32_t version;

	/* Check if Errata ABI is supported */
	if (smccc_arch_features(EM_VERSION) != SMCCC_RET_SUCCESS)
		return;

	/* Check we have Errata 1.0 or later */
	version = psci_call(EM_VERSION, 0, 0, 0);
	if (version < EM_VERSION_MIN)
		return;

	if (BUS_ADD_CHILD(parent, 0, "errata", DEVICE_UNIT_ANY) == NULL)
		device_printf(parent, "add errata child failed\n");
}

static int
errata_probe(device_t dev)
{
	device_set_desc(dev, "Arm SMCCC Errata Management");
	return (BUS_PROBE_NOWILDCARD);
}

static int
errata_attach(device_t dev)
{
	/* Check for EM_CPU_ERRATUM_FEATURES. It's mandatory, so should exist */
	if (arm_smccc_invoke(EM_FEATURES, EM_CPU_ERRATUM_FEATURES, NULL) < 0) {
		device_printf(dev,
		    "EM_CPU_ERRATUM_FEATURES is not implemented\n");
		return (ENXIO);
	}

	cpu_feat_register_errata_check(errata_cpu_feat_errata_check);

	return (0);
}

static cpu_feat_errata
errata_cpu_feat_errata_check(const struct cpu_feat *feat __unused, u_int errata_id)
{
	struct arm_smccc_res res;

	switch(arm_smccc_invoke(EM_CPU_ERRATUM_FEATURES, errata_id, 0, &res)) {
	default:
		return (ERRATA_UNKNOWN);
	case ERRATA_NOT_AFFECTED:
		return (ERRATA_NONE);
	case ERRATA_AFFECTED:
		return (ERRATA_AFFECTED);
	case ERRATA_HIGHER_EL_MITIGATION:
		return (ERRATA_FW_MITIGAION);
	}
}

static device_method_t errata_methods[] = {
	DEVMETHOD(device_identify,	errata_identify),
	DEVMETHOD(device_probe,		errata_probe),
	DEVMETHOD(device_attach,	errata_attach),

	DEVMETHOD_END
};

static driver_t errata_driver = {
	"errata",
	errata_methods,
	0
};

DRIVER_MODULE(errata, smccc, errata_driver, 0, 0);
