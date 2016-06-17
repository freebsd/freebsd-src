/*
 * IBM PowerPC Virtual I/O Infrastructure Support.
 *
 * Dave Engebretsen engebret@us.ibm.com
 *    Copyright (c) 2003 Dave Engebretsen
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/module.h>
#include <asm/rtas.h>
#include <asm/pci_dma.h>
#include <asm/dma.h>
#include <asm/ppcdebug.h>
#include <asm/vio.h>
#include <asm/hvcall.h>

extern struct TceTable *build_tce_table(struct TceTable *tbl);

extern dma_addr_t get_tces(struct TceTable *, unsigned order,
			   void *page, unsigned numPages, int direction);
extern void tce_free(struct TceTable *tbl, dma_addr_t dma_addr,
		     unsigned order, unsigned num_pages);


static struct vio_bus vio_bus;
static LIST_HEAD(registered_vio_drivers);
int vio_num_address_cells;
EXPORT_SYMBOL(vio_num_address_cells);

/**
 * vio_register_driver: - Register a new vio driver
 * @drv:	The vio_driver structure to be registered.
 *
 * Adds the driver structure to the list of registered drivers
 * Returns the number of vio devices which were claimed by the driver
 * during registration.  The driver remains registered even if the
 * return value is zero.
 */
int vio_register_driver(struct vio_driver *drv)
{
	int count = 0;
	struct vio_dev *dev;
	const struct vio_device_id* id;
	/*
	 * Walk the vio_bus, find devices for this driver, and
	 * call back into the driver probe interface.
	 */

	list_for_each_entry(dev, &vio_bus.devices, devices_list) {
		id = vio_match_device(drv->id_table, dev);
		if(drv && id) {
			if (0 == drv->probe(dev, id)) {
				dev->driver = drv;
				count++;
			}
		}
	}

	list_add_tail(&drv->node, &registered_vio_drivers);

	return count;
}
EXPORT_SYMBOL(vio_register_driver);

/**
 * vio_unregister_driver - Remove registration of vio driver.
 * @driver:	The vio_driver struct to be removed form registration
 *
 * Searches for devices that are assigned to the driver and calls
 * driver->remove() for each one.  Removes the driver from the list
 * of registered drivers.  Returns the number of devices that were
 * assigned to that driver.
 */
int vio_unregister_driver(struct vio_driver *driver)
{
	struct vio_dev *dev;
	int devices_found = 0;

	list_for_each_entry(dev, &vio_bus.devices, devices_list) {
		if (dev->driver == driver) {
			driver->remove(dev);
			dev->driver = NULL;
			devices_found++;
		}
	}

	list_del(&driver->node);

	return devices_found;
}
EXPORT_SYMBOL(vio_unregister_driver);

/**
 * vio_match_device: - Tell if a VIO device has a matching VIO device id structure.
 * @ids: 	array of VIO device id structures to search in
 * @dev: 	the VIO device structure to match against
 *
 * Used by a driver to check whether a VIO device present in the
 * system is in its list of supported devices. Returns the matching
 * vio_device_id structure or NULL if there is no match.
 */
const struct vio_device_id *
vio_match_device(const struct vio_device_id *ids, const struct vio_dev *dev)
{
	while (ids->type) {
		if ((strncmp(dev->archdata->type, ids->type, strlen(ids->type)) == 0) &&
			device_is_compatible((struct device_node*)dev->archdata, ids->compat))
			return ids;
		ids++;
	}
	return NULL;
}

/**
 * vio_bus_init: - Initialize the virtual IO bus
 */
int __init
vio_bus_init(void)
{
	struct device_node *node_vroot, *node_vdev;

	printk("vio_bus_init: start\n");

	INIT_LIST_HEAD(&vio_bus.devices);

	/*
	 * Create device node entries for each virtual device
	 * identified in the device tree.
	 * Functionally takes the place of pci_scan_bus
	 */
	node_vroot = find_devices("vdevice");
	if (!node_vroot) {
		printk(KERN_WARNING "vio_bus_init: no /vdevice node\n");
		return 0;
	}

	vio_num_address_cells = prom_n_addr_cells(node_vroot->child);

	for (node_vdev = node_vroot->child;
			node_vdev != NULL;
			node_vdev = node_vdev->sibling) {
		printk(KERN_INFO "vio_bus_init: processing %p\n", node_vdev);

		vio_register_device(node_vdev);
	}

	printk(KERN_INFO "vio_bus_init: done\n");

	return 0;
}

