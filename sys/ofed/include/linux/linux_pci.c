/*-
 * Copyright (c) 2015 Mellanox Technologies, Ltd.
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
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>
#include <machine/pmap.h>

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

static device_probe_t linux_pci_probe;
static device_attach_t linux_pci_attach;
static device_detach_t linux_pci_detach;

static device_method_t pci_methods[] = {
	DEVMETHOD(device_probe, linux_pci_probe),
	DEVMETHOD(device_attach, linux_pci_attach),
	DEVMETHOD(device_detach, linux_pci_detach),
	DEVMETHOD_END
};

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

static int
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

static int
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

static int
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

int
pci_register_driver(struct pci_driver *pdrv)
{
	devclass_t bus;
	int error = 0;

	bus = devclass_find("pci");

	spin_lock(&pci_lock);
	list_add(&pdrv->links, &pci_drivers);
	spin_unlock(&pci_lock);
	pdrv->driver.name = pdrv->name;
	pdrv->driver.methods = pci_methods;
	pdrv->driver.size = sizeof(struct pci_dev);
	mtx_lock(&Giant);
	if (bus != NULL) {
		error = devclass_add_driver(bus, &pdrv->driver, BUS_PASS_DEFAULT,
		    &pdrv->bsdclass);
	}
	mtx_unlock(&Giant);
	return (-error);
}

void
pci_unregister_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	bus = devclass_find("pci");

	list_del(&pdrv->links);
	mtx_lock(&Giant);
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->driver);
	mtx_unlock(&Giant);
}

