/*-
 * Copyright (c) 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: if_hme_pci.c,v 1.14 2004/03/17 08:58:23 martin Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * PCI front-end device driver for the HME ethernet device.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <machine/bus.h>
#if defined(__powerpc__) || defined(__sparc64__)
#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>
#endif
#include <machine/resource.h>

#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/hme/if_hmereg.h>
#include <dev/hme/if_hmevar.h>

#include "miibus_if.h"

struct hme_pci_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	struct	resource	*hsc_sres;
	int			hsc_srid;
	struct	resource	*hsc_ires;
	int			hsc_irid;
	bus_space_tag_t		hsc_memt;
	bus_space_handle_t	hsc_memh;
	void			*hsc_ih;
};

static int hme_pci_probe(device_t);
static int hme_pci_attach(device_t);
static int hme_pci_detach(device_t);
static int hme_pci_suspend(device_t);
static int hme_pci_resume(device_t);

static device_method_t hme_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hme_pci_probe),
	DEVMETHOD(device_attach,	hme_pci_attach),
	DEVMETHOD(device_detach,	hme_pci_detach),
	DEVMETHOD(device_suspend,	hme_pci_suspend),
	DEVMETHOD(device_resume,	hme_pci_resume),
	/* Can just use the suspend method here. */
	DEVMETHOD(device_shutdown,	hme_pci_suspend),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	hme_mii_readreg),
	DEVMETHOD(miibus_writereg,	hme_mii_writereg),
	DEVMETHOD(miibus_statchg,	hme_mii_statchg),

	{ 0, 0 }
};

static driver_t hme_pci_driver = {
	"hme",
	hme_pci_methods,
	sizeof(struct hme_pci_softc)
};

DRIVER_MODULE(hme, pci, hme_pci_driver, hme_devclass, 0, 0);
MODULE_DEPEND(hme, pci, 1, 1, 1);
MODULE_DEPEND(hme, ether, 1, 1, 1);

#define	PCI_VENDOR_SUN			0x108e
#define	PCI_PRODUCT_SUN_EBUS		0x1000
#define	PCI_PRODUCT_SUN_HMENETWORK	0x1001

int
hme_pci_probe(device_t dev)
{

	if (pci_get_vendor(dev) == PCI_VENDOR_SUN &&
	    pci_get_device(dev) ==  PCI_PRODUCT_SUN_HMENETWORK) {
		device_set_desc(dev, "Sun HME 10/100 Ethernet");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

int
hme_pci_attach(device_t dev)
{
	struct hme_pci_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;
	int error = 0;
#if !(defined(__powerpc__) || defined(__sparc64__))
	device_t *children, ebus_dev;
	struct resource *ebus_rres;
	bus_space_handle_t romh;
	bus_space_tag_t romt;
	int dataoff, ebus_rrid, slot, vpdoff;
	int i, nchildren;
	uint8_t buf[32];
	static const uint8_t promhdr[] = { 0x55, 0xaa };
#define	PROMHDR_PTR_DATA	0x18
	static const uint8_t promdat[] = {
		0x50, 0x43, 0x49, 0x52,	/* "PCIR" */
		PCI_VENDOR_SUN & 0xff, PCI_VENDOR_SUN >> 8,
		PCI_PRODUCT_SUN_HMENETWORK & 0xff,
		PCI_PRODUCT_SUN_HMENETWORK >> 8
	};
#define	PROMDATA_PTR_VPD	0x08
	struct pci_vpd {
		 uint8_t	vpd_key0;
		 uint8_t	vpd_key1;
		 uint8_t	vpd_len;
	} *vpd;
#define	PCI_VPDRES_ISLARGE(x)			((x) & 0x80)
#define	PCI_VPDRES_LARGE_NAME(x)		((x) & 0x7f)
#define	PCI_VPDRES_TYPE_VPD			0x10	/* large */
#endif

	pci_enable_busmaster(dev);
	/*
	 * Some Sun HMEs do have their intpin register bogusly set to 0,
	 * although it should be 1. correct that.
	 */
	if (pci_get_intpin(dev) == 0)
		pci_set_intpin(dev, 1);

	sc->sc_pci = 1;
	sc->sc_dev = dev;
	mtx_init(&sc->sc_lock, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers:	+0x0000
	 *	bank 1: HME ETX registers:	+0x2000
	 *	bank 2: HME ERX registers:	+0x4000
	 *	bank 3: HME MAC registers:	+0x6000
	 *	bank 4: HME MIF registers:	+0x7000
	 *
	 */
	hsc->hsc_srid = PCI_HME_BASEADDR;
	hsc->hsc_sres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 
	    &hsc->hsc_srid, RF_ACTIVE);
	if (hsc->hsc_sres == NULL) {
		device_printf(dev, "could not map device registers\n");
		error = ENXIO;
		goto fail_mtx;
	}
	hsc->hsc_irid = 0;
	hsc->hsc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
	    &hsc->hsc_irid, RF_SHAREABLE | RF_ACTIVE);
	if (hsc->hsc_ires == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		error = ENXIO;
		goto fail_sres;
	}
	hsc->hsc_memt = rman_get_bustag(hsc->hsc_sres);
	hsc->hsc_memh = rman_get_bushandle(hsc->hsc_sres);
	sc->sc_sebt = sc->sc_etxt = sc->sc_erxt = sc->sc_mact = sc->sc_mift =
	    hsc->hsc_memt;
	bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x0000, 0x1000,
	    &sc->sc_sebh);
	bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x2000, 0x1000,
	    &sc->sc_etxh);
	bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x4000, 0x1000,
	    &sc->sc_erxh);
	bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x6000, 0x1000,
	    &sc->sc_mach);
	bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x7000, 0x1000,
	    &sc->sc_mifh);

