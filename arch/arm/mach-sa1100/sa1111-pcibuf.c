/*
 *  linux/arch/arm/mach-sa1100/pci-sa1111.c
 *
 *  Special pci_{map/unmap/dma_sync}_* routines for SA-1111.
 *
 *  These functions utilize bouncer buffers to compensate for a bug in
 *  the SA-1111 hardware which don't allow DMA to/from addresses
 *  certain addresses above 1MB.
 *
 *  Re-written by Christopher Hoover <ch@murgatroid.com>
 *  Original version by Brad Parker (brad@heeltoe.com)
 *
 *  Copyright (C) 2002 Hewlett Packard Company.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 * */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <asm/hardware/sa1111.h>
//#define DEBUG
#ifdef DEBUG
#define DPRINTK(...) do { printk(KERN_DEBUG __VA_ARGS__); } while (0)
#else
#define DPRINTK(...) do { } while (0)
#endif

struct safe_buffer {
	struct list_head node;

	/* original request */
	void		*ptr;
	size_t		size;
	int		direction;

	/* safe buffer info */
	struct pci_pool *pool;
	void		*safe;
	dma_addr_t	safe_dma_addr;
};

LIST_HEAD(safe_buffers);

#define SIZE_SMALL	1024
#define SIZE_LARGE	(16*1024)

static struct pci_pool *small_buffer_pool, *large_buffer_pool;

static int __init
create_safe_buffer_pools(void)
{
	small_buffer_pool = pci_pool_create("sa1111_small_dma_buffer",
					    SA1111_FAKE_PCIDEV,
					    SIZE_SMALL,
					    0 /* byte alignment */,
					    0 /* no page-crossing issues */,
					    SLAB_KERNEL);
	if (0 == small_buffer_pool) {
		printk(KERN_ERR
		       "sa1111_pcibuf: could not allocate small pci pool\n");
		return -1;
	}

	large_buffer_pool = pci_pool_create("sa1111_large_dma_buffer",
					    SA1111_FAKE_PCIDEV,
					    SIZE_LARGE,
					    0 /* byte alignment */,
					    0 /* no page-crossing issues */,
					    SLAB_KERNEL);
	if (0 == large_buffer_pool) {
		printk(KERN_ERR
		       "sa1111_pcibuf: could not allocate large pci pool\n");
		pci_pool_destroy(small_buffer_pool);
		small_buffer_pool = 0;
		return -1;
	}

	return 0;
}

static void __exit
destroy_safe_buffer_pools(void)
{
	if (small_buffer_pool)
		pci_pool_destroy(small_buffer_pool);
	if (large_buffer_pool)
		pci_pool_destroy(large_buffer_pool);

	small_buffer_pool = large_buffer_pool = 0;
}


/* allocate a 'safe' buffer and keep track of it */
static struct safe_buffer *
alloc_safe_buffer(void *ptr, size_t size, int direction)
{
	struct safe_buffer *buf;
	struct pci_pool *pool;
	void *safe;
	dma_addr_t safe_dma_addr;

	DPRINTK("%s(ptr=%p, size=%d, direction=%d)\n",
		__func__, ptr, size, direction);

	buf = kmalloc(sizeof(struct safe_buffer), GFP_ATOMIC);
	if (buf == 0) {
		printk(KERN_WARNING "%s: kmalloc failed\n", __func__);
		return 0;
	}

	if (size <= SIZE_SMALL) {
		pool = small_buffer_pool;
		safe = pci_pool_alloc(pool, GFP_ATOMIC, &safe_dma_addr);
	} else if (size <= SIZE_LARGE) {
		pool = large_buffer_pool;
		safe = pci_pool_alloc(pool, GFP_ATOMIC, &safe_dma_addr);
	} else {
		printk(KERN_DEBUG
		       "sa111_pcibuf: resorting to pci_alloc_consistent\n");
		pool = 0;
		safe = pci_alloc_consistent(SA1111_FAKE_PCIDEV, size,
					    &safe_dma_addr);
	}

	if (safe == 0) {
		printk(KERN_WARNING
		       "%s: could not alloc dma memory (size=%d)\n",
		       __func__, size);
		kfree(buf);
		return 0;
	}

	BUG_ON(sa1111_check_dma_bug(safe_dma_addr));	// paranoia

	buf->ptr = ptr;
	buf->size = size;
	buf->direction = direction;
	buf->pool = pool;
	buf->safe = safe;
	buf->safe_dma_addr = safe_dma_addr;

	MOD_INC_USE_COUNT;
	list_add(&buf->node, &safe_buffers);

	return buf;
}

