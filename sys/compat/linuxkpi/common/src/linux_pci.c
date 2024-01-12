/*-
 * Copyright (c) 2015-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 * Copyright (c) 2020-2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/pciio.h>
#include <sys/pctrie.h>
#include <sys/rman.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pci_iov.h>
#include <dev/backlight/backlight.h>

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>
#include <linux/compat.h>

#include <linux/backlight.h>

#include "backlight_if.h"
#include "pcib_if.h"

/* Undef the linux function macro defined in linux/pci.h */
#undef pci_get_class

extern int linuxkpi_debug;

SYSCTL_DECL(_compat_linuxkpi);

static counter_u64_t lkpi_pci_nseg1_fail;
SYSCTL_COUNTER_U64(_compat_linuxkpi, OID_AUTO, lkpi_pci_nseg1_fail, CTLFLAG_RD,
    &lkpi_pci_nseg1_fail, "Count of busdma mapping failures of single-segment");

static device_probe_t linux_pci_probe;
static device_attach_t linux_pci_attach;
static device_detach_t linux_pci_detach;
static device_suspend_t linux_pci_suspend;
static device_resume_t linux_pci_resume;
static device_shutdown_t linux_pci_shutdown;
static pci_iov_init_t linux_pci_iov_init;
static pci_iov_uninit_t linux_pci_iov_uninit;
static pci_iov_add_vf_t linux_pci_iov_add_vf;
static int linux_backlight_get_status(device_t dev, struct backlight_props *props);
static int linux_backlight_update_status(device_t dev, struct backlight_props *props);
static int linux_backlight_get_info(device_t dev, struct backlight_info *info);
static void lkpi_pcim_iomap_table_release(struct device *, void *);

static device_method_t pci_methods[] = {
	DEVMETHOD(device_probe, linux_pci_probe),
	DEVMETHOD(device_attach, linux_pci_attach),
	DEVMETHOD(device_detach, linux_pci_detach),
	DEVMETHOD(device_suspend, linux_pci_suspend),
	DEVMETHOD(device_resume, linux_pci_resume),
	DEVMETHOD(device_shutdown, linux_pci_shutdown),
	DEVMETHOD(pci_iov_init, linux_pci_iov_init),
	DEVMETHOD(pci_iov_uninit, linux_pci_iov_uninit),
	DEVMETHOD(pci_iov_add_vf, linux_pci_iov_add_vf),

	/* backlight interface */
	DEVMETHOD(backlight_update_status, linux_backlight_update_status),
	DEVMETHOD(backlight_get_status, linux_backlight_get_status),
	DEVMETHOD(backlight_get_info, linux_backlight_get_info),
	DEVMETHOD_END
};

const char *pci_power_names[] = {
	"UNKNOWN", "D0", "D1", "D2", "D3hot", "D3cold"
};

/* We need some meta-struct to keep track of these for devres. */
struct pci_devres {
	bool		enable_io;
	/* PCIR_MAX_BAR_0 + 1 = 6 => BIT(0..5). */
	uint8_t		region_mask;
	struct resource	*region_table[PCIR_MAX_BAR_0 + 1]; /* Not needed. */
};
struct pcim_iomap_devres {
	void		*mmio_table[PCIR_MAX_BAR_0 + 1];
	struct resource	*res_table[PCIR_MAX_BAR_0 + 1];
};

struct linux_dma_priv {
	uint64_t	dma_mask;
	bus_dma_tag_t	dmat;
	uint64_t	dma_coherent_mask;
	bus_dma_tag_t	dmat_coherent;
	struct mtx	lock;
	struct pctrie	ptree;
};
#define	DMA_PRIV_LOCK(priv) mtx_lock(&(priv)->lock)
#define	DMA_PRIV_UNLOCK(priv) mtx_unlock(&(priv)->lock)

static int
linux_pdev_dma_uninit(struct pci_dev *pdev)
{
	struct linux_dma_priv *priv;

	priv = pdev->dev.dma_priv;
	if (priv->dmat)
		bus_dma_tag_destroy(priv->dmat);
	if (priv->dmat_coherent)
		bus_dma_tag_destroy(priv->dmat_coherent);
	mtx_destroy(&priv->lock);
	pdev->dev.dma_priv = NULL;
	free(priv, M_DEVBUF);
	return (0);
}

static int
linux_pdev_dma_init(struct pci_dev *pdev)
{
	struct linux_dma_priv *priv;
	int error;

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK | M_ZERO);

	mtx_init(&priv->lock, "lkpi-priv-dma", NULL, MTX_DEF);
	pctrie_init(&priv->ptree);

	pdev->dev.dma_priv = priv;

	/* Create a default DMA tags. */
	error = linux_dma_tag_init(&pdev->dev, DMA_BIT_MASK(64));
	if (error != 0)
		goto err;
	/* Coherent is lower 32bit only by default in Linux. */
	error = linux_dma_tag_init_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (error != 0)
		goto err;

	return (error);

err:
	linux_pdev_dma_uninit(pdev);
	return (error);
}

int
linux_dma_tag_init(struct device *dev, u64 dma_mask)
{
	struct linux_dma_priv *priv;
	int error;

	priv = dev->dma_priv;

	if (priv->dmat) {
		if (priv->dma_mask == dma_mask)
			return (0);

		bus_dma_tag_destroy(priv->dmat);
	}

	priv->dma_mask = dma_mask;

	error = bus_dma_tag_create(bus_get_dma_tag(dev->bsddev),
	    1, 0,			/* alignment, boundary */
	    dma_mask,			/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    BUS_SPACE_MAXSIZE,		/* maxsize */
	    1,				/* nsegments */
	    BUS_SPACE_MAXSIZE,		/* maxsegsz */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &priv->dmat);
	return (-error);
}

int
linux_dma_tag_init_coherent(struct device *dev, u64 dma_mask)
{
	struct linux_dma_priv *priv;
	int error;

	priv = dev->dma_priv;

	if (priv->dmat_coherent) {
		if (priv->dma_coherent_mask == dma_mask)
			return (0);

		bus_dma_tag_destroy(priv->dmat_coherent);
	}

	priv->dma_coherent_mask = dma_mask;

	error = bus_dma_tag_create(bus_get_dma_tag(dev->bsddev),
	    1, 0,			/* alignment, boundary */
	    dma_mask,			/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    BUS_SPACE_MAXSIZE,		/* maxsize */
	    1,				/* nsegments */
	    BUS_SPACE_MAXSIZE,		/* maxsegsz */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &priv->dmat_coherent);
	return (-error);
}

static struct pci_driver *
linux_pci_find(device_t dev, const struct pci_device_id **idp)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;
	uint16_t vendor;
	uint16_t device;
	uint16_t subvendor;
	uint16_t subdevice;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);
	subvendor = pci_get_subvendor(dev);
	subdevice = pci_get_subdevice(dev);

	spin_lock(&pci_lock);
	list_for_each_entry(pdrv, &pci_drivers, node) {
		for (id = pdrv->id_table; id->vendor != 0; id++) {
			if (vendor == id->vendor &&
			    (PCI_ANY_ID == id->device || device == id->device) &&
			    (PCI_ANY_ID == id->subvendor || subvendor == id->subvendor) &&
			    (PCI_ANY_ID == id->subdevice || subdevice == id->subdevice)) {
				*idp = id;
				spin_unlock(&pci_lock);
				return (pdrv);
			}
		}
	}
	spin_unlock(&pci_lock);
	return (NULL);
}