#if defined(__powerpc__) || defined(__sparc64__)
	OF_getetheraddr(dev, sc->sc_enaddr);
#else
	/*
	 * Dig out VPD (vital product data) and read NA (network address).
	 *
	 * The PCI HME is a PCIO chip, which is composed of two functions:
	 *	function 0: PCI-EBus2 bridge, and
	 *	function 1: HappyMeal Ethernet controller.
	 *
	 * The VPD of HME resides in the Boot PROM (PCI FCode) attached
	 * to the EBus bridge and can't be accessed via the PCI capability
	 * pointer.
	 * ``Writing FCode 3.x Programs'' (newer ones, dated 1997 and later)
	 * chapter 2 describes the data structure.
	 *
	 * We don't have a MI EBus driver since no EBus device exists
	 * (besides the FCode PROM) on add-on HME boards. The ``no driver
	 * attached'' message for function 0 therefore is what is expected.
	 */

	/* Search accompanying EBus bridge. */
	slot = pci_get_slot(dev);
	if (device_get_children(device_get_parent(dev), &children,
	    &nchildren) != 0) {
		device_printf(dev, "could not get children\n");
		error = ENXIO;
		goto fail_sres;
	}
	ebus_dev = NULL;
	for (i = 0; i < nchildren; i++) {
		if (pci_get_class(children[i]) == PCIC_BRIDGE &&
		    pci_get_vendor(children[i]) == PCI_VENDOR_SUN &&
		    pci_get_device(children[i]) ==  PCI_PRODUCT_SUN_EBUS &&
		    pci_get_slot(children[i]) == slot) {
			ebus_dev = children[i];
			break;
		}
	}
	if (ebus_dev == NULL) {
		device_printf(dev, "could not find EBus bridge\n");
		error = ENXIO;
		goto fail_children;
	}

	/* Map EBus bridge PROM registers. */
