/*-
 * Copyright (c) 2015-2016 Nathan Whitehorn
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

#include <dev/ofw/ofwpci.h>

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
 * bus interface.
 */

static int opalpci_setup_intr(device_t dev, device_t child, struct resource *r,
    int flags, driver_filter_t *filter, driver_intr_t *ithread,
    void *arg, void **cookiep);

/*
 * Commands
 */
#define	OPAL_M32_WINDOW_TYPE		1
#define	OPAL_M64_WINDOW_TYPE		2
#define	OPAL_IO_WINDOW_TYPE		3

#define	OPAL_RESET_PHB_COMPLETE		1
#define	OPAL_RESET_PCI_IODA_TABLE	6

#define	OPAL_DISABLE_M64		0
#define	OPAL_ENABLE_M64_SPLIT		1
#define	OPAL_ENABLE_M64_NON_SPLIT	2

#define	OPAL_EEH_ACTION_CLEAR_FREEZE_MMIO	1
#define	OPAL_EEH_ACTION_CLEAR_FREEZE_DMA	2
#define	OPAL_EEH_ACTION_CLEAR_FREEZE_ALL	3

/*
 * Constants
 */
#define OPAL_PCI_DEFAULT_PE			1

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

	/* bus overrides */
	DEVMETHOD(bus_setup_intr,	opalpci_setup_intr),

	DEVMETHOD_END
};

struct opalpci_softc {
	struct ofw_pci_softc ofw_sc;
	uint64_t phb_id;
};

static devclass_t	opalpci_devclass;
DEFINE_CLASS_1(pcib, opalpci_driver, opalpci_methods,
    sizeof(struct opalpci_softc), ofw_pci_driver);
EARLY_DRIVER_MODULE(opalpci, ofwbus, opalpci_driver, opalpci_devclass, 0, 0,
    BUS_PASS_BUS);

