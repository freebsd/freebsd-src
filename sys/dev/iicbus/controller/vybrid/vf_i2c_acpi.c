/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Pierre-Luc Drouin <pldrouin@pldrouin.net>
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
 * Vybrid Family Inter-Integrated Circuit (I2C)
 * Chapter 48, Vybrid Reference Manual, Rev. 5, 07/2013
 *
 * The current implementation is based on the original driver by Ruslan Bukin,
 * later modified by Dawid Górecki, and split into FDT and ACPI drivers by Val
 * Packett.
 */

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/iicbus/controller/vybrid/vf_i2c.h>

static char *vf_i2c_ids[] = {
	"NXP0001",
	NULL
};

static int
vf_i2c_acpi_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("vf_i2c"))
		return (ENXIO);

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, vf_i2c_ids, NULL);
	if (rv > 0)
		return (rv);

	device_set_desc(dev, VF_I2C_DEVSTR);
	return (BUS_PROBE_DEFAULT);
}

static int
vf_i2c_acpi_attach(device_t dev)
{
	struct vf_i2c_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->hwtype = HW_VF610;
	sc->freq = 0;

	return (vf_i2c_attach_common(dev));
}

static device_method_t vf_i2c_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,                 vf_i2c_acpi_probe),
	DEVMETHOD(device_attach,                vf_i2c_acpi_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(vf_i2c_acpi, vf_i2c_acpi_driver, vf_i2c_acpi_methods,
		sizeof(struct vf_i2c_softc), vf_i2c_driver);

DRIVER_MODULE(vf_i2c_acpi, acpi, vf_i2c_acpi_driver, 0, 0);
DRIVER_MODULE(iicbus, vf_i2c_acpi, iicbus_driver, 0, 0);
DRIVER_MODULE(acpi_iicbus, vf_i2c_acpi, acpi_iicbus_driver, 0, 0);