__initcall(vio_bus_init);


/**
 * vio_register_device: - Register a new vio device.
 * @archdata:	The OF node for this device.
 *
 * Creates and initializes a vio_dev structure from the data in
 * node_vdev (archdata) and adds it to the list of virtual devices.
 * Returns a pointer to the created vio_dev or NULL if node has
 * NULL device_type or compatible fields.
*/
struct vio_dev * __devinit vio_register_device(struct device_node *node_vdev)
{
	struct vio_dev *dev;
	unsigned int *unit_address;
	unsigned int *irq_p;

	/* guarantee all vio_devs have 'device_type' field*/
	if ((NULL == node_vdev->type)) {
		printk(KERN_WARNING "vio_register_device: node %s missing 'device_type' "
			, node_vdev->name?node_vdev->name:"UNKNOWN");
		return NULL;
	}

	unit_address = (unsigned int *)get_property(node_vdev, "reg", NULL);
	if(!unit_address) {
		printk(KERN_WARNING "Can't find %s reg property\n", node_vdev->name?node_vdev->name:"UNKNOWN_DEVICE");
		return NULL;
	}

	/* allocate a vio_dev for this node */
	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	memset(dev, 0, sizeof(*dev));

	dev->archdata = (void*)node_vdev; /* to become of_get_node(node_vdev); */
	dev->bus = &vio_bus;
	dev->unit_address = *unit_address;
	dev->tce_table = vio_build_tce_table(dev);

	irq_p = (unsigned int *) get_property(node_vdev, "interrupts", 0);
	if(irq_p) {
		dev->irq = irq_offset_up(*irq_p);
	} else {
		dev->irq = (unsigned int) -1;
	}

	list_add_tail(&dev->devices_list, &vio_bus.devices);

	return dev;
}

/**
 * vio_find_driver: - Find driver for vio_dev.
 * @dev:	Device to search for a driver.
 *
 * Walks the registered_vio_drivers list calling vio_match_device()
 * for every driver in the list. If there is a match, calls the
 * driver's probe().
 * Returns a pointer to the matched driver or NULL if driver is not
 * found.
*/
struct vio_driver * vio_find_driver(struct vio_dev* dev)
{
	struct vio_driver *driver;
	list_for_each_entry(driver, &registered_vio_drivers, node) {
		if(driver && vio_match_device(driver->id_table, dev)) {
			if (0 < driver->probe(dev, NULL)) {
				dev->driver = driver;
				return driver;
			}
		}
	}

	return NULL;
}

/**
 * vio_get_attribute: - get attribute for virtual device
 * @vdev:	The vio device to get property.
 * @which:	The property/attribute to be extracted.
 * @length:	Pointer to length of returned data size (unused if NULL).
 *
 * Calls prom.c's get_property() to return the value of the
 * attribute specified by the preprocessor constant @which
*/
const void * vio_get_attribute(struct vio_dev *vdev, void* which, int* length)
{
	return get_property((struct device_node *)vdev->archdata, (char*)which, length);
}
EXPORT_SYMBOL(vio_get_attribute);

/**
 * vio_build_tce_table: - gets the dma information from OF and builds the TCE tree.
 * @dev: the virtual device.
 *
 * Returns a pointer to the built tce tree, or NULL if it can't
 * find property.
*/
struct TceTable * vio_build_tce_table(struct vio_dev *dev)
{
	unsigned int *dma_window;
	struct TceTable *newTceTable;
	unsigned long offset;
	unsigned long size;
	int dma_window_property_size;

	dma_window = (unsigned int *) get_property((struct device_node *)dev->archdata, "ibm,my-dma-window", &dma_window_property_size);
	if(!dma_window) {
		return NULL;
	}

