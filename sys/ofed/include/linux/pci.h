/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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

#ifndef	_LINUX_PCI_H_
#define	_LINUX_PCI_H_

#define	CONFIG_PCI_MSI

#include <linux/types.h>

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pciio.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>

#include <machine/resource.h>

#include <linux/list.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <asm/atomic.h>
#include <linux/device.h>

struct pci_device_id {
	uint32_t	vendor;
	uint32_t	device;
        uint32_t	subvendor;
	uint32_t	subdevice;
	uint32_t	class_mask;
	uintptr_t	driver_data;
};

#define	MODULE_DEVICE_TABLE(bus, table)
#define	PCI_ANY_ID		(-1)
#define	PCI_VENDOR_ID_MELLANOX			0x15b3
#define	PCI_VENDOR_ID_TOPSPIN			0x1867
#define	PCI_DEVICE_ID_MELLANOX_TAVOR		0x5a44
#define	PCI_DEVICE_ID_MELLANOX_TAVOR_BRIDGE	0x5a46
#define	PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT	0x6278
#define	PCI_DEVICE_ID_MELLANOX_ARBEL		0x6282
#define	PCI_DEVICE_ID_MELLANOX_SINAI_OLD	0x5e8c
#define	PCI_DEVICE_ID_MELLANOX_SINAI		0x6274

#define PCI_DEVFN(slot, func)   ((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)         (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)         ((devfn) & 0x07)

#define PCI_VDEVICE(_vendor, _device)					\
	    .vendor = PCI_VENDOR_ID_##_vendor, .device = (_device),	\
	    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define	PCI_DEVICE(_vendor, _device)					\
	    .vendor = (_vendor), .device = (_device),			\
	    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define	to_pci_dev(n)	container_of(n, struct pci_dev, dev)

#define	PCI_VENDOR_ID		PCIR_DEVVENDOR
#define	PCI_COMMAND		PCIR_COMMAND
#define	PCI_EXP_DEVCTL		PCIER_DEVICE_CTL		/* Device Control */
#define	PCI_EXP_LNKCTL		PCIER_LINK_CTL			/* Link Control */
#define	PCI_EXP_FLAGS_TYPE	PCIEM_FLAGS_TYPE		/* Device/Port type */
#define	PCI_EXP_DEVCAP		PCIER_DEVICE_CAP		/* Device capabilities */
#define	PCI_EXP_DEVSTA		PCIER_DEVICE_STA		/* Device Status */
#define	PCI_EXP_LNKCAP		PCIER_LINK_CAP			/* Link Capabilities */
#define	PCI_EXP_LNKSTA		PCIER_LINK_STA			/* Link Status */
#define	PCI_EXP_SLTCAP		PCIER_SLOT_CAP			/* Slot Capabilities */
#define	PCI_EXP_SLTCTL		PCIER_SLOT_CTL			/* Slot Control */
#define	PCI_EXP_SLTSTA		PCIER_SLOT_STA			/* Slot Status */
#define	PCI_EXP_RTCTL		PCIER_ROOT_CTL			/* Root Control */
#define	PCI_EXP_RTCAP		PCIER_ROOT_CAP			/* Root Capabilities */
#define	PCI_EXP_RTSTA		PCIER_ROOT_STA			/* Root Status */
#define	PCI_EXP_DEVCAP2		PCIER_DEVICE_CAP2		/* Device Capabilities 2 */
#define	PCI_EXP_DEVCTL2		PCIER_DEVICE_CTL2		/* Device Control 2 */
#define	PCI_EXP_LNKCAP2		PCIER_LINK_CAP2			/* Link Capabilities 2 */
#define	PCI_EXP_LNKCTL2		PCIER_LINK_CTL2			/* Link Control 2 */
#define	PCI_EXP_LNKSTA2		PCIER_LINK_STA2			/* Link Status 2 */
#define	PCI_EXP_FLAGS		PCIER_FLAGS			/* Capabilities register */
#define	PCI_EXP_FLAGS_VERS	PCIEM_FLAGS_VERSION		/* Capability version */
#define	PCI_EXP_TYPE_ROOT_PORT	PCIEM_TYPE_ROOT_PORT		/* Root Port */
#define	PCI_EXP_TYPE_ENDPOINT	PCIEM_TYPE_ENDPOINT		/* Express Endpoint */
#define	PCI_EXP_TYPE_LEG_END	PCIEM_TYPE_LEGACY_ENDPOINT	/* Legacy Endpoint */
#define	PCI_EXP_TYPE_DOWNSTREAM PCIEM_TYPE_DOWNSTREAM_PORT	/* Downstream Port */
#define	PCI_EXP_FLAGS_SLOT	PCIEM_FLAGS_SLOT		/* Slot implemented */
#define	PCI_EXP_TYPE_RC_EC	PCIEM_TYPE_ROOT_EC		/* Root Complex Event Collector */