struct pci_dev *
lkpi_pci_get_device(uint16_t vendor, uint16_t device, struct pci_dev *odev)
{
	struct pci_dev *pdev;

	KASSERT(odev == NULL, ("%s: odev argument not yet supported\n", __func__));

	spin_lock(&pci_lock);
	list_for_each_entry(pdev, &pci_devices, links) {
		if (pdev->vendor == vendor && pdev->device == device)
			break;
	}
	spin_unlock(&pci_lock);

	return (pdev);
}

static void
lkpi_pci_dev_release(struct device *dev)
{

	lkpi_devres_release_free_list(dev);
	spin_lock_destroy(&dev->devres_lock);
}

static void
lkpifill_pci_dev(device_t dev, struct pci_dev *pdev)
{

	pdev->devfn = PCI_DEVFN(pci_get_slot(dev), pci_get_function(dev));
	pdev->vendor = pci_get_vendor(dev);
	pdev->device = pci_get_device(dev);
	pdev->subsystem_vendor = pci_get_subvendor(dev);
	pdev->subsystem_device = pci_get_subdevice(dev);
	pdev->class = pci_get_class(dev);
	pdev->revision = pci_get_revid(dev);
	pdev->path_name = kasprintf(GFP_KERNEL, "%04d:%02d:%02d.%d",
	    pci_get_domain(dev), pci_get_bus(dev), pci_get_slot(dev),
	    pci_get_function(dev));
	pdev->bus = malloc(sizeof(*pdev->bus), M_DEVBUF, M_WAITOK | M_ZERO);
	/*
	 * This should be the upstream bridge; pci_upstream_bridge()
	 * handles that case on demand as otherwise we'll shadow the
	 * entire PCI hierarchy.
	 */
	pdev->bus->self = pdev;
	pdev->bus->number = pci_get_bus(dev);
	pdev->bus->domain = pci_get_domain(dev);
	pdev->dev.bsddev = dev;
	pdev->dev.parent = &linux_root_device;
	pdev->dev.release = lkpi_pci_dev_release;
	INIT_LIST_HEAD(&pdev->dev.irqents);

	if (pci_msi_count(dev) > 0)
		pdev->msi_desc = malloc(pci_msi_count(dev) *
		    sizeof(*pdev->msi_desc), M_DEVBUF, M_WAITOK | M_ZERO);

	kobject_init(&pdev->dev.kobj, &linux_dev_ktype);
	kobject_set_name(&pdev->dev.kobj, device_get_nameunit(dev));
	kobject_add(&pdev->dev.kobj, &linux_root_device.kobj,
	    kobject_name(&pdev->dev.kobj));
	spin_lock_init(&pdev->dev.devres_lock);
	INIT_LIST_HEAD(&pdev->dev.devres_head);
}

static void
lkpinew_pci_dev_release(struct device *dev)
{
	struct pci_dev *pdev;
	int i;

	pdev = to_pci_dev(dev);
	if (pdev->root != NULL)
		pci_dev_put(pdev->root);
	if (pdev->bus->self != pdev)
		pci_dev_put(pdev->bus->self);
	free(pdev->bus, M_DEVBUF);
	if (pdev->msi_desc != NULL) {
		for (i = pci_msi_count(pdev->dev.bsddev) - 1; i >= 0; i--)
			free(pdev->msi_desc[i], M_DEVBUF);
		free(pdev->msi_desc, M_DEVBUF);
	}
	kfree(pdev->path_name);
	free(pdev, M_DEVBUF);
}

struct pci_dev *
lkpinew_pci_dev(device_t dev)
{
	struct pci_dev *pdev;

	pdev = malloc(sizeof(*pdev), M_DEVBUF, M_WAITOK|M_ZERO);
	lkpifill_pci_dev(dev, pdev);
	pdev->dev.release = lkpinew_pci_dev_release;

	return (pdev);
}

struct pci_dev *
lkpi_pci_get_class(unsigned int class, struct pci_dev *from)
{
	device_t dev;
	device_t devfrom = NULL;
	struct pci_dev *pdev;

	if (from != NULL)
		devfrom = from->dev.bsddev;

	dev = pci_find_class_from(class >> 16, (class >> 8) & 0xFF, devfrom);
	if (dev == NULL)
		return (NULL);

	pdev = lkpinew_pci_dev(dev);
	return (pdev);
}

struct pci_dev *
lkpi_pci_get_domain_bus_and_slot(int domain, unsigned int bus,
    unsigned int devfn)
{
	device_t dev;
	struct pci_dev *pdev;

	dev = pci_find_dbsf(domain, bus, PCI_SLOT(devfn), PCI_FUNC(devfn));
	if (dev == NULL)
		return (NULL);

	pdev = lkpinew_pci_dev(dev);
	return (pdev);
}

static int
linux_pci_probe(device_t dev)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;

	if ((pdrv = linux_pci_find(dev, &id)) == NULL)
		return (ENXIO);
	if (device_get_driver(dev) != &pdrv->bsddriver)
		return (ENXIO);
	device_set_desc(dev, pdrv->name);

	/* Assume BSS initialized (should never return BUS_PROBE_SPECIFIC). */
	if (pdrv->bsd_probe_return == 0)
		return (BUS_PROBE_DEFAULT);
	else
		return (pdrv->bsd_probe_return);
}

static int
linux_pci_attach(device_t dev)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;
	struct pci_dev *pdev;

	pdrv = linux_pci_find(dev, &id);
	pdev = device_get_softc(dev);

	MPASS(pdrv != NULL);
	MPASS(pdev != NULL);

	return (linux_pci_attach_device(dev, pdrv, id, pdev));
}

static struct resource_list_entry *
linux_pci_reserve_bar(struct pci_dev *pdev, struct resource_list *rl,
    int type, int rid)
{
	device_t dev;
	struct resource *res;

	KASSERT(type == SYS_RES_IOPORT || type == SYS_RES_MEMORY,
	    ("trying to reserve non-BAR type %d", type));

	dev = pdev->pdrv != NULL && pdev->pdrv->isdrm ?
	    device_get_parent(pdev->dev.bsddev) : pdev->dev.bsddev;
	res = pci_reserve_map(device_get_parent(dev), dev, type, &rid, 0, ~0,
	    1, 1, 0);
	if (res == NULL)
		return (NULL);
	return (resource_list_find(rl, type, rid));
}

static struct resource_list_entry *
linux_pci_get_rle(struct pci_dev *pdev, int type, int rid, bool reserve_bar)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	dinfo = device_get_ivars(pdev->dev.bsddev);
	rl = &dinfo->resources;
	rle = resource_list_find(rl, type, rid);
	/* Reserve resources for this BAR if needed. */
	if (rle == NULL && reserve_bar)
		rle = linux_pci_reserve_bar(pdev, rl, type, rid);
	return (rle);
}