	newTceTable = (struct TceTable *) kmalloc(sizeof(struct TceTable), GFP_KERNEL);

	/* RPA docs say that #address-cells is always 1 for virtual
		devices, but some older boxes' OF returns 2.  This should
		be removed by GA, unless there is legacy OFs that still
		have 2 for #address-cells */
	size = ((dma_window[1+vio_num_address_cells]
		>> PAGE_SHIFT) << 3) >> PAGE_SHIFT;

	/* This is just an ugly kludge. Remove as soon as the OF for all
	machines actually follow the spec and encodes the offset field
	as phys-encode (that is, #address-cells wide)*/
	if (dma_window_property_size == 12) {
		size = ((dma_window[1] >> PAGE_SHIFT) << 3) >> PAGE_SHIFT;
	} else if (dma_window_property_size == 20) {
		size = ((dma_window[4] >> PAGE_SHIFT) << 3) >> PAGE_SHIFT;
	} else {
		printk(KERN_WARNING "vio_build_tce_table: Invalid size of ibm,my-dma-window=%i, using 0x80 for size\n", dma_window_property_size);
		size = 0x80;
	}

	/*  There should be some code to extract the phys-encoded offset
		using prom_n_addr_cells(). However, according to a comment
		on earlier versions, it's always zero, so we don't bother */
	offset = dma_window[1] >>  PAGE_SHIFT;

	/* TCE table size - measured in units of pages of tce table */
	newTceTable->size = size;
	/* offset for VIO should always be 0 */
	newTceTable->startOffset = offset;
	newTceTable->busNumber   = 0;
	newTceTable->index       = (unsigned long)dma_window[0];
	newTceTable->tceType     = TCE_VB;

	return build_tce_table(newTceTable);
}

int vio_enable_interrupts(struct vio_dev *dev)
{
	int rc = h_vio_signal(dev->unit_address, VIO_IRQ_ENABLE);
	if (rc != H_Success) {
		printk(KERN_ERR "vio: Error 0x%x enabling interrupts\n", rc);
	}
	return rc;
}
EXPORT_SYMBOL(vio_enable_interrupts);

int vio_disable_interrupts(struct vio_dev *dev)
{
	int rc = h_vio_signal(dev->unit_address, VIO_IRQ_DISABLE);
	if (rc != H_Success) {
	printk(KERN_ERR "vio: Error 0x%x disabling interrupts\n", rc);
	}
	return rc;
}
EXPORT_SYMBOL(vio_disable_interrupts);