#define	IORESOURCE_MEM	(1 << SYS_RES_MEMORY)
#define	IORESOURCE_IO	(1 << SYS_RES_IOPORT)
#define	IORESOURCE_IRQ	(1 << SYS_RES_IRQ)

struct pci_dev;


struct pci_driver {
	struct list_head		links;
	char				*name;
	const struct pci_device_id		*id_table;
	int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove)(struct pci_dev *dev);
        int  (*suspend) (struct pci_dev *dev, pm_message_t state);      /* Device suspended */
        int  (*resume) (struct pci_dev *dev);                   /* Device woken up */
	driver_t			driver;
	devclass_t			bsdclass;
        const struct pci_error_handlers       *err_handler;
};

extern struct list_head pci_drivers;
extern struct list_head pci_devices;
extern spinlock_t pci_lock;

#define	__devexit_p(x)	x

struct pci_dev {
	struct device		dev;
	struct list_head	links;
	struct pci_driver	*pdrv;
	uint64_t		dma_mask;
	uint16_t		device;
	uint16_t		vendor;
	unsigned int		irq;
	unsigned int		devfn;
	u8			revision;
};

static inline struct resource_list_entry *
_pci_get_rle(struct pci_dev *pdev, int type, int rid)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;

	dinfo = device_get_ivars(pdev->dev.bsddev);
	rl = &dinfo->resources;
	return resource_list_find(rl, type, rid);
}

static inline struct resource_list_entry *
_pci_get_bar(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;

	bar = PCIR_BAR(bar);
	if ((rle = _pci_get_rle(pdev, SYS_RES_MEMORY, bar)) == NULL)
		rle = _pci_get_rle(pdev, SYS_RES_IOPORT, bar);
	return (rle);
}

static inline struct device *
_pci_find_irq_dev(unsigned int irq)
{
	struct pci_dev *pdev;

	spin_lock(&pci_lock);
	list_for_each_entry(pdev, &pci_devices, links) {
		if (irq == pdev->dev.irq)
			break;
		if (irq >= pdev->dev.msix && irq < pdev->dev.msix_max)
			break;
	}
	spin_unlock(&pci_lock);
	if (pdev)
		return &pdev->dev;
	return (NULL);
}

static inline unsigned long
pci_resource_start(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;

	if ((rle = _pci_get_bar(pdev, bar)) == NULL)
		return (0);
	return rle->start;
}

static inline unsigned long
pci_resource_len(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;

	if ((rle = _pci_get_bar(pdev, bar)) == NULL)
		return (0);
	return rle->count;
}

static inline int
pci_resource_type(struct pci_dev *pdev, int bar)
{
	struct pci_map *pm;

	pm = pci_find_bar(pdev->dev.bsddev, PCIR_BAR(bar));
	if (!pm)
		return (-1);

	if (PCI_BAR_IO(pm->pm_value))
		return (SYS_RES_IOPORT);
	else
		return (SYS_RES_MEMORY);
}

/*
 * All drivers just seem to want to inspect the type not flags.
 */
static inline int
pci_resource_flags(struct pci_dev *pdev, int bar)
{
	int type;

	type = pci_resource_type(pdev, bar);
	if (type < 0)
		return (0);
	return (1 << type);
}

static inline const char *
pci_name(struct pci_dev *d)
{

	return device_get_desc(d->dev.bsddev);
}

static inline void *
pci_get_drvdata(struct pci_dev *pdev)
{

	return dev_get_drvdata(&pdev->dev);
}

static inline void
pci_set_drvdata(struct pci_dev *pdev, void *data)
{

	dev_set_drvdata(&pdev->dev, data);
}

static inline int
pci_enable_device(struct pci_dev *pdev)
{

	pci_enable_io(pdev->dev.bsddev, SYS_RES_IOPORT);
	pci_enable_io(pdev->dev.bsddev, SYS_RES_MEMORY);
	return (0);
}

static inline void
pci_disable_device(struct pci_dev *pdev)
{
}

static inline int
pci_set_master(struct pci_dev *pdev)
{

	pci_enable_busmaster(pdev->dev.bsddev);
	return (0);
}