int
linux_pci_attach_device(device_t dev, struct pci_driver *pdrv,
    const struct pci_device_id *id, struct pci_dev *pdev)
{
	struct resource_list_entry *rle;
	device_t parent;
	uintptr_t rid;
	int error;
	bool isdrm;

	linux_set_current(curthread);

	parent = device_get_parent(dev);
	isdrm = pdrv != NULL && pdrv->isdrm;

	if (isdrm) {
		struct pci_devinfo *dinfo;

		dinfo = device_get_ivars(parent);
		device_set_ivars(dev, dinfo);
	}

	lkpifill_pci_dev(dev, pdev);
	if (isdrm)
		PCI_GET_ID(device_get_parent(parent), parent, PCI_ID_RID, &rid);
	else
		PCI_GET_ID(parent, dev, PCI_ID_RID, &rid);
	pdev->devfn = rid;
	pdev->pdrv = pdrv;
	rle = linux_pci_get_rle(pdev, SYS_RES_IRQ, 0, false);
	if (rle != NULL)
		pdev->dev.irq = rle->start;
	else
		pdev->dev.irq = LINUX_IRQ_INVALID;
	pdev->irq = pdev->dev.irq;
	error = linux_pdev_dma_init(pdev);
	if (error)
		goto out_dma_init;

	TAILQ_INIT(&pdev->mmio);
	spin_lock_init(&pdev->pcie_cap_lock);

	spin_lock(&pci_lock);
	list_add(&pdev->links, &pci_devices);
	spin_unlock(&pci_lock);

	if (pdrv != NULL) {
		error = pdrv->probe(pdev, id);
		if (error)
			goto out_probe;
	}
	return (0);

out_probe:
	free(pdev->bus, M_DEVBUF);
	spin_lock_destroy(&pdev->pcie_cap_lock);
	linux_pdev_dma_uninit(pdev);
out_dma_init:
	spin_lock(&pci_lock);
	list_del(&pdev->links);
	spin_unlock(&pci_lock);
	put_device(&pdev->dev);
	return (-error);
}

static int
linux_pci_detach(device_t dev)
{
	struct pci_dev *pdev;

	pdev = device_get_softc(dev);

	MPASS(pdev != NULL);

	device_set_desc(dev, NULL);

	return (linux_pci_detach_device(pdev));
}

int
linux_pci_detach_device(struct pci_dev *pdev)
{

	linux_set_current(curthread);

	if (pdev->pdrv != NULL)
		pdev->pdrv->remove(pdev);

	if (pdev->root != NULL)
		pci_dev_put(pdev->root);
	free(pdev->bus, M_DEVBUF);
	linux_pdev_dma_uninit(pdev);

	spin_lock(&pci_lock);
	list_del(&pdev->links);
	spin_unlock(&pci_lock);
	spin_lock_destroy(&pdev->pcie_cap_lock);
	put_device(&pdev->dev);

	return (0);
}

static int
lkpi_pci_disable_dev(struct device *dev)
{

	(void) pci_disable_io(dev->bsddev, SYS_RES_MEMORY);
	(void) pci_disable_io(dev->bsddev, SYS_RES_IOPORT);
	return (0);
}

static struct pci_devres *
lkpi_pci_devres_get_alloc(struct pci_dev *pdev)
{
	struct pci_devres *dr;

	dr = lkpi_devres_find(&pdev->dev, lkpi_pci_devres_release, NULL, NULL);
	if (dr == NULL) {
		dr = lkpi_devres_alloc(lkpi_pci_devres_release, sizeof(*dr),
		    GFP_KERNEL | __GFP_ZERO);
		if (dr != NULL)
			lkpi_devres_add(&pdev->dev, dr);
	}

	return (dr);
}

static struct pci_devres *
lkpi_pci_devres_find(struct pci_dev *pdev)
{
	if (!pdev->managed)
		return (NULL);

	return (lkpi_pci_devres_get_alloc(pdev));
}

void
lkpi_pci_devres_release(struct device *dev, void *p)
{
	struct pci_devres *dr;
	struct pci_dev *pdev;
	int bar;

	pdev = to_pci_dev(dev);
	dr = p;

	if (pdev->msix_enabled)
		lkpi_pci_disable_msix(pdev);
        if (pdev->msi_enabled)
		lkpi_pci_disable_msi(pdev);

	if (dr->enable_io && lkpi_pci_disable_dev(dev) == 0)
		dr->enable_io = false;

	if (dr->region_mask == 0)
		return;
	for (bar = PCIR_MAX_BAR_0; bar >= 0; bar--) {

		if ((dr->region_mask & (1 << bar)) == 0)
			continue;
		pci_release_region(pdev, bar);
	}
}

int
linuxkpi_pcim_enable_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;
	int error;

	/* Here we cannot run through the pdev->managed check. */
	dr = lkpi_pci_devres_get_alloc(pdev);
	if (dr == NULL)
		return (-ENOMEM);

	/* If resources were enabled before do not do it again. */
	if (dr->enable_io)
		return (0);

	error = pci_enable_device(pdev);
	if (error == 0)
		dr->enable_io = true;

	/* This device is not managed. */
	pdev->managed = true;

	return (error);
}

static struct pcim_iomap_devres *
lkpi_pcim_iomap_devres_find(struct pci_dev *pdev)
{
	struct pcim_iomap_devres *dr;

	dr = lkpi_devres_find(&pdev->dev, lkpi_pcim_iomap_table_release,
	    NULL, NULL);
	if (dr == NULL) {
		dr = lkpi_devres_alloc(lkpi_pcim_iomap_table_release,
		    sizeof(*dr), GFP_KERNEL | __GFP_ZERO);
		if (dr != NULL)
			lkpi_devres_add(&pdev->dev, dr);
	}

	if (dr == NULL)
		device_printf(pdev->dev.bsddev, "%s: NULL\n", __func__);

	return (dr);
}

void __iomem **
linuxkpi_pcim_iomap_table(struct pci_dev *pdev)
{
	struct pcim_iomap_devres *dr;

	dr = lkpi_pcim_iomap_devres_find(pdev);
	if (dr == NULL)
		return (NULL);

	/*
	 * If the driver has manually set a flag to be able to request the
	 * resource to use bus_read/write_<n>, return the shadow table.
	 */
	if (pdev->want_iomap_res)
		return ((void **)dr->res_table);

	/* This is the Linux default. */
	return (dr->mmio_table);
}