dma_addr_t vio_map_single(struct vio_dev *dev, void *vaddr,
			  size_t size, int direction )
{
	struct TceTable * tbl;
	dma_addr_t dma_handle = NO_TCE;
	unsigned long uaddr;
	unsigned order, nPages;

	if(direction == PCI_DMA_NONE) BUG();

	uaddr = (unsigned long)vaddr;
	nPages = PAGE_ALIGN( uaddr + size ) - ( uaddr & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;

	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
	if(order >= NUM_TCE_LEVELS) {
		printk("VIO_DMA: vio_map_single size to large: 0x%lx \n",size);
		return NO_TCE;
	}

	tbl = dev->tce_table;

	if(tbl) {
		dma_handle = get_tces(tbl, order, vaddr, nPages, direction);
		dma_handle |= (uaddr & ~PAGE_MASK);
	}

	return dma_handle;
}
EXPORT_SYMBOL(vio_map_single);

void vio_unmap_single(struct vio_dev *dev, dma_addr_t dma_handle,
		      size_t size, int direction)
{
	struct TceTable * tbl;
	unsigned order, nPages;

	if (direction == PCI_DMA_NONE) BUG();

	nPages = PAGE_ALIGN( dma_handle + size ) - ( dma_handle & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;

	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
	if(order >= NUM_TCE_LEVELS) {
		printk("VIO_DMA: vio_unmap_single 0x%lx size to large: 0x%lx \n",(unsigned long)dma_handle,(unsigned long)size);
		return;
	}

	tbl = dev->tce_table;
	if(tbl) tce_free(tbl, dma_handle, order, nPages);
}
EXPORT_SYMBOL(vio_unmap_single);

int vio_map_sg(struct vio_dev *vdev, struct scatterlist *sglist, int nelems,
	       int direction)
{
	int i;

	for (i = 0; i < nelems; i++) {

		/* 2.4 scsi scatterlists use address field.
		   Not sure about other subsystems. */
		void *vaddr;
		if (sglist->address)
			vaddr = sglist->address;
		else
			vaddr = page_address(sglist->page) + sglist->offset;

		sglist->dma_address = vio_map_single(vdev, vaddr,
						     sglist->length,
						     direction);
		sglist->dma_length = sglist->length;
		sglist++;
	}

	return nelems;
}
EXPORT_SYMBOL(vio_map_sg);

void vio_unmap_sg(struct vio_dev *vdev, struct scatterlist *sglist, int nelems,
		  int direction)
{
	while (nelems--) {
		vio_unmap_single(vdev, sglist->dma_address,
				 sglist->dma_length, direction);
		sglist++;
	}
}

void *vio_alloc_consistent(struct vio_dev *dev, size_t size,
			   dma_addr_t *dma_handle)
{
	struct TceTable * tbl;
	void *ret = NULL;
	unsigned order, nPages;
	dma_addr_t tce;

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
	if(order >= NUM_TCE_LEVELS) {
		printk("VIO_DMA: vio_alloc_consistent size to large: 0x%lx \n",size);
		return (void *)NO_TCE;
	}

	tbl = dev->tce_table;

	if ( tbl ) {
		/* Alloc enough pages (and possibly more) */
		ret = (void *)__get_free_pages( GFP_ATOMIC, order );
		if ( ret ) {
			/* Page allocation succeeded */
			memset(ret, 0, nPages << PAGE_SHIFT);
			/* Set up tces to cover the allocated range */
			tce = get_tces( tbl, order, ret, nPages, PCI_DMA_BIDIRECTIONAL );
			if ( tce == NO_TCE ) {
				PPCDBG(PPCDBG_TCE, "vio_alloc_consistent: get_tces failed\n" );
				free_pages( (unsigned long)ret, order );
				ret = NULL;
			}
			else
				{
					*dma_handle = tce;
				}
		}
		else PPCDBG(PPCDBG_TCE, "vio_alloc_consistent: __get_free_pages failed for order = %d\n", order);
	}
	else PPCDBG(PPCDBG_TCE, "vio_alloc_consistent: get_tce_table failed for 0x%016lx\n", dev);

	PPCDBG(PPCDBG_TCE, "\tvio_alloc_consistent: dma_handle = 0x%16.16lx\n", *dma_handle);
	PPCDBG(PPCDBG_TCE, "\tvio_alloc_consistent: return     = 0x%16.16lx\n", ret);
	return ret;
}
EXPORT_SYMBOL(vio_alloc_consistent);

void vio_free_consistent(struct vio_dev *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	struct TceTable * tbl;
	unsigned order, nPages;

	PPCDBG(PPCDBG_TCE, "vio_free_consistent:\n");
	PPCDBG(PPCDBG_TCE, "\tdev = 0x%16.16lx, size = 0x%16.16lx, dma_handle = 0x%16.16lx, vaddr = 0x%16.16lx\n", dev, size, dma_handle, vaddr);

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

	/* Client asked for way to much space.  This is checked later anyway */
	/* It is easier to debug here for the drivers than in the tce tables.*/
	if(order >= NUM_TCE_LEVELS) {
		printk("PCI_DMA: pci_free_consistent size to large: 0x%lx \n",size);
		return;
	}

	tbl = dev->tce_table;

	if ( tbl ) {
		tce_free(tbl, dma_handle, order, nPages);
		free_pages( (unsigned long)vaddr, order );
	}
}
EXPORT_SYMBOL(vio_free_consistent);

EXPORT_SYMBOL(plpar_hcall_norets);
EXPORT_SYMBOL(plpar_hcall_8arg_2ret);


