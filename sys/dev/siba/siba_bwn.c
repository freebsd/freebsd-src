/*-
 * Copyright (c) 2009-2010 Weongyo Jeong <weongyo@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Sonics Silicon Backplane front-end for bwn(4).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

/*
 * PCI glue.
 */

struct siba_bwn_softc {
	/* Child driver using MSI. */
	device_t			ssc_msi_child;
	struct siba_softc		ssc_siba;
};

#define	BS_BAR				0x10
#define	PCI_VENDOR_BROADCOM		0x14e4
#define	N(a)				(sizeof(a) / sizeof(a[0]))

static const struct siba_dev {
	uint16_t	vid;
	uint16_t	did;
	const char	*desc;
} siba_devices[] = {
	{ PCI_VENDOR_BROADCOM, 0x4301, "Broadcom BCM4301 802.11b Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4306, "Unknown" },
	{ PCI_VENDOR_BROADCOM, 0x4307, "Broadcom BCM4307 802.11b Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4311, "Broadcom BCM4311 802.11b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4312,
	  "Broadcom BCM4312 802.11a/b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4315, "Broadcom BCM4312 802.11b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4318, "Broadcom BCM4318 802.11b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4319,
	  "Broadcom BCM4318 802.11a/b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4320, "Broadcom BCM4306 802.11b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4321, "Broadcom BCM4306 802.11a Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4324,
	  "Broadcom BCM4309 802.11a/b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4325, "Broadcom BCM4306 802.11b/g Wireless" },
	{ PCI_VENDOR_BROADCOM, 0x4328, "Unknown" },
	{ PCI_VENDOR_BROADCOM, 0x4329, "Unknown" },
	{ PCI_VENDOR_BROADCOM, 0x432b, "Unknown" }
};

int		siba_core_attach(struct siba_softc *);
int		siba_core_detach(struct siba_softc *);
int		siba_core_suspend(struct siba_softc *);
int		siba_core_resume(struct siba_softc *);

static int
siba_bwn_probe(device_t dev)
{
	int i;
	uint16_t did, vid;

	did = pci_get_device(dev);
	vid = pci_get_vendor(dev);

	for (i = 0; i < N(siba_devices); i++) {
		if (siba_devices[i].did == did && siba_devices[i].vid == vid) {
			device_set_desc(dev, siba_devices[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
siba_bwn_attach(device_t dev)
{
	struct siba_bwn_softc *ssc = device_get_softc(dev);
	struct siba_softc *siba = &ssc->ssc_siba;

	siba->siba_dev = dev;
	siba->siba_type = SIBA_TYPE_PCI;

	/*
	 * Enable bus mastering.
	 */
	pci_enable_busmaster(dev);

	/* 
	 * Setup memory-mapping of PCI registers.
	 */
	siba->siba_mem_rid = SIBA_PCIR_BAR;
	siba->siba_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		&siba->siba_mem_rid, RF_ACTIVE);
	if (siba->siba_mem_res == NULL) {
		device_printf(dev, "cannot map register space\n");
		return (ENXIO);
	}
	siba->siba_mem_bt = rman_get_bustag(siba->siba_mem_res);
	siba->siba_mem_bh = rman_get_bushandle(siba->siba_mem_res);

	/* Get more PCI information */
	siba->siba_pci_did = pci_get_device(dev);
	siba->siba_pci_vid = pci_get_vendor(dev);
	siba->siba_pci_subvid = pci_get_subvendor(dev);
	siba->siba_pci_subdid = pci_get_subdevice(dev);
	siba->siba_pci_revid = pci_get_revid(dev);

	return (siba_core_attach(siba));
}

static int
siba_bwn_detach(device_t dev)
{
	struct siba_bwn_softc *ssc = device_get_softc(dev);
	struct siba_softc *siba = &ssc->ssc_siba;

	/* check if device was removed */
	siba->siba_invalid = !bus_child_present(dev);

	pci_disable_busmaster(dev);
	bus_generic_detach(dev);
	siba_core_detach(siba);

	bus_release_resource(dev, SYS_RES_MEMORY, BS_BAR, siba->siba_mem_res);

	return (0);
}

static int
siba_bwn_shutdown(device_t dev)
{
	device_t *devlistp;
	int devcnt, error = 0, i;

	error = device_get_children(dev, &devlistp, &devcnt);
	if (error != 0)
		return (error);

	for (i = 0 ; i < devcnt ; i++)
		device_shutdown(devlistp[i]);
	free(devlistp, M_TEMP);
	return (0);
}

static int
siba_bwn_suspend(device_t dev)
{
	struct siba_bwn_softc *ssc = device_get_softc(dev);
	struct siba_softc *siba = &ssc->ssc_siba;
	device_t *devlistp;
	int devcnt, error = 0, i, j;

	error = device_get_children(dev, &devlistp, &devcnt);
	if (error != 0)
		return (error);

	for (i = 0 ; i < devcnt ; i++) {
		error = DEVICE_SUSPEND(devlistp[i]);
		if (error) {
			for (j = 0; j < i; i++)
				DEVICE_RESUME(devlistp[j]);
			return (error);
		}
	}
	free(devlistp, M_TEMP);
	return (siba_core_suspend(siba));
}

static int
siba_bwn_resume(device_t dev)
{
	struct siba_bwn_softc *ssc = device_get_softc(dev);
	struct siba_softc *siba = &ssc->ssc_siba;
	device_t *devlistp;
	int devcnt, error = 0, i;

	error = siba_core_resume(siba);
	if (error != 0)
		return (error);

	error = device_get_children(dev, &devlistp, &devcnt);
	if (error != 0)
		return (error);

	for (i = 0 ; i < devcnt ; i++)
		DEVICE_RESUME(devlistp[i]);
	free(devlistp, M_TEMP);
	return (0);
}

/* proxying to the parent */
static struct resource *
siba_bwn_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{

	return (BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
	    type, rid, start, end, count, flags));
}

/* proxying to the parent */
static int
siba_bwn_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{

	return (BUS_RELEASE_RESOURCE(device_get_parent(dev), dev, type,
	    rid, r));
}

/* proxying to the parent */
static int
siba_bwn_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_filter_t *filter, driver_intr_t *intr, void *arg,
    void **cookiep)
{

	return (BUS_SETUP_INTR(device_get_parent(dev), dev, irq, flags,
	    filter, intr, arg, cookiep));
}

/* proxying to the parent */
static int
siba_bwn_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), dev, irq, cookie));
}