static struct resource *
_lkpi_pci_iomap(struct pci_dev *pdev, int bar, int mmio_size __unused)
{
	struct pci_mmio_region *mmio, *p;
	int type;

	type = pci_resource_type(pdev, bar);
	if (type < 0) {
		device_printf(pdev->dev.bsddev, "%s: bar %d type %d\n",
		     __func__, bar, type);
		return (NULL);
	}

	/*
	 * Check for duplicate mappings.
	 * This can happen if a driver calls pci_request_region() first.
	 */
	TAILQ_FOREACH_SAFE(mmio, &pdev->mmio, next, p) {
		if (mmio->type == type && mmio->rid == PCIR_BAR(bar)) {
			return (mmio->res);
		}
	}

	mmio = malloc(sizeof(*mmio), M_DEVBUF, M_WAITOK | M_ZERO);
	mmio->rid = PCIR_BAR(bar);
	mmio->type = type;
	mmio->res = bus_alloc_resource_any(pdev->dev.bsddev, mmio->type,
	    &mmio->rid, RF_ACTIVE|RF_SHAREABLE);
	if (mmio->res == NULL) {
		device_printf(pdev->dev.bsddev, "%s: failed to alloc "
		    "bar %d type %d rid %d\n",
		    __func__, bar, type, PCIR_BAR(bar));
		free(mmio, M_DEVBUF);
		return (NULL);
	}
	TAILQ_INSERT_TAIL(&pdev->mmio, mmio, next);

	return (mmio->res);
}

void *
linuxkpi_pci_iomap(struct pci_dev *pdev, int mmio_bar, int mmio_size)
{
	struct resource *res;

	res = _lkpi_pci_iomap(pdev, mmio_bar, mmio_size);
	if (res == NULL)
		return (NULL);
	/* This is a FreeBSD extension so we can use bus_*(). */
	if (pdev->want_iomap_res)
		return (res);
	return ((void *)rman_get_bushandle(res));
}

void
linuxkpi_pci_iounmap(struct pci_dev *pdev, void *res)
{
	struct pci_mmio_region *mmio, *p;

	TAILQ_FOREACH_SAFE(mmio, &pdev->mmio, next, p) {
		if (res != (void *)rman_get_bushandle(mmio->res))
			continue;
		bus_release_resource(pdev->dev.bsddev,
		    mmio->type, mmio->rid, mmio->res);
		TAILQ_REMOVE(&pdev->mmio, mmio, next);
		free(mmio, M_DEVBUF);
		return;
	}
}

int
linuxkpi_pcim_iomap_regions(struct pci_dev *pdev, uint32_t mask, const char *name)
{
	struct pcim_iomap_devres *dr;
	void *res;
	uint32_t mappings;
	int bar;

	dr = lkpi_pcim_iomap_devres_find(pdev);
	if (dr == NULL)
		return (-ENOMEM);

	/* Now iomap all the requested (by "mask") ones. */
	for (bar = mappings = 0; mappings != mask; bar++) {
		if ((mask & (1 << bar)) == 0)
			continue;

		/* Request double is not allowed. */
		if (dr->mmio_table[bar] != NULL) {
			device_printf(pdev->dev.bsddev, "%s: bar %d %p\n",
			    __func__, bar, dr->mmio_table[bar]);
			goto err;
		}

		res = _lkpi_pci_iomap(pdev, bar, 0);
		if (res == NULL)
			goto err;
		dr->mmio_table[bar] = (void *)rman_get_bushandle(res);
		dr->res_table[bar] = res;

		mappings |= (1 << bar);
	}

	return (0);
err:
	for (bar = PCIR_MAX_BAR_0; bar >= 0; bar--) {
		if ((mappings & (1 << bar)) != 0) {
			res = dr->mmio_table[bar];
			if (res == NULL)
				continue;
			pci_iounmap(pdev, res);
		}
	}

	return (-EINVAL);
}

static void
lkpi_pcim_iomap_table_release(struct device *dev, void *p)
{
	struct pcim_iomap_devres *dr;
	struct pci_dev *pdev;
	int bar;

	dr = p;
	pdev = to_pci_dev(dev);
	for (bar = PCIR_MAX_BAR_0; bar >= 0; bar--) {

		if (dr->mmio_table[bar] == NULL)
			continue;

		pci_iounmap(pdev, dr->mmio_table[bar]);
	}
}

static int
linux_pci_suspend(device_t dev)
{
	const struct dev_pm_ops *pmops;
	struct pm_message pm = { };
	struct pci_dev *pdev;
	int error;

	error = 0;
	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	pmops = pdev->pdrv->driver.pm;

	if (pdev->pdrv->suspend != NULL)
		error = -pdev->pdrv->suspend(pdev, pm);
	else if (pmops != NULL && pmops->suspend != NULL) {
		error = -pmops->suspend(&pdev->dev);
		if (error == 0 && pmops->suspend_late != NULL)
			error = -pmops->suspend_late(&pdev->dev);
		if (error == 0 && pmops->suspend_noirq != NULL)
			error = -pmops->suspend_noirq(&pdev->dev);
	}
	return (error);
}

static int
linux_pci_resume(device_t dev)
{
	const struct dev_pm_ops *pmops;
	struct pci_dev *pdev;
	int error;

	error = 0;
	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	pmops = pdev->pdrv->driver.pm;

	if (pdev->pdrv->resume != NULL)
		error = -pdev->pdrv->resume(pdev);
	else if (pmops != NULL && pmops->resume != NULL) {
		if (pmops->resume_early != NULL)
			error = -pmops->resume_early(&pdev->dev);
		if (error == 0 && pmops->resume != NULL)
			error = -pmops->resume(&pdev->dev);
	}
	return (error);
}

static int
linux_pci_shutdown(device_t dev)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->shutdown != NULL)
		pdev->pdrv->shutdown(pdev);
	return (0);
}

static int
linux_pci_iov_init(device_t dev, uint16_t num_vfs, const nvlist_t *pf_config)
{
	struct pci_dev *pdev;
	int error;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->bsd_iov_init != NULL)
		error = pdev->pdrv->bsd_iov_init(dev, num_vfs, pf_config);
	else
		error = EINVAL;
	return (error);
}

static void
linux_pci_iov_uninit(device_t dev)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->bsd_iov_uninit != NULL)
		pdev->pdrv->bsd_iov_uninit(dev);
}

static int
linux_pci_iov_add_vf(device_t dev, uint16_t vfnum, const nvlist_t *vf_config)
{
	struct pci_dev *pdev;
	int error;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->bsd_iov_add_vf != NULL)
		error = pdev->pdrv->bsd_iov_add_vf(dev, vfnum, vf_config);
	else
		error = EINVAL;
	return (error);
}

static int
_linux_pci_register_driver(struct pci_driver *pdrv, devclass_t dc)
{
	int error;

	linux_set_current(curthread);
	spin_lock(&pci_lock);
	list_add(&pdrv->node, &pci_drivers);
	spin_unlock(&pci_lock);
	if (pdrv->bsddriver.name == NULL)
		pdrv->bsddriver.name = pdrv->name;
	pdrv->bsddriver.methods = pci_methods;
	pdrv->bsddriver.size = sizeof(struct pci_dev);

	bus_topo_lock();
	error = devclass_add_driver(dc, &pdrv->bsddriver,
	    BUS_PASS_DEFAULT, &pdrv->bsdclass);
	bus_topo_unlock();
	return (-error);
}

int
linux_pci_register_driver(struct pci_driver *pdrv)
{
	devclass_t dc;

	dc = devclass_find("pci");
	if (dc == NULL)
		return (-ENXIO);
	pdrv->isdrm = false;
	return (_linux_pci_register_driver(pdrv, dc));
}