/* determine if a buffer is from our "safe" pool */
static struct safe_buffer *
find_safe_buffer(dma_addr_t safe_dma_addr)
{
	struct list_head *entry;

	list_for_each(entry, &safe_buffers) {
		struct safe_buffer *b =
			list_entry(entry, struct safe_buffer, node);

		if (b->safe_dma_addr == safe_dma_addr) {
			return b;
		}
	}

	return 0;
}

static void
free_safe_buffer(struct safe_buffer *buf)
{
	DPRINTK("%s(buf=%p)\n", __func__, buf);

	list_del(&buf->node);

	if (buf->pool)
		pci_pool_free(buf->pool, buf->safe, buf->safe_dma_addr);
	else
		pci_free_consistent(SA1111_FAKE_PCIDEV, buf->size, buf->safe,
				    buf->safe_dma_addr);
	kfree(buf);

	MOD_DEC_USE_COUNT;
}

static inline int
dma_range_is_safe(dma_addr_t addr, size_t size)
{
	unsigned int physaddr = SA1111_DMA_ADDR((unsigned int) addr);

	/* Any address within one megabyte of the start of the target
         * bank will be OK.  This is an overly conservative test:
         * other addresses can be OK depending on the dram
         * configuration.  (See sa1111.c:sa1111_check_dma_bug() * for
         * details.)
	 *
	 * We take care to ensure the entire dma region is within
	 * the safe range.
	 */

	return ((physaddr + size - 1) < (1<<20));
}

/*
 * see if a buffer address is in an 'unsafe' range.  if it is
 * allocate a 'safe' buffer and copy the unsafe buffer into it.
 * substitute the safe buffer for the unsafe one.
 * (basically move the buffer from an unsafe area to a safe one)
 */
dma_addr_t
sa1111_map_single(void *ptr, size_t size, int direction)
{
	unsigned long flags;
	dma_addr_t dma_addr;

	DPRINTK("%s(ptr=%p,size=%d,dir=%x)\n",
	       __func__, ptr, size, direction);

	BUG_ON(direction == PCI_DMA_NONE);

	local_irq_save(flags);

	dma_addr = virt_to_bus(ptr);

	if (!dma_range_is_safe(dma_addr, size)) {
		struct safe_buffer *buf;

		buf = alloc_safe_buffer(ptr, size, direction);
		if (buf == 0) {
			printk(KERN_ERR
			       "%s: unable to map unsafe buffer %p!\n",
			       __func__, ptr);
			local_irq_restore(flags);
			return 0;
		}

		DPRINTK("%s: unsafe buffer %p (phy=%p) mapped to %p (phy=%p)\n",
			__func__,
			buf->ptr, (void *) virt_to_bus(buf->ptr),
			buf->safe, (void *) buf->safe_dma_addr);

		if ((direction == PCI_DMA_TODEVICE) ||
		    (direction == PCI_DMA_BIDIRECTIONAL)) {
			DPRINTK("%s: copy out from unsafe %p, to safe %p, size %d\n",
				__func__, ptr, buf->safe, size);
			memcpy(buf->safe, ptr, size);
		}
		consistent_sync(buf->safe, size, direction);

		dma_addr = buf->safe_dma_addr;
	} else {
		consistent_sync(ptr, size, direction);
	}

	local_irq_restore(flags);
	return dma_addr;
}

/*
 * see if a mapped address was really a "safe" buffer and if so, copy
 * the data from the safe buffer back to the unsafe buffer and free up
 * the safe buffer.  (basically return things back to the way they
 * should be)
 */

