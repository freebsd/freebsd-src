/*-
 * Copyright (c) 2015-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 * Copyright (c) 2020-2021 The FreeBSD Foundation
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pci_iov.h>
#include <dev/backlight/backlight.h>

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
	pdev->bus = malloc(sizeof(*pdev->bus), M_DEVBUF, M_WAITOK | M_ZERO);
	pdev->bus->self = pdev;
	pdev->bus->number = pci_get_bus(dev);
	pdev->bus->domain = pci_get_domain(dev);
	pdev->dev.bsddev = dev;
	pdev->dev.parent = &linux_root_device;
	pdev->dev.release = lkpi_pci_dev_release;
	INIT_LIST_HEAD(&pdev->dev.irqents);
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

	pdev = to_pci_dev(dev);
	if (pdev->root != NULL)
		pci_dev_put(pdev->root);
	free(pdev->bus, M_DEVBUF);
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
	return (0);
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

void
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
	pdrv->bsddriver.name = pdrv->name;
	pdrv->bsddriver.methods = pci_methods;
	pdrv->bsddriver.size = sizeof(struct pci_dev);

	mtx_lock(&Giant);
	error = devclass_add_driver(dc, &pdrv->bsddriver,
	    BUS_PASS_DEFAULT, &pdrv->bsdclass);
	mtx_unlock(&Giant);
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

struct resource_list_entry *
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

unsigned long
pci_resource_start(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;
	rman_res_t newstart;
	device_t dev;
	int error;

	if ((rle = linux_pci_get_bar(pdev, bar, true)) == NULL)
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

	if ((rle = linux_pci_get_bar(pdev, bar, true)) == NULL)
		return (0);
	return (rle->count);
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
	mtx_lock(&Giant);
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->bsddriver);
	mtx_unlock(&Giant);
}

void
linux_pci_unregister_drm_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	bus = devclass_find("vgapci");

	spin_lock(&pci_lock);
	list_del(&pdrv->node);
	spin_unlock(&pci_lock);
	mtx_lock(&Giant);
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->bsddriver);
	mtx_unlock(&Giant);
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

}
SYSINIT(linux_dma, SI_SUB_DRIVERS, SI_ORDER_THIRD, linux_dma_init, NULL);

static void
linux_dma_uninit(void *arg)
{

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
	mem = (void *)kmem_alloc_contig(size, flag & GFP_NATIVE_MASK, 0, high,
	    align, 0, VM_MEMATTR_DEFAULT);
	if (mem != NULL) {
		*dma_handle = linux_dma_map_phys_common(dev, vtophys(mem), size,
		    priv->dmat_coherent);
		if (*dma_handle == 0) {
			kmem_free((vm_offset_t)mem, size);
			mem = NULL;
		}
	} else {
		*dma_handle = 0;
	}
	return (mem);
}

int
linux_dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir __unused, unsigned long attrs __unused)
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
	DMA_PRIV_UNLOCK(priv);

	return (nents);
}

void
linux_dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sgl,
    int nents __unused, enum dma_data_direction dir __unused,
    unsigned long attrs __unused)
{
	struct linux_dma_priv *priv;

	priv = dev->dma_priv;

	DMA_PRIV_LOCK(priv);
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