static struct resource_list_entry *
lkpi_pci_get_bar(struct pci_dev *pdev, int bar, bool reserve)
{
	int type;

	type = pci_resource_type(pdev, bar);
	if (type < 0)
		return (NULL);
	bar = PCIR_BAR(bar);
	return (linux_pci_get_rle(pdev, type, bar, reserve));
}

struct device *
lkpi_pci_find_irq_dev(unsigned int irq)
{
	struct pci_dev *pdev;
	struct device *found;

	found = NULL;
	spin_lock(&pci_lock);
	list_for_each_entry(pdev, &pci_devices, links) {
		if (irq == pdev->dev.irq ||
		    (irq >= pdev->dev.irq_start && irq < pdev->dev.irq_end)) {
			found = &pdev->dev;
			break;
		}
	}
	spin_unlock(&pci_lock);
	return (found);
}

unsigned long
pci_resource_start(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;
	rman_res_t newstart;
	device_t dev;
	int error;

	if ((rle = lkpi_pci_get_bar(pdev, bar, true)) == NULL)
		return (0);
	dev = pdev->pdrv != NULL && pdev->pdrv->isdrm ?
	    device_get_parent(pdev->dev.bsddev) : pdev->dev.bsddev;
	error = bus_translate_resource(dev, rle->type, rle->start, &newstart);
	if (error != 0) {
		device_printf(pdev->dev.bsddev,
		    "translate of %#jx failed: %d\n",
		    (uintmax_t)rle->start, error);
		return (0);
	}
	return (newstart);
}

unsigned long
pci_resource_len(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;

	if ((rle = lkpi_pci_get_bar(pdev, bar, true)) == NULL)
		return (0);
	return (rle->count);
}

int
pci_request_region(struct pci_dev *pdev, int bar, const char *res_name)
{
	struct resource *res;
	struct pci_devres *dr;
	struct pci_mmio_region *mmio;
	int rid;
	int type;

	type = pci_resource_type(pdev, bar);
	if (type < 0)
		return (-ENODEV);
	rid = PCIR_BAR(bar);
	res = bus_alloc_resource_any(pdev->dev.bsddev, type, &rid,
	    RF_ACTIVE|RF_SHAREABLE);
	if (res == NULL) {
		device_printf(pdev->dev.bsddev, "%s: failed to alloc "
		    "bar %d type %d rid %d\n",
		    __func__, bar, type, PCIR_BAR(bar));
		return (-ENODEV);
	}

	/*
	 * It seems there is an implicit devres tracking on these if the device
	 * is managed; otherwise the resources are not automatiaclly freed on
	 * FreeBSD/LinuxKPI tough they should be/are expected to be by Linux
	 * drivers.
	 */
	dr = lkpi_pci_devres_find(pdev);
	if (dr != NULL) {
		dr->region_mask |= (1 << bar);
		dr->region_table[bar] = res;
	}

	/* Even if the device is not managed we need to track it for iomap. */
	mmio = malloc(sizeof(*mmio), M_DEVBUF, M_WAITOK | M_ZERO);
	mmio->rid = PCIR_BAR(bar);
	mmio->type = type;
	mmio->res = res;
	TAILQ_INSERT_TAIL(&pdev->mmio, mmio, next);

	return (0);
}

int
linuxkpi_pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	int error;
	int i;

	for (i = 0; i <= PCIR_MAX_BAR_0; i++) {
		error = pci_request_region(pdev, i, res_name);
		if (error && error != -ENODEV) {
			pci_release_regions(pdev);
			return (error);
		}
	}
	return (0);
}

void
linuxkpi_pci_release_region(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;
	struct pci_devres *dr;
	struct pci_mmio_region *mmio, *p;

	if ((rle = lkpi_pci_get_bar(pdev, bar, false)) == NULL)
		return;

	/*
	 * As we implicitly track the requests we also need to clear them on
	 * release.  Do clear before resource release.
	 */
	dr = lkpi_pci_devres_find(pdev);
	if (dr != NULL) {
		KASSERT(dr->region_table[bar] == rle->res, ("%s: pdev %p bar %d"
		    " region_table res %p != rel->res %p\n", __func__, pdev,
		    bar, dr->region_table[bar], rle->res));
		dr->region_table[bar] = NULL;
		dr->region_mask &= ~(1 << bar);
	}

	TAILQ_FOREACH_SAFE(mmio, &pdev->mmio, next, p) {
		if (rle->res != (void *)rman_get_bushandle(mmio->res))
			continue;
		TAILQ_REMOVE(&pdev->mmio, mmio, next);
		free(mmio, M_DEVBUF);
	}

	bus_release_resource(pdev->dev.bsddev, rle->type, rle->rid, rle->res);
}

void
linuxkpi_pci_release_regions(struct pci_dev *pdev)
{
	int i;

	for (i = 0; i <= PCIR_MAX_BAR_0; i++)
		pci_release_region(pdev, i);
}

int
linux_pci_register_drm_driver(struct pci_driver *pdrv)
{
	devclass_t dc;

	dc = devclass_create("vgapci");
	if (dc == NULL)
		return (-ENXIO);
	pdrv->isdrm = true;
	pdrv->name = "drmn";
	return (_linux_pci_register_driver(pdrv, dc));
}

void
linux_pci_unregister_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	bus = devclass_find("pci");

	spin_lock(&pci_lock);
	list_del(&pdrv->node);
	spin_unlock(&pci_lock);
	bus_topo_lock();
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->bsddriver);
	bus_topo_unlock();
}

void
linux_pci_unregister_drm_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	bus = devclass_find("vgapci");

	spin_lock(&pci_lock);
	list_del(&pdrv->node);
	spin_unlock(&pci_lock);
	bus_topo_lock();
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->bsddriver);
	bus_topo_unlock();
}

int
linuxkpi_pci_enable_msix(struct pci_dev *pdev, struct msix_entry *entries,
    int nreq)
{
	struct resource_list_entry *rle;
	int error;
	int avail;
	int i;

	avail = pci_msix_count(pdev->dev.bsddev);
	if (avail < nreq) {
		if (avail == 0)
			return -EINVAL;
		return avail;
	}
	avail = nreq;
	if ((error = -pci_alloc_msix(pdev->dev.bsddev, &avail)) != 0)
		return error;
	/*
	* Handle case where "pci_alloc_msix()" may allocate less
	* interrupts than available and return with no error:
	*/
	if (avail < nreq) {
		pci_release_msi(pdev->dev.bsddev);
		return avail;
	}
	rle = linux_pci_get_rle(pdev, SYS_RES_IRQ, 1, false);
	pdev->dev.irq_start = rle->start;
	pdev->dev.irq_end = rle->start + avail;
	for (i = 0; i < nreq; i++)
		entries[i].vector = pdev->dev.irq_start + i;
	pdev->msix_enabled = true;
	return (0);
}