void
sa1111_unmap_single(dma_addr_t dma_addr, size_t size, int direction)
{
	unsigned long flags;
	struct safe_buffer *buf;

	DPRINTK("%s(ptr=%p,size=%d,dir=%x)\n",
		__func__, (void *) dma_addr, size, direction);

	BUG_ON(direction == PCI_DMA_NONE);

	local_irq_save(flags);

	buf = find_safe_buffer(dma_addr);
	if (buf) {
		BUG_ON(buf->size != size);
		BUG_ON(buf->direction != direction);

		DPRINTK("%s: unsafe buffer %p (phy=%p) mapped to %p (phy=%p)\n",
			__func__,
			buf->ptr, (void *) virt_to_bus(buf->ptr),
			buf->safe, (void *) buf->safe_dma_addr);

		if ((direction == PCI_DMA_FROMDEVICE) ||
		    (direction == PCI_DMA_BIDIRECTIONAL)) {
			DPRINTK("%s: copy back from safe %p, to unsafe %p size %d\n",
				__func__, buf->safe, buf->ptr, size);
			memcpy(buf->ptr, buf->safe, size);
		}
		free_safe_buffer(buf);
	}

	local_irq_restore(flags);
}

int
sa1111_map_sg(struct scatterlist *sg, int nents, int direction)
{
	BUG();			/* Not implemented. */

	return -1;
}

void
sa1111_unmap_sg(struct scatterlist *sg, int nents, int direction)
{
	BUG();			/* Not implemented. */
}

void
sa1111_dma_sync_single(dma_addr_t dma_addr, size_t size, int direction)
{
	unsigned long flags;
	struct safe_buffer *buf;

	DPRINTK("%s(ptr=%p,size=%d,dir=%x)\n",
		__func__, (void *) dma_addr, size, direction);

	local_irq_save(flags);

	buf = find_safe_buffer(dma_addr);
	if (buf) {
		BUG_ON(buf->size != size);
		BUG_ON(buf->direction != direction);

		DPRINTK("%s: unsafe buffer %p (phy=%p) mapped to %p (phy=%p)\n",
			__func__,
			buf->ptr, (void *) virt_to_bus(buf->ptr),
			buf->safe, (void *) buf->safe_dma_addr);

		switch (direction) {
		case PCI_DMA_FROMDEVICE:
			DPRINTK("%s: copy back from safe %p, to unsafe %p size %d\n",
				__func__, buf->safe, buf->ptr, size);
			memcpy(buf->ptr, buf->safe, size);
			break;
		case PCI_DMA_TODEVICE:
			DPRINTK("%s: copy out from unsafe %p, to safe %p, size %d\n",
				__func__,buf->ptr, buf->safe, size);
			memcpy(buf->safe, buf->ptr, size);
			break;
		case PCI_DMA_BIDIRECTIONAL:
			BUG();	/* is this allowed?  what does it mean? */
		default:
			BUG();
		}
		consistent_sync(buf->safe, size, direction);
	} else {
		consistent_sync(bus_to_virt(dma_addr), size, direction);
	}

	local_irq_restore(flags);
}

void
sa1111_dma_sync_sg(struct scatterlist *sg, int nelems, int direction)
{
	BUG();			/* Not implemented. */
}

EXPORT_SYMBOL(sa1111_map_single);
EXPORT_SYMBOL(sa1111_unmap_single);
EXPORT_SYMBOL(sa1111_map_sg);
EXPORT_SYMBOL(sa1111_unmap_sg);
EXPORT_SYMBOL(sa1111_dma_sync_single);
EXPORT_SYMBOL(sa1111_dma_sync_sg);

/* **************************************** */

static int __init sa1111_pcibuf_init(void)
{
	int ret;

	printk(KERN_DEBUG
	       "sa1111_pcibuf: initializing SA-1111 DMA workaround\n");

	ret = create_safe_buffer_pools();

	return ret;
}
module_init(sa1111_pcibuf_init);

static void __exit sa1111_pcibuf_exit(void)
{
	BUG_ON(!list_empty(&safe_buffers));

	destroy_safe_buffer_pools();
}
module_exit(sa1111_pcibuf_exit);

MODULE_AUTHOR("Christopher Hoover <ch@hpl.hp.com>");
MODULE_DESCRIPTION("Special pci_{map/unmap/dma_sync}_* routines for SA-1111.");
MODULE_LICENSE("GPL");
