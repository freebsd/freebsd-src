/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <arm64/coresight/coresight.h>
#include <arm64/coresight/coresight_replicator.h>

static int
replicator_acpi_probe(device_t dev)
{
	static char *replicator_ids[] = { "ARMHC98D", NULL };
	int error;

	error = ACPI_ID_PROBE(device_get_parent(dev), dev,
	    replicator_ids, NULL);
	if (error <= 0)
		device_set_desc(dev, "ARM Coresight Replicator");

	return (error);
}

static int
replicator_acpi_attach(device_t dev)
{
	struct replicator_softc *sc;

	sc = device_get_softc(dev);
	sc->pdata = coresight_acpi_get_platform_data(dev);

	return (replicator_attach(dev));
}

static device_method_t replicator_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		replicator_acpi_probe),
	DEVMETHOD(device_attach,	replicator_acpi_attach),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(replicator, replicator_acpi_driver, replicator_acpi_methods,
    sizeof(struct replicator_softc), replicator_driver);

EARLY_DRIVER_MODULE(replicator, acpi, replicator_acpi_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
