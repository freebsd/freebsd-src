/*-
 * Copyright (c) 2015-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
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
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/pctrie.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

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

static device_probe_t linux_pci_probe;
static device_attach_t linux_pci_attach;
static device_detach_t linux_pci_detach;
static device_suspend_t linux_pci_suspend;
static device_resume_t linux_pci_resume;
static device_shutdown_t linux_pci_shutdown;

static device_method_t pci_methods[] = {
	DEVMETHOD(device_probe, linux_pci_probe),
	DEVMETHOD(device_attach, linux_pci_attach),
	DEVMETHOD(device_detach, linux_pci_detach),
	DEVMETHOD(device_suspend, linux_pci_suspend),
	DEVMETHOD(device_resume, linux_pci_resume),
	DEVMETHOD(device_shutdown, linux_pci_shutdown),
	DEVMETHOD_END
};

struct linux_dma_priv {
	uint64_t	dma_mask;
	struct mtx	lock;
	bus_dma_tag_t	dmat;
	struct pctrie	ptree;
};
#define	DMA_PRIV_LOCK(priv) mtx_lock(&(priv)->lock)
#define	DMA_PRIV_UNLOCK(priv) mtx_unlock(&(priv)->lock)

static int
linux_pdev_dma_init(struct pci_dev *pdev)
{
	struct linux_dma_priv *priv;

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK | M_ZERO);
	pdev->dev.dma_priv = priv;

	mtx_init(&priv->lock, "lkpi-priv-dma", NULL, MTX_DEF);

	pctrie_init(&priv->ptree);

	return (0);
}

static int
linux_pdev_dma_uninit(struct pci_dev *pdev)
{
	struct linux_dma_priv *priv;

	priv = pdev->dev.dma_priv;
	if (priv->dmat)
		bus_dma_tag_destroy(priv->dmat);
	mtx_destroy(&priv->lock);
	free(priv, M_DEVBUF);
	pdev->dev.dma_priv = NULL;
	return (0);
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
	list_for_each_entry(pdrv, &pci_drivers, links) {
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
	struct resource_list_entry *rle;
	struct pci_bus *pbus;
	struct pci_dev *pdev;
	struct pci_devinfo *dinfo;
	struct pci_driver *pdrv;
	const struct pci_device_id *id;
	device_t parent;
	devclass_t devclass;
	int error;

	linux_set_current(curthread);

	pdrv = linux_pci_find(dev, &id);
	pdev = device_get_softc(dev);

	parent = device_get_parent(dev);
	devclass = device_get_devclass(parent);
	if (pdrv->isdrm) {
		dinfo = device_get_ivars(parent);
		device_set_ivars(dev, dinfo);
	} else {
		dinfo = device_get_ivars(dev);
	}

	pdev->dev.parent = &linux_root_device;
	pdev->dev.bsddev = dev;
	INIT_LIST_HEAD(&pdev->dev.irqents);
	pdev->devfn = PCI_DEVFN(pci_get_slot(dev), pci_get_function(dev));
	pdev->device = dinfo->cfg.device;
	pdev->vendor = dinfo->cfg.vendor;
	pdev->subsystem_vendor = dinfo->cfg.subvendor;
	pdev->subsystem_device = dinfo->cfg.subdevice;
	pdev->class = pci_get_class(dev);
	pdev->revision = pci_get_revid(dev);
	pdev->pdrv = pdrv;
	kobject_init(&pdev->dev.kobj, &linux_dev_ktype);
	kobject_set_name(&pdev->dev.kobj, device_get_nameunit(dev));
	kobject_add(&pdev->dev.kobj, &linux_root_device.kobj,
	    kobject_name(&pdev->dev.kobj));
	rle = linux_pci_get_rle(pdev, SYS_RES_IRQ, 0);
	if (rle != NULL)
		pdev->dev.irq = rle->start;
	else
		pdev->dev.irq = LINUX_IRQ_INVALID;
	pdev->irq = pdev->dev.irq;
	error = linux_pdev_dma_init(pdev);
	if (error)
		goto out;

	if (pdev->bus == NULL) {
		pbus = malloc(sizeof(*pbus), M_DEVBUF, M_WAITOK | M_ZERO);
		pbus->self = pdev;
		pbus->number = pci_get_bus(dev);
		pdev->bus = pbus;
	}

	spin_lock(&pci_lock);
	list_add(&pdev->links, &pci_devices);
	spin_unlock(&pci_lock);

	error = pdrv->probe(pdev, id);
out:
	if (error) {
		spin_lock(&pci_lock);
		list_del(&pdev->links);
		spin_unlock(&pci_lock);
		put_device(&pdev->dev);
		error = -error;
	}
	return (error);
}

static int
linux_pci_detach(device_t dev)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);

	pdev->pdrv->remove(pdev);
	linux_pdev_dma_uninit(pdev);

	spin_lock(&pci_lock);
	list_del(&pdev->links);
	spin_unlock(&pci_lock);
	device_set_desc(dev, NULL);
	put_device(&pdev->dev);

	return (0);
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
_linux_pci_register_driver(struct pci_driver *pdrv, devclass_t dc)
{
	int error;

	linux_set_current(curthread);
	spin_lock(&pci_lock);
	list_add(&pdrv->links, &pci_drivers);
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
	list_del(&pdrv->links);
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

	return (uma_zalloc(linux_dma_trie_zone, 0));
}

static void
linux_dma_trie_free(struct pctrie *ptree, void *node)
{

	uma_zfree(linux_dma_trie_zone, node);
}


PCTRIE_DEFINE(LINUX_DMA, linux_dma_obj, dma_addr, linux_dma_trie_alloc,
    linux_dma_trie_free);

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
	if (priv->dma_mask)
		high = priv->dma_mask;
	else if (flag & GFP_DMA32)
		high = BUS_SPACE_MAXADDR_32BIT;
	else
		high = BUS_SPACE_MAXADDR;
	align = PAGE_SIZE << get_order(size);
	mem = (void *)kmem_alloc_contig(size, flag, 0, high, align, 0,
	    VM_MEMATTR_DEFAULT);
	if (mem != NULL) {
		*dma_handle = linux_dma_map_phys(dev, vtophys(mem), size);
		if (*dma_handle == 0) {
			kmem_free((vm_offset_t)mem, size);
			mem = NULL;
		}
	} else {
		*dma_handle = 0;
	}
	return (mem);
}

dma_addr_t
linux_dma_map_phys(struct device *dev, vm_paddr_t phys, size_t len)
{
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;
	int error, nseg;
	bus_dma_segment_t seg;

	priv = dev->dma_priv;

	obj = uma_zalloc(linux_dma_obj_zone, 0);

	DMA_PRIV_LOCK(priv);
	if (bus_dmamap_create(priv->dmat, 0, &obj->dmamap) != 0) {
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		return (0);
	}

	nseg = -1;
	if (_bus_dmamap_load_phys(priv->dmat, obj->dmamap, phys, len,
	    BUS_DMA_NOWAIT, &seg, &nseg) != 0) {
		bus_dmamap_destroy(priv->dmat, obj->dmamap);
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		return (0);
	}

	KASSERT(++nseg == 1, ("More than one segment (nseg=%d)", nseg));
	obj->dma_addr = seg.ds_addr;

	error = LINUX_DMA_PCTRIE_INSERT(&priv->ptree, obj);
	if (error != 0) {
		bus_dmamap_unload(priv->dmat, obj->dmamap);
		bus_dmamap_destroy(priv->dmat, obj->dmamap);
		DMA_PRIV_UNLOCK(priv);
		uma_zfree(linux_dma_obj_zone, obj);
		return (0);
	}
	DMA_PRIV_UNLOCK(priv);
	return (obj->dma_addr);
}

void
linux_dma_unmap(struct device *dev, dma_addr_t dma_addr, size_t len)
{
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;

	priv = dev->dma_priv;

	DMA_PRIV_LOCK(priv);
	obj = LINUX_DMA_PCTRIE_LOOKUP(&priv->ptree, dma_addr);
	if (obj == NULL) {
		DMA_PRIV_UNLOCK(priv);
		return;
	}
	LINUX_DMA_PCTRIE_REMOVE(&priv->ptree, dma_addr);
	bus_dmamap_unload(priv->dmat, obj->dmamap);
	bus_dmamap_destroy(priv->dmat, obj->dmamap);
	DMA_PRIV_UNLOCK(priv);

	uma_zfree(linux_dma_obj_zone, obj);
}

int
linux_dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir, struct dma_attrs *attrs)
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
    int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
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
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;
	int error, i;

	priv = pool->pool_device->dma_priv;
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
	struct linux_dma_priv *priv;
	struct linux_dma_obj *obj;
	int i;

	priv = pool->pool_device->dma_priv;
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

void *
linux_dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
    dma_addr_t *handle)
{
	struct linux_dma_obj *obj;

	obj = uma_zalloc_arg(pool->pool_zone, pool, mem_flags);
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