static int
siba_bwn_find_extcap(device_t dev, device_t child, int capability,
    int *capreg)
{

	return (pci_find_extcap(dev, capability, capreg));
}

static int
siba_bwn_alloc_msi(device_t dev, device_t child, int *count)
{
	struct siba_bwn_softc *ssc;
	int error;

	ssc = device_get_softc(dev);
	if (ssc->ssc_msi_child != NULL)
		return (EBUSY);
	error = pci_alloc_msi(dev, count);
	if (error == 0)
		ssc->ssc_msi_child = child;
	return (error);
}

static int
siba_bwn_release_msi(device_t dev, device_t child)
{
	struct siba_bwn_softc *ssc;
	int error;

	ssc = device_get_softc(dev);
	if (ssc->ssc_msi_child != child)
		return (ENXIO);
	error = pci_release_msi(dev);
	if (error == 0)
		ssc->ssc_msi_child = NULL;
	return (error);
}

static int
siba_bwn_msi_count(device_t dev, device_t child)
{

	return (pci_msi_count(dev));
}

static int
siba_bwn_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct siba_dev_softc *sd;
	struct siba_softc *siba;;

	sd = device_get_ivars(child);
	siba = sd->sd_bus;

	switch (which) {
	case SIBA_IVAR_VENDOR:
		*result = sd->sd_id.sd_vendor;
		break;
	case SIBA_IVAR_DEVICE:
		*result = sd->sd_id.sd_device;
		break;
	case SIBA_IVAR_REVID:
		*result = sd->sd_id.sd_rev;
		break;
	case SIBA_IVAR_PCI_VENDOR:
		*result = siba->siba_pci_vid;
		break;
	case SIBA_IVAR_PCI_DEVICE:
		*result = siba->siba_pci_did;
		break;
	case SIBA_IVAR_PCI_SUBVENDOR:
		*result = siba->siba_pci_subvid;
		break;
	case SIBA_IVAR_PCI_SUBDEVICE:
		*result = siba->siba_pci_subdid;
		break;
	case SIBA_IVAR_PCI_REVID:
		*result = siba->siba_pci_revid;
		break;
	case SIBA_IVAR_CHIPID:
		*result = siba->siba_chipid;
		break;
	case SIBA_IVAR_CHIPREV:
		*result = siba->siba_chiprev;
		break;
	case SIBA_IVAR_CHIPPKG:
		*result = siba->siba_chippkg;
		break;
	case SIBA_IVAR_TYPE:
		*result = siba->siba_type;
		break;
	case SIBA_IVAR_CC_PMUFREQ:
		*result = siba->siba_cc.scc_pmu.freq;
		break;
	case SIBA_IVAR_CC_CAPS:
		*result = siba->siba_cc.scc_caps;
		break;
	case SIBA_IVAR_CC_POWERDELAY:
		*result = siba->siba_cc.scc_powerup_delay;
		break;
	case SIBA_IVAR_PCICORE_REVID:
		*result = siba->siba_pci.spc_dev->sd_id.sd_rev;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static device_method_t siba_bwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		siba_bwn_probe),
	DEVMETHOD(device_attach,	siba_bwn_attach),
	DEVMETHOD(device_detach,	siba_bwn_detach),
	DEVMETHOD(device_shutdown,	siba_bwn_shutdown),
	DEVMETHOD(device_suspend,	siba_bwn_suspend),
	DEVMETHOD(device_resume,	siba_bwn_resume),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,   siba_bwn_alloc_resource),
	DEVMETHOD(bus_release_resource, siba_bwn_release_resource),
	DEVMETHOD(bus_read_ivar,	siba_bwn_read_ivar),
	DEVMETHOD(bus_setup_intr,       siba_bwn_setup_intr),
	DEVMETHOD(bus_teardown_intr,    siba_bwn_teardown_intr),

	/* PCI interface */
	DEVMETHOD(pci_find_extcap,	siba_bwn_find_extcap),
	DEVMETHOD(pci_alloc_msi,	siba_bwn_alloc_msi),
	DEVMETHOD(pci_release_msi,	siba_bwn_release_msi),
	DEVMETHOD(pci_msi_count,	siba_bwn_msi_count),

	KOBJMETHOD_END
};
static driver_t siba_bwn_driver = {
	"siba_bwn",
	siba_bwn_methods,
	sizeof(struct siba_bwn_softc)
};
static devclass_t siba_bwn_devclass;
DRIVER_MODULE(siba_bwn, pci, siba_bwn_driver, siba_bwn_devclass, 0, 0);
MODULE_VERSION(siba_bwn, 1);
