/*-
 * Copyright (c) 2015 Nathan Whitehorn
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
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>
#include <machine/rtas.h>

#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <powerpc/ofw/ofw_pci.h>

#include "pcib_if.h"
#include "iommu_if.h"
#include "opal.h"

/*
 * Device interface.
 */
static int		opalpci_probe(device_t);
static int		opalpci_attach(device_t);

/*
 * pcib interface.
 */
static u_int32_t	opalpci_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		opalpci_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);

/*
 * Driver methods.
 */
static device_method_t	opalpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		opalpci_probe),
	DEVMETHOD(device_attach,	opalpci_attach),

	/* pcib interface */
	DEVMETHOD(pcib_read_config,	opalpci_read_config),
	DEVMETHOD(pcib_write_config,	opalpci_write_config),

	DEVMETHOD_END
};

struct opalpci_softc {
	struct ofw_pci_softc ofw_sc;
	uint64_t phb_id;
};

static devclass_t	opalpci_devclass;
DEFINE_CLASS_1(pcib, opalpci_driver, opalpci_methods,
    sizeof(struct opalpci_softc), ofw_pci_driver);
DRIVER_MODULE(opalpci, ofwbus, opalpci_driver, opalpci_devclass, 0, 0);

static int
opalpci_probe(device_t dev)
{
	const char	*type;

	if (opal_check() != 0)
		return (ENXIO);

	type = ofw_bus_get_type(dev);

	if (type == NULL || strcmp(type, "pci") != 0)
		return (ENXIO);

	if (!OF_hasprop(ofw_bus_get_node(dev), "ibm,opal-phbid"))
		return (ENXIO); 

	device_set_desc(dev, "OPAL Host-PCI bridge");
	return (BUS_PROBE_GENERIC);
}

static int
opalpci_attach(device_t dev)
{
	struct opalpci_softc *sc;
	cell_t id[2];

	sc = device_get_softc(dev);

	switch (OF_getproplen(ofw_bus_get_node(dev), "ibm,opal-phbid")) {
	case 8:
		OF_getencprop(ofw_bus_get_node(dev), "ibm,opal-phbid", id, 8);
		sc->phb_id = ((uint64_t)id[0] << 32) | id[1];
		break;
	case 4:
		OF_getencprop(ofw_bus_get_node(dev), "ibm,opal-phbid", id, 4);
		sc->phb_id = id[0];
		break;
	default:
		device_printf(dev, "PHB ID property had wrong length (%zd)\n",
		    OF_getproplen(ofw_bus_get_node(dev), "ibm,opal-phbid"));
		return (ENXIO);
	}

	return (ofw_pci_attach(dev));
}

static uint32_t
opalpci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct opalpci_softc *sc;
	uint64_t config_addr;
	uint8_t byte;
	uint16_t half;
	uint32_t word;
	int error;

	sc = device_get_softc(dev);

	config_addr = (bus << 8) | ((slot & 0x1f) << 3) | (func & 0x7);

	/* Sign-extend output */
	switch (width) {
	case 1:
		error = opal_call(OPAL_PCI_CONFIG_READ_BYTE, sc->phb_id,
		    config_addr, reg, vtophys(&byte));
		word = byte;
		break;
	case 2:
		error = opal_call(OPAL_PCI_CONFIG_READ_HALF_WORD, sc->phb_id,
		    config_addr, reg, vtophys(&half));
		word = half;
		break;
	case 4:
		error = opal_call(OPAL_PCI_CONFIG_READ_WORD, sc->phb_id,
		    config_addr, reg, vtophys(&word));
		break;
	default:
		word = 0xffffffff;
	}
	
	if (error != OPAL_SUCCESS)
		word = 0xffffffff;

	return (word);
}

static void
opalpci_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int width)
{
	struct opalpci_softc *sc;
	uint64_t config_addr;

	sc = device_get_softc(dev);

	config_addr = (bus << 8) | ((slot & 0x1f) << 3) | (func & 0x7);

	/* Sign-extend output */
	switch (width) {
	case 1:
		opal_call(OPAL_PCI_CONFIG_WRITE_BYTE, sc->phb_id, config_addr,
		    reg, val);
		break;
	case 2:
		opal_call(OPAL_PCI_CONFIG_WRITE_HALF_WORD, sc->phb_id,
		    config_addr, reg, val);
		break;
	case 4:
		opal_call(OPAL_PCI_CONFIG_WRITE_WORD, sc->phb_id, config_addr,
		    reg, val);
		break;
	}
}