static inline int
pci_clear_master(struct pci_dev *pdev)
{

	pci_disable_busmaster(pdev->dev.bsddev);
	return (0);
}

static inline int
pci_request_region(struct pci_dev *pdev, int bar, const char *res_name)
{
	int rid;
	int type;

	type = pci_resource_type(pdev, bar);
	if (type < 0)
		return (-ENODEV);
	rid = PCIR_BAR(bar);
	if (bus_alloc_resource_any(pdev->dev.bsddev, type, &rid,
	    RF_ACTIVE) == NULL)
		return (-EINVAL);
	return (0);
}

static inline void
pci_release_region(struct pci_dev *pdev, int bar)
{
	struct resource_list_entry *rle;

	if ((rle = _pci_get_bar(pdev, bar)) == NULL)
		return;
	bus_release_resource(pdev->dev.bsddev, rle->type, rle->rid, rle->res);
}

static inline void
pci_release_regions(struct pci_dev *pdev)
{
	int i;

	for (i = 0; i <= PCIR_MAX_BAR_0; i++)
		pci_release_region(pdev, i);
}

static inline int
pci_request_regions(struct pci_dev *pdev, const char *res_name)
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

static inline void
pci_disable_msix(struct pci_dev *pdev)
{

	pci_release_msi(pdev->dev.bsddev);
}

#define	PCI_CAP_ID_EXP	PCIY_EXPRESS
#define	PCI_CAP_ID_PCIX	PCIY_PCIX


static inline int
pci_find_capability(struct pci_dev *pdev, int capid)
{
	int reg;

	if (pci_find_cap(pdev->dev.bsddev, capid, &reg))
		return (0);
	return (reg);
}




/**
 * pci_pcie_cap - get the saved PCIe capability offset
 * @dev: PCI device
 *
 * PCIe capability offset is calculated at PCI device initialization
 * time and saved in the data structure. This function returns saved
 * PCIe capability offset. Using this instead of pci_find_capability()
 * reduces unnecessary search in the PCI configuration space. If you
 * need to calculate PCIe capability offset from raw device for some
 * reasons, please use pci_find_capability() instead.
 */
static inline int pci_pcie_cap(struct pci_dev *dev)
{
        return pci_find_capability(dev, PCI_CAP_ID_EXP);
}


static inline int
pci_read_config_byte(struct pci_dev *pdev, int where, u8 *val)
{

	*val = (u8)pci_read_config(pdev->dev.bsddev, where, 1);
	return (0);
}

static inline int
pci_read_config_word(struct pci_dev *pdev, int where, u16 *val)
{

	*val = (u16)pci_read_config(pdev->dev.bsddev, where, 2);
	return (0);
}

static inline int
pci_read_config_dword(struct pci_dev *pdev, int where, u32 *val)
{

	*val = (u32)pci_read_config(pdev->dev.bsddev, where, 4);
	return (0);
} 

static inline int
pci_write_config_byte(struct pci_dev *pdev, int where, u8 val)
{

	pci_write_config(pdev->dev.bsddev, where, val, 1);
	return (0);
}

static inline int
pci_write_config_word(struct pci_dev *pdev, int where, u16 val)
{

	pci_write_config(pdev->dev.bsddev, where, val, 2);
	return (0);
}

static inline int
pci_write_config_dword(struct pci_dev *pdev, int where, u32 val)
{ 

	pci_write_config(pdev->dev.bsddev, where, val, 4);
	return (0);
}

static struct pci_driver *
linux_pci_find(device_t dev, const struct pci_device_id **idp)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;
	uint16_t vendor;
	uint16_t device;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);

	spin_lock(&pci_lock);
	list_for_each_entry(pdrv, &pci_drivers, links) {
		for (id = pdrv->id_table; id->vendor != 0; id++) {
			if (vendor == id->vendor && device == id->device) {
				*idp = id;
				spin_unlock(&pci_lock);
				return (pdrv);
			}
		}
	}
	spin_unlock(&pci_lock);
	return (NULL);
}

static inline int
linux_pci_probe(device_t dev)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;

	if ((pdrv = linux_pci_find(dev, &id)) == NULL)
		return (ENXIO);
	if (device_get_driver(dev) != &pdrv->driver)
		return (ENXIO);
	device_set_desc(dev, pdrv->name);
	return (0);
}