int
_lkpi_pci_enable_msi_range(struct pci_dev *pdev, int minvec, int maxvec)
{
	struct resource_list_entry *rle;
	int error;
	int nvec;

	if (maxvec < minvec)
		return (-EINVAL);

	nvec = pci_msi_count(pdev->dev.bsddev);
	if (nvec < 1 || nvec < minvec)
		return (-ENOSPC);

	nvec = min(nvec, maxvec);
	if ((error = -pci_alloc_msi(pdev->dev.bsddev, &nvec)) != 0)
		return error;

	/* Native PCI might only ever ask for 32 vectors. */
	if (nvec < minvec) {
		pci_release_msi(pdev->dev.bsddev);
		return (-ENOSPC);
	}

	rle = linux_pci_get_rle(pdev, SYS_RES_IRQ, 1, false);
	pdev->dev.irq_start = rle->start;
	pdev->dev.irq_end = rle->start + nvec;
	pdev->irq = rle->start;
	pdev->msi_enabled = true;
	return (0);
}

int
pci_alloc_irq_vectors(struct pci_dev *pdev, int minv, int maxv,
    unsigned int flags)
{
	int error;

	if (flags & PCI_IRQ_MSIX) {
		struct msix_entry *entries;
		int i;

		entries = kcalloc(maxv, sizeof(*entries), GFP_KERNEL);
		if (entries == NULL) {
			error = -ENOMEM;
			goto out;
		}
		for (i = 0; i < maxv; ++i)
			entries[i].entry = i;
		error = pci_enable_msix(pdev, entries, maxv);
out:
		kfree(entries);
		if (error == 0 && pdev->msix_enabled)
			return (pdev->dev.irq_end - pdev->dev.irq_start);
	}
	if (flags & PCI_IRQ_MSI) {
		if (pci_msi_count(pdev->dev.bsddev) < minv)
			return (-ENOSPC);
		error = _lkpi_pci_enable_msi_range(pdev, minv, maxv);
		if (error == 0 && pdev->msi_enabled)
			return (pdev->dev.irq_end - pdev->dev.irq_start);
	}
	if (flags & PCI_IRQ_LEGACY) {
		if (pdev->irq)
			return (1);
	}

	return (-EINVAL);
}

struct msi_desc *
lkpi_pci_msi_desc_alloc(int irq)
{
	struct device *dev;
	struct pci_dev *pdev;
	struct msi_desc *desc;
	struct pci_devinfo *dinfo;
	struct pcicfg_msi *msi;
	int vec;

	dev = lkpi_pci_find_irq_dev(irq);
	if (dev == NULL)
		return (NULL);

	pdev = to_pci_dev(dev);

	if (pdev->msi_desc == NULL)
		return (NULL);

	if (irq < pdev->dev.irq_start || irq >= pdev->dev.irq_end)
		return (NULL);

	vec = pdev->dev.irq_start - irq;

	if (pdev->msi_desc[vec] != NULL)
		return (pdev->msi_desc[vec]);

	dinfo = device_get_ivars(dev->bsddev);
	msi = &dinfo->cfg.msi;

	desc = malloc(sizeof(*desc), M_DEVBUF, M_WAITOK | M_ZERO);

	desc->pci.msi_attrib.is_64 =
	   (msi->msi_ctrl & PCIM_MSICTRL_64BIT) ? true : false;
	desc->msg.data = msi->msi_data;

	pdev->msi_desc[vec] = desc;

	return (desc);
}

bool
pci_device_is_present(struct pci_dev *pdev)
{
	device_t dev;

	dev = pdev->dev.bsddev;

	return (bus_child_present(dev));
}

CTASSERT(sizeof(dma_addr_t) <= sizeof(uint64_t));

struct linux_dma_obj {
	void		*vaddr;
	uint64_t	dma_addr;
	bus_dmamap_t	dmamap;
	bus_dma_tag_t	dmat;
};

static uma_zone_t linux_dma_trie_zone;
static uma_zone_t linux_dma_obj_zone;

static void
linux_dma_init(void *arg)
{

	linux_dma_trie_zone = uma_zcreate("linux_dma_pctrie",
	    pctrie_node_size(), NULL, NULL, pctrie_zone_init, NULL,
	    UMA_ALIGN_PTR, 0);
	linux_dma_obj_zone = uma_zcreate("linux_dma_object",
	    sizeof(struct linux_dma_obj), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	lkpi_pci_nseg1_fail = counter_u64_alloc(M_WAITOK);
}
SYSINIT(linux_dma, SI_SUB_DRIVERS, SI_ORDER_THIRD, linux_dma_init, NULL);

static void
linux_dma_uninit(void *arg)
{

	counter_u64_free(lkpi_pci_nseg1_fail);
	uma_zdestroy(linux_dma_obj_zone);
	uma_zdestroy(linux_dma_trie_zone);
}
SYSUNINIT(linux_dma, SI_SUB_DRIVERS, SI_ORDER_THIRD, linux_dma_uninit, NULL);

static void *
linux_dma_trie_alloc(struct pctrie *ptree)
{

	return (uma_zalloc(linux_dma_trie_zone, M_NOWAIT));
}

static void
linux_dma_trie_free(struct pctrie *ptree, void *node)
{

	uma_zfree(linux_dma_trie_zone, node);
}

PCTRIE_DEFINE(LINUX_DMA, linux_dma_obj, dma_addr, linux_dma_trie_alloc,
    linux_dma_trie_free);

#if defined(__i386__) || defined(__amd64__) || defined(__aarch64__)
static dma_addr_t
linux_dma_map_phys_common(struct device *dev, vm_paddr_t phys, size_t len,
    bus_dma_tag_t dmat)
{
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;
	int error, nseg;
	bus_dma_segment_t seg;

	priv = dev->dma_priv;

	/*
	 * If the resultant mapping will be entirely 1:1 with the
	 * physical address, short-circuit the remainder of the
	 * bus_dma API.  This avoids tracking collisions in the pctrie
	 * with the additional benefit of reducing overhead.
	 */
	if (bus_dma_id_mapped(dmat, phys, len))
		return (phys);

	obj = uma_zalloc(linux_dma_obj_zone, M_NOWAIT);
	if (obj == NULL) {
		return (0);
	}
	obj->dmat = dmat;

	DMA_PRIV_LOCK(priv);
	if (bus_dmamap_create(obj->dmat, 0, &obj->dmamap) != 0) {
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		return (0);
	}

	nseg = -1;
	if (_bus_dmamap_load_phys(obj->dmat, obj->dmamap, phys, len,
	    BUS_DMA_NOWAIT, &seg, &nseg) != 0) {
		bus_dmamap_destroy(obj->dmat, obj->dmamap);
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		counter_u64_add(lkpi_pci_nseg1_fail, 1);
		if (linuxkpi_debug)
			dump_stack();
		return (0);
	}

	KASSERT(++nseg == 1, ("More than one segment (nseg=%d)", nseg));
	obj->dma_addr = seg.ds_addr;

	error = LINUX_DMA_PCTRIE_INSERT(&priv->ptree, obj);
	if (error != 0) {
		bus_dmamap_unload(obj->dmat, obj->dmamap);
		bus_dmamap_destroy(obj->dmat, obj->dmamap);
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		return (0);
	}
	DMA_PRIV_UNLOCK(priv);
	return (obj->dma_addr);
}
#else
static dma_addr_t
linux_dma_map_phys_common(struct device *dev __unused, vm_paddr_t phys,
    size_t len __unused, bus_dma_tag_t dmat __unused)
{
	return (phys);
}
#endif

dma_addr_t
linux_dma_map_phys(struct device *dev, vm_paddr_t phys, size_t len)
{
	struct linux_dma_priv *priv;

	priv = dev->dma_priv;
	return (linux_dma_map_phys_common(dev, phys, len, priv->dmat));
}

#if defined(__i386__) || defined(__amd64__) || defined(__aarch64__)
void
linux_dma_unmap(struct device *dev, dma_addr_t dma_addr, size_t len)
{
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;

	priv = dev->dma_priv;

	if (pctrie_is_empty(&priv->ptree))
		return;

	DMA_PRIV_LOCK(priv);
	obj = LINUX_DMA_PCTRIE_LOOKUP(&priv->ptree, dma_addr);
	if (obj == NULL) {
		DMA_PRIV_UNLOCK(priv);
		return;
	}
	LINUX_DMA_PCTRIE_REMOVE(&priv->ptree, dma_addr);
	bus_dmamap_unload(obj->dmat, obj->dmamap);
	bus_dmamap_destroy(obj->dmat, obj->dmamap);
	DMA_PRIV_UNLOCK(priv);

	uma_zfree(linux_dma_obj_zone, obj);
}
#else
void
linux_dma_unmap(struct device *dev, dma_addr_t dma_addr, size_t len)
{
}
#endif

void *
linux_dma_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t flag)
{
	struct linux_dma_priv *priv;
	vm_paddr_t high;
	size_t align;
	void *mem;

	if (dev == NULL || dev->dma_priv == NULL) {
		*dma_handle = 0;
		return (NULL);
	}
	priv = dev->dma_priv;
	if (priv->dma_coherent_mask)
		high = priv->dma_coherent_mask;
	else
		/* Coherent is lower 32bit only by default in Linux. */
		high = BUS_SPACE_MAXADDR_32BIT;
	align = PAGE_SIZE << get_order(size);
	/* Always zero the allocation. */
	flag |= M_ZERO;
	mem = kmem_alloc_contig(size, flag & GFP_NATIVE_MASK, 0, high,
	    align, 0, VM_MEMATTR_DEFAULT);
	if (mem != NULL) {
		*dma_handle = linux_dma_map_phys_common(dev, vtophys(mem), size,
		    priv->dmat_coherent);
		if (*dma_handle == 0) {
			kmem_free(mem, size);
			mem = NULL;
		}
	} else {
		*dma_handle = 0;
	}
	return (mem);
}