#define	PCI_EBUS2_BOOTROM	0x10
	ebus_rrid = PCI_EBUS2_BOOTROM;
	if ((ebus_rres = bus_alloc_resource_any(ebus_dev, SYS_RES_MEMORY,
	    &ebus_rrid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "could not map PROM registers\n");
		error = ENXIO;
		goto fail_children;
	}
	romt = rman_get_bustag(ebus_rres);
	romh = rman_get_bushandle(ebus_rres);

	/* Read PCI expansion PROM header. */
	bus_space_read_region_1(romt, romh, 0, buf, sizeof(buf));
	if (memcmp(buf, promhdr, sizeof(promhdr)) != 0 ||
	    (dataoff = (buf[PROMHDR_PTR_DATA] |
	    (buf[PROMHDR_PTR_DATA + 1] << 8))) < 0x1c) {
		device_printf(dev, "unexpected PCI expansion PROM header\n");
		error = ENXIO;
		goto fail_rres;
	}

	/* Read PCI expansion PROM data. */
	bus_space_read_region_1(romt, romh, dataoff, buf, sizeof(buf));
	if (memcmp(buf, promdat, sizeof(promdat)) != 0 ||
	    (vpdoff = (buf[PROMDATA_PTR_VPD] |
	    (buf[PROMDATA_PTR_VPD + 1] << 8))) < 0x1c) {
		device_printf(dev, "unexpected PCI expansion PROM data\n");
		error = ENXIO;
		goto fail_rres;
	}

	/*
	 * Read PCI VPD.
	 * SUNW,hme cards have a single large resource VPD-R tag
	 * containing one NA. SUNW,qfe cards have four large resource
	 * VPD-R tags containing one NA each (all four HME chips share
	 * the same PROM).
	 * The VPD used on both cards is not in PCI 2.2 standard format
	 * however. The length in the resource header is in big endian
	 * and the end tag is non-standard (0x79) and followed by an
	 * all-zero "checksum" byte. Sun calls this a "Fresh Choice
	 * Ethernet" VPD...
	 */
	/* Look at the end tag to determine whether this is a VPD with 4 NAs. */
	if (bus_space_read_1(romt, romh,
	    vpdoff + 3 + sizeof(struct pci_vpd) + ETHER_ADDR_LEN) != 0x79 &&
	    bus_space_read_1(romt, romh,
	    vpdoff + 4 * (3 + sizeof(struct pci_vpd) + ETHER_ADDR_LEN)) == 0x79)
		/* Use the Nth NA for the Nth HME on this SUNW,qfe. */
		vpdoff += slot * (3 + sizeof(struct pci_vpd) + ETHER_ADDR_LEN);
	bus_space_read_region_1(romt, romh, vpdoff, buf, sizeof(buf));
	vpd = (void *)(buf + 3);
	if (PCI_VPDRES_ISLARGE(buf[0]) == 0 ||
	    PCI_VPDRES_LARGE_NAME(buf[0]) != PCI_VPDRES_TYPE_VPD ||
	    (buf[1] << 8 | buf[2]) != sizeof(struct pci_vpd) + ETHER_ADDR_LEN ||
	    vpd->vpd_key0 != 0x4e /* N */ ||
	    vpd->vpd_key1 != 0x41 /* A */ ||
	    vpd->vpd_len != ETHER_ADDR_LEN) {
		device_printf(dev, "unexpected PCI VPD\n");
		error = ENXIO;
		goto fail_rres;
	}
	bcopy(buf + 3 + sizeof(struct pci_vpd), sc->sc_enaddr,
	    ETHER_ADDR_LEN);

fail_rres:
	bus_release_resource(ebus_dev, SYS_RES_MEMORY, ebus_rrid, ebus_rres);
fail_children:
	free(children, M_TEMP);
	if (error != 0)
		goto fail_sres;
#endif

	sc->sc_burst = 64;	/* XXX */

	/*
	 * call the main configure
	 */
	if ((error = hme_config(sc)) != 0) {
		device_printf(dev, "could not be configured\n");
		goto fail_ires;
	}

	if ((error = bus_setup_intr(dev, hsc->hsc_ires, INTR_TYPE_NET |
	    INTR_MPSAFE, hme_intr, sc, &hsc->hsc_ih)) != 0) {
		device_printf(dev, "couldn't establish interrupt\n");
		hme_detach(sc);
		goto fail_ires;
	}
	return (0);

fail_ires:
	bus_release_resource(dev, SYS_RES_IRQ, hsc->hsc_irid, hsc->hsc_ires);
fail_sres:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_srid, hsc->hsc_sres);
fail_mtx:
	mtx_destroy(&sc->sc_lock);
	return (error);
}

static int
hme_pci_detach(device_t dev)
{
	struct hme_pci_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	bus_teardown_intr(dev, hsc->hsc_ires, hsc->hsc_ih);
	hme_detach(sc);
	bus_release_resource(dev, SYS_RES_IRQ, hsc->hsc_irid, hsc->hsc_ires);
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_srid, hsc->hsc_sres);
	return (0);
}

static int
hme_pci_suspend(device_t dev)
{
	struct hme_pci_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	hme_suspend(sc);
	return (0);
}

static int
hme_pci_resume(device_t dev)
{
	struct hme_pci_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;

	hme_resume(sc);
	return (0);
}