static inline int
linux_pci_attach(device_t dev)
{
	struct resource_list_entry *rle;
	struct pci_dev *pdev;
	struct pci_driver *pdrv;
	const struct pci_device_id *id;
	int error;

	pdrv = linux_pci_find(dev, &id);
	pdev = device_get_softc(dev);
	pdev->dev.parent = &linux_rootdev;
	pdev->dev.bsddev = dev;
	INIT_LIST_HEAD(&pdev->dev.irqents);
	pdev->device = id->device;
	pdev->vendor = id->vendor;
	pdev->dev.dma_mask = &pdev->dma_mask;
	pdev->pdrv = pdrv;
	kobject_init(&pdev->dev.kobj, &dev_ktype);
	kobject_set_name(&pdev->dev.kobj, device_get_nameunit(dev));
	kobject_add(&pdev->dev.kobj, &linux_rootdev.kobj,
	    kobject_name(&pdev->dev.kobj));
	rle = _pci_get_rle(pdev, SYS_RES_IRQ, 0);
	if (rle)
		pdev->dev.irq = rle->start;
	else
		pdev->dev.irq = 0;
	pdev->irq = pdev->dev.irq;
	mtx_unlock(&Giant);
	spin_lock(&pci_lock);
	list_add(&pdev->links, &pci_devices);
	spin_unlock(&pci_lock);
	error = pdrv->probe(pdev, id);
	mtx_lock(&Giant);
	if (error) {
		spin_lock(&pci_lock);
		list_del(&pdev->links);
		spin_unlock(&pci_lock);
		put_device(&pdev->dev);
		return (-error);
	}
	return (0);
}

static inline int
linux_pci_detach(device_t dev)
{
	struct pci_dev *pdev;

	pdev = device_get_softc(dev);
	mtx_unlock(&Giant);
	pdev->pdrv->remove(pdev);
	mtx_lock(&Giant);
	spin_lock(&pci_lock);
	list_del(&pdev->links);
	spin_unlock(&pci_lock);
	put_device(&pdev->dev);

	return (0);
}

static device_method_t pci_methods[] = {
	DEVMETHOD(device_probe, linux_pci_probe),
	DEVMETHOD(device_attach, linux_pci_attach),
	DEVMETHOD(device_detach, linux_pci_detach),
	{0, 0}
};

static inline int
pci_register_driver(struct pci_driver *pdrv)
{
	devclass_t bus;
	int error;

	spin_lock(&pci_lock);
	list_add(&pdrv->links, &pci_drivers);
	spin_unlock(&pci_lock);
	bus = devclass_find("pci");
	pdrv->driver.name = pdrv->name;
	pdrv->driver.methods = pci_methods;
	pdrv->driver.size = sizeof(struct pci_dev);
	mtx_lock(&Giant);
	error = devclass_add_driver(bus, &pdrv->driver, BUS_PASS_DEFAULT,
	    &pdrv->bsdclass);
	mtx_unlock(&Giant);
	if (error)
		return (-error);
	return (0);
}

static inline void
pci_unregister_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	list_del(&pdrv->links);
	bus = devclass_find("pci");
	mtx_lock(&Giant);
	devclass_delete_driver(bus, &pdrv->driver);
	mtx_unlock(&Giant);
}

struct msix_entry {
	int entry;
	int vector;
};

/*
 * Enable msix, positive errors indicate actual number of available
 * vectors.  Negative errors are failures.
 *
 * NB: define added to prevent this definition of pci_enable_msix from
 * clashing with the native FreeBSD version.
 */
#define	pci_enable_msix		linux_pci_enable_msix
static inline int
pci_enable_msix(struct pci_dev *pdev, struct msix_entry *entries, int nreq)
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
	rle = _pci_get_rle(pdev, SYS_RES_IRQ, 1);
	pdev->dev.msix = rle->start;
	pdev->dev.msix_max = rle->start + avail;
	for (i = 0; i < nreq; i++)
		entries[i].vector = pdev->dev.msix + i;
	return (0);
}

#define	pci_enable_msix_range	linux_pci_enable_msix_range
static inline int
pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
    int minvec, int maxvec)
{
	int nvec = maxvec;
	int rc;

	if (maxvec < minvec)
		return (-ERANGE);

	do {
		rc = pci_enable_msix(dev, entries, nvec);
		if (rc < 0) {
			return (rc);
		} else if (rc > 0) {
			if (rc < minvec)
				return (-ENOSPC);
			nvec = rc;
		}
	} while (rc);
	return (nvec);
}

static inline int pci_channel_offline(struct pci_dev *pdev)
{
        return false;
}