struct lkpi_devres_dmam_coherent {
	size_t size;
	dma_addr_t *handle;
	void *mem;
};

static void
lkpi_dmam_free_coherent(struct device *dev, void *p)
{
	struct lkpi_devres_dmam_coherent *dr;

	dr = p;
	dma_free_coherent(dev, dr->size, dr->mem, *dr->handle);
}

void *
linuxkpi_dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t flag)
{
	struct lkpi_devres_dmam_coherent *dr;

	dr = lkpi_devres_alloc(lkpi_dmam_free_coherent,
	    sizeof(*dr), GFP_KERNEL | __GFP_ZERO);

	if (dr == NULL)
		return (NULL);

	dr->size = size;
	dr->mem = linux_dma_alloc_coherent(dev, size, dma_handle, flag);
	dr->handle = dma_handle;
	if (dr->mem == NULL) {
		lkpi_devres_free(dr);
		return (NULL);
	}

	lkpi_devres_add(dev, dr);
	return (dr->mem);
}

void
linuxkpi_dma_sync(struct device *dev, dma_addr_t dma_addr, size_t size,
    bus_dmasync_op_t op)
{
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;

	priv = dev->dma_priv;

	if (pctrie_is_empty(&priv->ptree))
		return;

	DMA_PRIV_LOCK(priv);
	obj = LINUX_DMA_PCTRIE_LOOKUP(&priv->ptree, dma_addr);
	if (obj == NULL) {
		DMA_PRIV_UNLOCK(priv);
		return;
	}

	bus_dmamap_sync(obj->dmat, obj->dmamap, op);
	DMA_PRIV_UNLOCK(priv);
}

int
linux_dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction direction, unsigned long attrs __unused)
{
	struct linux_dma_priv *priv;
	struct scatterlist *sg;
	int i, nseg;
	bus_dma_segment_t seg;

	priv = dev->dma_priv;

	DMA_PRIV_LOCK(priv);

	/* create common DMA map in the first S/G entry */
	if (bus_dmamap_create(priv->dmat, 0, &sgl->dma_map) != 0) {
		DMA_PRIV_UNLOCK(priv);
		return (0);
	}

	/* load all S/G list entries */
	for_each_sg(sgl, sg, nents, i) {
		nseg = -1;
		if (_bus_dmamap_load_phys(priv->dmat, sgl->dma_map,
		    sg_phys(sg), sg->length, BUS_DMA_NOWAIT,
		    &seg, &nseg) != 0) {
			bus_dmamap_unload(priv->dmat, sgl->dma_map);
			bus_dmamap_destroy(priv->dmat, sgl->dma_map);
			DMA_PRIV_UNLOCK(priv);
			return (0);
		}
		KASSERT(nseg == 0,
		    ("More than one segment (nseg=%d)", nseg + 1));

		sg_dma_address(sg) = seg.ds_addr;
	}

	switch (direction) {
	case DMA_BIDIRECTIONAL:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_PREWRITE);
		break;
	case DMA_TO_DEVICE:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_PREREAD);
		break;
	case DMA_FROM_DEVICE:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_PREWRITE);
		break;
	default:
		break;
	}

	DMA_PRIV_UNLOCK(priv);

	return (nents);
}

void
linux_dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sgl,
    int nents __unused, enum dma_data_direction direction,
    unsigned long attrs __unused)
{
	struct linux_dma_priv *priv;

	priv = dev->dma_priv;

	DMA_PRIV_LOCK(priv);

	switch (direction) {
	case DMA_BIDIRECTIONAL:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_PREREAD);
		break;
	case DMA_TO_DEVICE:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_POSTWRITE);
		break;
	case DMA_FROM_DEVICE:
		bus_dmamap_sync(priv->dmat, sgl->dma_map, BUS_DMASYNC_POSTREAD);
		break;
	default:
		break;
	}

	bus_dmamap_unload(priv->dmat, sgl->dma_map);
	bus_dmamap_destroy(priv->dmat, sgl->dma_map);
	DMA_PRIV_UNLOCK(priv);
}

struct dma_pool {
	struct device  *pool_device;
	uma_zone_t	pool_zone;
	struct mtx	pool_lock;
	bus_dma_tag_t	pool_dmat;
	size_t		pool_entry_size;
	struct pctrie	pool_ptree;
};

