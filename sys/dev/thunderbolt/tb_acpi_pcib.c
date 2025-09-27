/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
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

#include "opt_acpi.h"
#include "opt_thunderbolt.h"

/* ACPI identified PCIe bridge for Thunderbolt */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>
#include <sys/rman.h>

#include <machine/pci_cfgreg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_private.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>
#include <machine/md_var.h>

#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_pcib.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/tb_debug.h>

static int	tb_acpi_pcib_probe(device_t);
static int	tb_acpi_pcib_attach(device_t);
static int	tb_acpi_pcib_detach(device_t);

/* ACPI attachment for Thudnerbolt Bridges */

static int
tb_acpi_pcib_probe(device_t dev)
{
	char desc[TB_DESC_MAX], desc1[TB_DESC_MAX];
	int val;

	if (pci_get_class(dev) != PCIC_BRIDGE ||
	    pci_get_subclass(dev) != PCIS_BRIDGE_PCI ||
	    acpi_disabled("pci"))
		return (ENXIO);
	if (acpi_get_handle(dev) == NULL)
		return (ENXIO);
	if (pci_cfgregopen() == 0)
		return (ENXIO);

	/*
	 * Success? Specify a higher probe priority than the conventional
	 * Thunderbolt PCIb driver
	 */
	if ((val = tb_pcib_probe_common(dev, desc)) < 0) {
		val++;
		snprintf(desc1, TB_DESC_MAX, "ACPI %s", desc);
		device_set_desc_copy(dev, desc1);
	}

	return (val);
}

static int
tb_acpi_pcib_attach(device_t dev)
{
	struct tb_pcib_softc *sc;
	int error;

	error = tb_pcib_attach_common(dev);
	if (error)
		return (error);

	sc = device_get_softc(dev);
	sc->ap_handle = acpi_get_handle(dev);
	KASSERT(sc->ap_handle != NULL, ("ACPI handle cannot be NULL\n"));

	/* Execute OSUP in case the BIOS didn't */
	if (TB_IS_ROOT(sc)) {
		ACPI_OBJECT_LIST list;
		ACPI_OBJECT arg;
		ACPI_BUFFER buf;
		ACPI_STATUS s;

		tb_debug(sc, DBG_BRIDGE, "Executing OSUP\n");

		list.Pointer = &arg;
		list.Count = 1;
		arg.Integer.Value = 0;
		arg.Type = ACPI_TYPE_INTEGER;
		buf.Length = ACPI_ALLOCATE_BUFFER;
		buf.Pointer = NULL;

		s = AcpiEvaluateObject(sc->ap_handle, "\\_GPE.OSUP", &list,
		    &buf);
		tb_debug(sc, DBG_BRIDGE|DBG_FULL,
		    "ACPI returned %d, buf= %p\n", s, buf.Pointer);
		if (buf.Pointer != NULL)
			tb_debug(sc, DBG_BRIDGE|DBG_FULL, "buffer= 0x%x\n",
			    *(uint32_t *)buf.Pointer);

		AcpiOsFree(buf.Pointer);
	}

	pcib_attach_common(dev);
	acpi_pcib_fetch_prt(dev, &sc->ap_prt);

	return (pcib_attach_child(dev));
}

static int
tb_acpi_pcib_detach(device_t dev)
{
	struct tb_pcib_softc *sc;
	int error;

	sc = device_get_softc(dev);
	tb_debug(sc, DBG_BRIDGE|DBG_ROUTER|DBG_EXTRA, "tb_acpi_pcib_detach\n");

	error = pcib_detach(dev);
	if (error == 0)
		AcpiOsFree(sc->ap_prt.Pointer);
	return (error);
}

static device_method_t tb_acpi_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tb_acpi_pcib_probe),
	DEVMETHOD(device_attach,	tb_acpi_pcib_attach),
	DEVMETHOD(device_detach,	tb_acpi_pcib_detach),

	/* Thunderbolt interface is inherited */

	DEVMETHOD_END
};

DEFINE_CLASS_2(tbolt, tb_acpi_pcib_driver, tb_acpi_pcib_methods,
    sizeof(struct tb_pcib_softc), pcib_driver, tb_pcib_driver);
DRIVER_MODULE_ORDERED(tb_acpi_pcib, pci, tb_acpi_pcib_driver,
     NULL, NULL, SI_ORDER_MIDDLE);
MODULE_DEPEND(tb_acpi_pcib, acpi, 1, 1, 1);
