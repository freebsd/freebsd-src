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
 * A driver for the Arm True Random Number Generator Firmware Interface.
 * This queries into the SMCCC firmware for random numbers using the
 * interface documented in den0098 [1].
 *
 * [1] https://developer.arm.com/documentation/den0098/latest
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

#include <dev/random/randomdev.h>

#define	TRNG_VERSION		SMCCC_FUNC_ID(SMCCC_FAST_CALL, \
    SMCCC_32BIT_CALL, SMCCC_STD_SECURE_SERVICE_CALLS, 0x50)
#define	 TRNG_VERSION_MIN	0x10000L
#define	TRNG_RND64		SMCCC_FUNC_ID(SMCCC_FAST_CALL, \
    SMCCC_64BIT_CALL, SMCCC_STD_SECURE_SERVICE_CALLS, 0x53)

static device_identify_t trng_identify;
static device_probe_t trng_probe;
static device_attach_t trng_attach;

static unsigned trng_read(void *, unsigned);

static struct random_source random_trng = {
	.rs_ident = "Arm SMCCC TRNG",
	.rs_source = RANDOM_PURE_ARM_TRNG,
	.rs_read = trng_read,
};

static void
trng_identify(driver_t *driver, device_t parent)
{
	int32_t version;

	/* TRNG depends on SMCCC 1.1 (per the spec) */
	if (smccc_get_version() < SMCCC_MAKE_VERSION(1, 1))
		return;

	/* Check we have TRNG 1.0 or later */
	version = psci_call(TRNG_VERSION, 0, 0, 0);
	if (version < TRNG_VERSION_MIN)
		return;

	if (BUS_ADD_CHILD(parent, 0, "trng", -1) == NULL)
		device_printf(parent, "add TRNG child failed\n");
}

static int
trng_probe(device_t dev)
{
	device_set_desc(dev, "Arm SMCCC TRNG");
	return (BUS_PROBE_NOWILDCARD);
}

static int
trng_attach(device_t dev)
{
	struct arm_smccc_res res;
	int32_t ret;

	ret = arm_smccc_invoke(TRNG_RND64, 192, &res);
	if (ret < 0) {
		device_printf(dev, "Failed to read fron TRNG\n");
	} else {
		random_source_register(&random_trng);
	}

	return (0);
}

static unsigned
trng_read(void *buf, unsigned usz)
{
	struct arm_smccc_res res;
	register_t len;
	int32_t ret;

	len = usz;
	if (len > sizeof(uint64_t))
		len = sizeof(uint64_t);
	if (len == 0)
		return (0);

	ret = arm_smccc_invoke(TRNG_RND64, len * 8, &res);
	if (ret < 0)
		return (0);

	memcpy(buf, &res.a0, len);
	return (len);
}

static device_method_t trng_methods[] = {
	DEVMETHOD(device_identify,	trng_identify),
	DEVMETHOD(device_probe,		trng_probe),
	DEVMETHOD(device_attach,	trng_attach),

	DEVMETHOD_END
};

static driver_t trng_driver = {
	"trng",
	trng_methods,
	0
};

DRIVER_MODULE(trng, smccc, trng_driver, 0, 0);