static inline int pci_enable_sriov(struct pci_dev *dev, int nr_virtfn)
{
        return -ENODEV;
}
static inline void pci_disable_sriov(struct pci_dev *dev)
{
}

/**
 * DEFINE_PCI_DEVICE_TABLE - macro used to describe a pci device table
 * @_table: device table name
 *
 * This macro is used to create a struct pci_device_id array (a device table)
 * in a generic manner.
 */
#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[] __devinitdata


/* XXX This should not be necessary. */
#define	pcix_set_mmrbc(d, v)	0
#define	pcix_get_max_mmrbc(d)	0
#define	pcie_set_readrq(d, v)	0

#define	PCI_DMA_BIDIRECTIONAL	0
#define	PCI_DMA_TODEVICE	1
#define	PCI_DMA_FROMDEVICE	2
#define	PCI_DMA_NONE		3

#define	pci_pool		dma_pool
#define pci_pool_destroy	dma_pool_destroy
#define pci_pool_alloc		dma_pool_alloc
#define pci_pool_free		dma_pool_free
#define	pci_pool_create(_name, _pdev, _size, _align, _alloc)		\
	    dma_pool_create(_name, &(_pdev)->dev, _size, _align, _alloc)
#define	pci_free_consistent(_hwdev, _size, _vaddr, _dma_handle)		\
	    dma_free_coherent((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_size, _vaddr, _dma_handle)
#define	pci_map_sg(_hwdev, _sg, _nents, _dir)				\
	    dma_map_sg((_hwdev) == NULL ? NULL : &(_hwdev->dev),	\
		_sg, _nents, (enum dma_data_direction)_dir)
#define	pci_map_single(_hwdev, _ptr, _size, _dir)			\
	    dma_map_single((_hwdev) == NULL ? NULL : &(_hwdev->dev),	\
		(_ptr), (_size), (enum dma_data_direction)_dir)
#define	pci_unmap_single(_hwdev, _addr, _size, _dir)			\
	    dma_unmap_single((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_addr, _size, (enum dma_data_direction)_dir)
#define	pci_unmap_sg(_hwdev, _sg, _nents, _dir)				\
	    dma_unmap_sg((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_sg, _nents, (enum dma_data_direction)_dir)
#define	pci_map_page(_hwdev, _page, _offset, _size, _dir)		\
	    dma_map_page((_hwdev) == NULL ? NULL : &(_hwdev)->dev, _page,\
		_offset, _size, (enum dma_data_direction)_dir)
#define	pci_unmap_page(_hwdev, _dma_address, _size, _dir)		\
	    dma_unmap_page((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_dma_address, _size, (enum dma_data_direction)_dir)
#define	pci_set_dma_mask(_pdev, mask)	dma_set_mask(&(_pdev)->dev, (mask))
#define	pci_dma_mapping_error(_pdev, _dma_addr)				\
	    dma_mapping_error(&(_pdev)->dev, _dma_addr)
#define	pci_set_consistent_dma_mask(_pdev, _mask)			\
	    dma_set_coherent_mask(&(_pdev)->dev, (_mask))
#define	DECLARE_PCI_UNMAP_ADDR(x)	DEFINE_DMA_UNMAP_ADDR(x);
#define	DECLARE_PCI_UNMAP_LEN(x)	DEFINE_DMA_UNMAP_LEN(x);
#define	pci_unmap_addr		dma_unmap_addr
#define	pci_unmap_addr_set	dma_unmap_addr_set
#define	pci_unmap_len		dma_unmap_len
#define	pci_unmap_len_set	dma_unmap_len_set

typedef unsigned int __bitwise pci_channel_state_t;
typedef unsigned int __bitwise pci_ers_result_t;

enum pci_channel_state {
        /* I/O channel is in normal state */
        pci_channel_io_normal = (__force pci_channel_state_t) 1,

        /* I/O to channel is blocked */
        pci_channel_io_frozen = (__force pci_channel_state_t) 2,

        /* PCI card is dead */
        pci_channel_io_perm_failure = (__force pci_channel_state_t) 3,
};

enum pci_ers_result {
        /* no result/none/not supported in device driver */
        PCI_ERS_RESULT_NONE = (__force pci_ers_result_t) 1,

        /* Device driver can recover without slot reset */
        PCI_ERS_RESULT_CAN_RECOVER = (__force pci_ers_result_t) 2,

        /* Device driver wants slot to be reset. */
        PCI_ERS_RESULT_NEED_RESET = (__force pci_ers_result_t) 3,

        /* Device has completely failed, is unrecoverable */
        PCI_ERS_RESULT_DISCONNECT = (__force pci_ers_result_t) 4,

        /* Device driver is fully recovered and operational */
        PCI_ERS_RESULT_RECOVERED = (__force pci_ers_result_t) 5,
};


/* PCI bus error event callbacks */
struct pci_error_handlers {
        /* PCI bus error detected on this device */
        pci_ers_result_t (*error_detected)(struct pci_dev *dev,
                        enum pci_channel_state error);

        /* MMIO has been re-enabled, but not DMA */
        pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);

        /* PCI Express link has been reset */
        pci_ers_result_t (*link_reset)(struct pci_dev *dev);

        /* PCI slot has been reset */
        pci_ers_result_t (*slot_reset)(struct pci_dev *dev);

        /* Device driver may resume normal operations */
        void (*resume)(struct pci_dev *dev);
};

/* freeBSD does not support SRIOV - yet */
static inline struct pci_dev *pci_physfn(struct pci_dev *dev)
{
        return dev;
}

static inline bool pci_is_pcie(struct pci_dev *dev)
{
        return !!pci_pcie_cap(dev);
}

static inline u16 pcie_flags_reg(struct pci_dev *dev)
{
        int pos;
        u16 reg16;

        pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
        if (!pos)
                return 0;

        pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &reg16);

        return reg16;
}


static inline int pci_pcie_type(struct pci_dev *dev)
{
        return (pcie_flags_reg(dev) & PCI_EXP_FLAGS_TYPE) >> 4;
}

static inline int pcie_cap_version(struct pci_dev *dev)
{
        return pcie_flags_reg(dev) & PCI_EXP_FLAGS_VERS;
}

static inline bool pcie_cap_has_lnkctl(struct pci_dev *dev)
{
        int type = pci_pcie_type(dev);

        return pcie_cap_version(dev) > 1 ||
               type == PCI_EXP_TYPE_ROOT_PORT ||
               type == PCI_EXP_TYPE_ENDPOINT ||
               type == PCI_EXP_TYPE_LEG_END;
}

static inline bool pcie_cap_has_devctl(const struct pci_dev *dev)
{
                return true;
}

static inline bool pcie_cap_has_sltctl(struct pci_dev *dev)
{
        int type = pci_pcie_type(dev);

        return pcie_cap_version(dev) > 1 ||
               type == PCI_EXP_TYPE_ROOT_PORT ||
               (type == PCI_EXP_TYPE_DOWNSTREAM &&
                pcie_flags_reg(dev) & PCI_EXP_FLAGS_SLOT);
}

static inline bool pcie_cap_has_rtctl(struct pci_dev *dev)
{
        int type = pci_pcie_type(dev);

        return pcie_cap_version(dev) > 1 ||
               type == PCI_EXP_TYPE_ROOT_PORT ||
               type == PCI_EXP_TYPE_RC_EC;
}

static bool pcie_capability_reg_implemented(struct pci_dev *dev, int pos)
{
        if (!pci_is_pcie(dev))
                return false;

        switch (pos) {
        case PCI_EXP_FLAGS_TYPE:
                return true;
        case PCI_EXP_DEVCAP:
        case PCI_EXP_DEVCTL:
        case PCI_EXP_DEVSTA:
                return pcie_cap_has_devctl(dev);
        case PCI_EXP_LNKCAP:
        case PCI_EXP_LNKCTL:
        case PCI_EXP_LNKSTA:
                return pcie_cap_has_lnkctl(dev);
        case PCI_EXP_SLTCAP:
        case PCI_EXP_SLTCTL:
        case PCI_EXP_SLTSTA:
                return pcie_cap_has_sltctl(dev);
        case PCI_EXP_RTCTL:
        case PCI_EXP_RTCAP:
        case PCI_EXP_RTSTA:
                return pcie_cap_has_rtctl(dev);
        case PCI_EXP_DEVCAP2:
        case PCI_EXP_DEVCTL2:
        case PCI_EXP_LNKCAP2:
        case PCI_EXP_LNKCTL2:
        case PCI_EXP_LNKSTA2:
                return pcie_cap_version(dev) > 1;
        default:
                return false;
        }
}

 
static inline int pcie_capability_write_word(struct pci_dev *dev, int pos, u16 val)
{
        if (pos & 1)
                return -EINVAL;

        if (!pcie_capability_reg_implemented(dev, pos))
                return 0;

        return pci_write_config_word(dev, pci_pcie_cap(dev) + pos, val);
}


#endif	/* _LINUX_PCI_H_ */