static int
opalpci_probe(device_t dev)
{
	const char	*type;

	if (opal_check() != 0)
		return (ENXIO);

	type = ofw_bus_get_type(dev);

	if (type == NULL || (strcmp(type, "pci") != 0 &&
	    strcmp(type, "pciex") != 0))
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
	cell_t id[2], m64window[6], npe;
	int i, err;

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

	if (bootverbose)
		device_printf(dev, "OPAL ID %#lx\n", sc->phb_id);

	/*
	 * Reset PCI IODA table
	 */
	err = opal_call(OPAL_PCI_RESET, sc->phb_id, OPAL_RESET_PCI_IODA_TABLE,
	    1);
	if (err != 0) {
		device_printf(dev, "IODA table reset failed: %d\n", err);
		return (ENXIO);
	}
	while ((err = opal_call(OPAL_PCI_POLL, sc->phb_id)) > 0)
		DELAY(1000*err); /* Returns expected delay in ms */
	if (err < 0) {
		device_printf(dev, "PHB IODA reset poll failed: %d\n", err);
		return (ENXIO);
	}

	/*
	 * Reset everything. Especially important if we have inherited the
	 * system from Linux by kexec()
	 */
#ifdef NOTYET
	if (bootverbose)
		device_printf(dev, "Resetting PCI bus\n");
	err = opal_call(OPAL_PCI_RESET, sc->phb_id, OPAL_RESET_PHB_COMPLETE, 1);
	if (err < 0) {
		device_printf(dev, "PHB reset failed: %d\n", err);
		return (ENXIO);
	}
	while ((err = opal_call(OPAL_PCI_POLL, sc->phb_id)) > 0)
		DELAY(1000*err); /* Returns expected delay in ms */
	if (err < 0) {
		device_printf(dev, "PHB reset poll failed: %d\n", err);
		return (ENXIO);
	}
	DELAY(10000);
	err = opal_call(OPAL_PCI_RESET, sc->phb_id, OPAL_RESET_PHB_COMPLETE, 0);
	if (err < 0) {
		device_printf(dev, "PHB reset completion failed: %d\n", err);
		return (ENXIO);
	}
	while ((err = opal_call(OPAL_PCI_POLL, sc->phb_id)) > 0)
		DELAY(1000*err); /* Returns expected delay in ms */
	if (err < 0) {
		device_printf(dev, "PHB reset completion  poll failed: %d\n",
		    err);
		return (ENXIO);
	}
	DELAY(10000);
#endif

	/*
	 * Map all devices on the bus to partitionable endpoint one until
	 * such time as we start wanting to do things like bhyve.
	 */
	err = opal_call(OPAL_PCI_SET_PE, sc->phb_id, OPAL_PCI_DEFAULT_PE,
	    0, 0, 0, 0, /* All devices */
	    OPAL_MAP_PE);
	if (err != 0) {
		device_printf(dev, "PE mapping failed: %d\n", err);
		return (ENXIO);
	}

	/*
	 * Turn on MMIO, mapped to PE 1
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "ibm,opal-num-pes", &npe, 4)
	    != 4)
		npe = 1;
	for (i = 0; i < npe; i++) {
		err = opal_call(OPAL_PCI_MAP_PE_MMIO_WINDOW, sc->phb_id,
		    OPAL_PCI_DEFAULT_PE, OPAL_M32_WINDOW_TYPE, 0, i);
		if (err != 0)
			device_printf(dev, "MMIO %d map failed: %d\n", i, err);
	}

	/* XXX: multiple M64 windows? */
	if (OF_getencprop(ofw_bus_get_node(dev), "ibm,opal-m64-window",
	    m64window, sizeof(m64window)) == sizeof(m64window)) {
		opal_call(OPAL_PCI_SET_PHB_MEM_WINDOW, sc->phb_id,
		    OPAL_M64_WINDOW_TYPE, 0 /* index */, 
		    ((uint64_t)m64window[2] << 32) | m64window[3], 0,
		    ((uint64_t)m64window[4] << 32) | m64window[5]);
		opal_call(OPAL_PCI_MAP_PE_MMIO_WINDOW, sc->phb_id,
		    OPAL_PCI_DEFAULT_PE, OPAL_M64_WINDOW_TYPE,
		    0 /* index */, 0);
		opal_call(OPAL_PCI_PHB_MMIO_ENABLE, sc->phb_id,
		    OPAL_M64_WINDOW_TYPE, 0, OPAL_ENABLE_M64_NON_SPLIT);
	}

	/*
	 * Also disable the IOMMU for the time being for PE 1 (everything)
	 */
	if (bootverbose)
		device_printf(dev, "Mapping 0-%#lx for DMA\n",
		    roundup2(powerpc_ptob(Maxmem), 16*1024*1024));
	err = opal_call(OPAL_PCI_MAP_PE_DMA_WINDOW_REAL, sc->phb_id,
	    OPAL_PCI_DEFAULT_PE, OPAL_PCI_DEFAULT_PE << 1,
	    0 /* start address */, roundup2(powerpc_ptob(Maxmem),
	    16*1024*1024)/* all RAM */);
	if (err != 0) {
		device_printf(dev, "DMA mapping failed: %d\n", err);
		return (ENXIO);
	}

	/*
	 * General OFW PCI attach
	 */
	err = ofw_pci_init(dev);
	if (err != 0)
		return (err);

	/*
	 * Unfreeze non-config-space PCI operations. Let this fail silently
	 * if e.g. there is no current freeze.
	 */
	opal_call(OPAL_PCI_EEH_FREEZE_CLEAR, sc->phb_id, OPAL_PCI_DEFAULT_PE,
	    OPAL_EEH_ACTION_CLEAR_FREEZE_ALL);

	/*
	 * OPAL stores 64-bit BARs in a special property rather than "ranges"
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "ibm,opal-m64-window",
	    m64window, sizeof(m64window)) == sizeof(m64window)) {
		struct ofw_pci_range *rp;

		sc->ofw_sc.sc_nrange++;
		sc->ofw_sc.sc_range = realloc(sc->ofw_sc.sc_range,
		    sc->ofw_sc.sc_nrange * sizeof(sc->ofw_sc.sc_range[0]),
		    M_DEVBUF, M_WAITOK);
		rp = &sc->ofw_sc.sc_range[sc->ofw_sc.sc_nrange-1];
		rp->pci_hi = OFW_PCI_PHYS_HI_SPACE_MEM64 |
		    OFW_PCI_PHYS_HI_PREFETCHABLE;
		rp->pci = ((uint64_t)m64window[0] << 32) | m64window[1];
		rp->host = ((uint64_t)m64window[2] << 32) | m64window[3];
		rp->size = ((uint64_t)m64window[4] << 32) | m64window[5];
		rman_manage_region(&sc->ofw_sc.sc_mem_rman, rp->pci,
		   rp->pci + rp->size - 1);
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

	/*
	 * Poking config state for non-existant devices can make
	 * the host bridge hang up. Clear any errors.
	 *
	 * XXX: Make this conditional on the existence of a freeze
	 */
	opal_call(OPAL_PCI_EEH_FREEZE_CLEAR, sc->phb_id, OPAL_PCI_DEFAULT_PE,
	    OPAL_EEH_ACTION_CLEAR_FREEZE_ALL);
	
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
	int error = OPAL_SUCCESS;

	sc = device_get_softc(dev);

	config_addr = (bus << 8) | ((slot & 0x1f) << 3) | (func & 0x7);

	switch (width) {
	case 1:
		error = opal_call(OPAL_PCI_CONFIG_WRITE_BYTE, sc->phb_id,
		    config_addr, reg, val);
		break;
	case 2:
		error = opal_call(OPAL_PCI_CONFIG_WRITE_HALF_WORD, sc->phb_id,
		    config_addr, reg, val);
		break;
	case 4:
		error = opal_call(OPAL_PCI_CONFIG_WRITE_WORD, sc->phb_id,
		    config_addr, reg, val);
		break;
	}

	if (error != OPAL_SUCCESS) {
		/*
		 * Poking config state for non-existant devices can make
		 * the host bridge hang up. Clear any errors.
		 */
		opal_call(OPAL_PCI_EEH_FREEZE_CLEAR, sc->phb_id,
		    OPAL_PCI_DEFAULT_PE, OPAL_EEH_ACTION_CLEAR_FREEZE_ALL);
	}
}

static int
opalpci_setup_intr(device_t dev, device_t child, struct resource *r,
    int flags, driver_filter_t *filter, driver_intr_t *ithread,
    void *arg, void **cookiep)
{
	struct opalpci_softc *sc;

	sc = device_get_softc(dev);
	opal_call(OPAL_PCI_SET_XIVE_PE, sc->phb_id, OPAL_PCI_DEFAULT_PE,
	    rman_get_start(r));

	return BUS_SETUP_INTR(device_get_parent(dev), child, r, flags, filter,
	    ithread, arg, cookiep);
}