#define	DMA_POOL_LOCK(pool) mtx_lock(&(pool)->pool_lock)
#define	DMA_POOL_UNLOCK(pool) mtx_unlock(&(pool)->pool_lock)

static inline int
dma_pool_obj_ctor(void *mem, int size, void *arg, int flags)
{
	struct linux_dma_obj *obj = mem;
	struct dma_pool *pool = arg;
	int error, nseg;
	bus_dma_segment_t seg;

	nseg = -1;
	DMA_POOL_LOCK(pool);
	error = _bus_dmamap_load_phys(pool->pool_dmat, obj->dmamap,
	    vtophys(obj->vaddr), pool->pool_entry_size, BUS_DMA_NOWAIT,
	    &seg, &nseg);
	DMA_POOL_UNLOCK(pool);
	if (error != 0) {
		return (error);
	}
	KASSERT(++nseg == 1, ("More than one segment (nseg=%d)", nseg));
	obj->dma_addr = seg.ds_addr;

	return (0);
}

static void
dma_pool_obj_dtor(void *mem, int size, void *arg)
{
	struct linux_dma_obj *obj = mem;
	struct dma_pool *pool = arg;

	DMA_POOL_LOCK(pool);
	bus_dmamap_unload(pool->pool_dmat, obj->dmamap);
	DMA_POOL_UNLOCK(pool);
}

static int
dma_pool_obj_import(void *arg, void **store, int count, int domain __unused,
    int flags)
{
	struct dma_pool *pool = arg;
	struct linux_dma_obj *obj;
	int error, i;

	for (i = 0; i < count; i++) {
		obj = uma_zalloc(linux_dma_obj_zone, flags);
		if (obj == NULL)
			break;

		error = bus_dmamem_alloc(pool->pool_dmat, &obj->vaddr,
		    BUS_DMA_NOWAIT, &obj->dmamap);
		if (error!= 0) {
			uma_zfree(linux_dma_obj_zone, obj);
			break;
		}

		store[i] = obj;
	}

	return (i);
}

static void
dma_pool_obj_release(void *arg, void **store, int count)
{
	struct dma_pool *pool = arg;
	struct linux_dma_obj *obj;
	int i;

	for (i = 0; i < count; i++) {
		obj = store[i];
		bus_dmamem_free(pool->pool_dmat, obj->vaddr, obj->dmamap);
		uma_zfree(linux_dma_obj_zone, obj);
	}
}

struct dma_pool *
linux_dma_pool_create(char *name, struct device *dev, size_t size,
    size_t align, size_t boundary)
{
	struct linux_dma_priv *priv;
	struct dma_pool *pool;

	priv = dev->dma_priv;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	pool->pool_device = dev;
	pool->pool_entry_size = size;

	if (bus_dma_tag_create(bus_get_dma_tag(dev->bsddev),
	    align, boundary,		/* alignment, boundary */
	    priv->dma_mask,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    size,			/* maxsize */
	    1,				/* nsegments */
	    size,			/* maxsegsz */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &pool->pool_dmat)) {
		kfree(pool);
		return (NULL);
	}

	pool->pool_zone = uma_zcache_create(name, -1, dma_pool_obj_ctor,
	    dma_pool_obj_dtor, NULL, NULL, dma_pool_obj_import,
	    dma_pool_obj_release, pool, 0);

	mtx_init(&pool->pool_lock, "lkpi-dma-pool", NULL, MTX_DEF);
	pctrie_init(&pool->pool_ptree);

	return (pool);
}

void
linux_dma_pool_destroy(struct dma_pool *pool)
{

	uma_zdestroy(pool->pool_zone);
	bus_dma_tag_destroy(pool->pool_dmat);
	mtx_destroy(&pool->pool_lock);
	kfree(pool);
}

void
lkpi_dmam_pool_destroy(struct device *dev, void *p)
{
	struct dma_pool *pool;

	pool = *(struct dma_pool **)p;
	LINUX_DMA_PCTRIE_RECLAIM(&pool->pool_ptree);
	linux_dma_pool_destroy(pool);
}

void *
linux_dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
    dma_addr_t *handle)
{
	struct linux_dma_obj *obj;

	obj = uma_zalloc_arg(pool->pool_zone, pool, mem_flags & GFP_NATIVE_MASK);
	if (obj == NULL)
		return (NULL);

	DMA_POOL_LOCK(pool);
	if (LINUX_DMA_PCTRIE_INSERT(&pool->pool_ptree, obj) != 0) {
		DMA_POOL_UNLOCK(pool);
		uma_zfree_arg(pool->pool_zone, obj, pool);
		return (NULL);
	}
	DMA_POOL_UNLOCK(pool);

	*handle = obj->dma_addr;
	return (obj->vaddr);
}

void
linux_dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma_addr)
{
	struct linux_dma_obj *obj;

	DMA_POOL_LOCK(pool);
	obj = LINUX_DMA_PCTRIE_LOOKUP(&pool->pool_ptree, dma_addr);
	if (obj == NULL) {
		DMA_POOL_UNLOCK(pool);
		return;
	}
	LINUX_DMA_PCTRIE_REMOVE(&pool->pool_ptree, dma_addr);
	DMA_POOL_UNLOCK(pool);

	uma_zfree_arg(pool->pool_zone, obj, pool);
}

static int
linux_backlight_get_status(device_t dev, struct backlight_props *props)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);

	props->brightness = pdev->dev.bd->props.brightness;
	props->brightness = props->brightness * 100 / pdev->dev.bd->props.max_brightness;
	props->nlevels = 0;

	return (0);
}

static int
linux_backlight_get_info(device_t dev, struct backlight_info *info)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);

	info->type = BACKLIGHT_TYPE_PANEL;
	strlcpy(info->name, pdev->dev.bd->name, BACKLIGHTMAXNAMELENGTH);
	return (0);
}

static int
linux_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);

	pdev->dev.bd->props.brightness = pdev->dev.bd->props.max_brightness *
		props->brightness / 100;
	pdev->dev.bd->props.power = props->brightness == 0 ?
		4/* FB_BLANK_POWERDOWN */ : 0/* FB_BLANK_UNBLANK */;
	return (pdev->dev.bd->ops->update_status(pdev->dev.bd));
}

struct backlight_device *
linux_backlight_device_register(const char *name, struct device *dev,
    void *data, const struct backlight_ops *ops, struct backlight_properties *props)
{

	dev->bd = malloc(sizeof(*dev->bd), M_DEVBUF, M_WAITOK | M_ZERO);
	dev->bd->ops = ops;
	dev->bd->props.type = props->type;
	dev->bd->props.max_brightness = props->max_brightness;
	dev->bd->props.brightness = props->brightness;
	dev->bd->props.power = props->power;
	dev->bd->data = data;
	dev->bd->dev = dev;
	dev->bd->name = strdup(name, M_DEVBUF);

	dev->backlight_dev = backlight_register(name, dev->bsddev);

	return (dev->bd);
}

void
linux_backlight_device_unregister(struct backlight_device *bd)
{

	backlight_destroy(bd->dev->backlight_dev);
	free(bd->name, M_DEVBUF);
	free(bd, M_DEVBUF);
}
