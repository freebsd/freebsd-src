/* 
 * Motion Eye video4linux driver for Sony Vaio PictureBook
 *
 * Copyright (C) 2001-2003 Stelian Pop <stelian@popies.net>
 *
 * Copyright (C) 2001-2002 Alcôve <www.alcove.com>
 *
 * Copyright (C) 2000 Andrew Tridgell <tridge@valinux.com>
 *
 * Earlier work by Werner Almesberger, Paul `Rusty' Russell and Paul Mackerras.
 *
 * Some parts borrowed from various video4linux drivers, especially
 * bttv-driver.c and zoran.c, see original files for credits.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/videodev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>

#include "meye.h"
#include "linux/meye.h"

/* driver structure - only one possible */
static struct meye meye;
/* number of grab buffers */
static unsigned int gbuffers = 2;
/* size of a grab buffer */
static unsigned int gbufsize = MEYE_MAX_BUFSIZE;
/* /dev/videoX registration number */
static int video_nr = -1;

/****************************************************************************/
/* Queue routines                                                           */
/****************************************************************************/

/* Inits the queue */
static inline void meye_initq(struct meye_queue *queue) {
	queue->head = queue->tail = 0;
	queue->len = 0;
	queue->s_lock = (spinlock_t)SPIN_LOCK_UNLOCKED;
	init_waitqueue_head(&queue->proc_list);
}

/* Pulls an element from the queue */
static inline int meye_pullq(struct meye_queue *queue) {
	int result;
	unsigned long flags;

	spin_lock_irqsave(&queue->s_lock, flags);
	if (!queue->len) {
		spin_unlock_irqrestore(&queue->s_lock, flags);
		return -1;
	}
	result = queue->buf[queue->head];
	queue->head++;
	queue->head &= (MEYE_QUEUE_SIZE - 1);
	queue->len--;
	spin_unlock_irqrestore(&queue->s_lock, flags);
	return result;
}

/* Pushes an element into the queue */
static inline void meye_pushq(struct meye_queue *queue, int element) {
	unsigned long flags;

	spin_lock_irqsave(&queue->s_lock, flags);
	if (queue->len == MEYE_QUEUE_SIZE) {
		/* remove the first element */
		queue->head++;
		queue->head &= (MEYE_QUEUE_SIZE - 1);
		queue->len--;
	}
	queue->buf[queue->tail] = element;
	queue->tail++;
	queue->tail &= (MEYE_QUEUE_SIZE - 1);
	queue->len++;

	spin_unlock_irqrestore(&queue->s_lock, flags);
}

/* Tests if the queue is empty */
static inline int meye_emptyq(struct meye_queue *queue, int *elem) {
	int result;
	unsigned long flags;

	spin_lock_irqsave(&queue->s_lock, flags);
	result = (queue->len == 0);
	if (!result && elem)
		*elem = queue->buf[queue->head];
	spin_unlock_irqrestore(&queue->s_lock, flags);
	return result;
}

/****************************************************************************/
/* Memory allocation routines (stolen from bttv-driver.c)                   */
/****************************************************************************/

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the area.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr) {
        unsigned long kva, ret;

        kva = (unsigned long) page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); /* restore the offset */
	ret = __pa(kva);
        return ret;
}

static void *rvmalloc(unsigned long size) {
	void *mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (mem) {
		memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	        adr = (unsigned long)mem;
		while (size > 0) {
			SetPageReserved(vmalloc_to_page((void *)adr));
			adr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}
	return mem;
}

static void rvfree(void * mem, unsigned long size) {
        unsigned long adr;

	if (mem) {
	        adr = (unsigned long) mem;
		while ((long) size > 0) {
			ClearPageReserved(vmalloc_to_page((void *)adr));
			adr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
		vfree(mem);
	}
}

/****************************************************************************/
/* dma_alloc_coherent / dma_free_coherent ported from 2.5                  */
/****************************************************************************/

void *dma_alloc_coherent(struct pci_dev *dev, size_t size,
                           dma_addr_t *dma_handle, int gfp)
{
        void *ret;
        /* ignore region specifiers */
        gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

        if (dev == NULL || ((u32)dev->dma_mask < 0xffffffff))
                gfp |= GFP_DMA;
        ret = (void *)__get_free_pages(gfp, get_order(size));

        if (ret != NULL) { 
                memset(ret, 0, size);
                *dma_handle = virt_to_phys(ret);
        }       
        return ret;
}

void dma_free_coherent(struct pci_dev *dev, size_t size,
                         void *vaddr, dma_addr_t dma_handle)
{
        free_pages((unsigned long)vaddr, get_order(size));
}

/*
 * return a page table pointing to N pages of locked memory
 *
 * NOTE: The meye device expects dma_addr_t size to be 32 bits
 * (the toc must be exactly 1024 entries each of them being 4 bytes
 * in size, the whole result being 4096 bytes). We're using here
 * dma_addr_t for correctness but the compilation of this driver is
 * disabled for HIGHMEM64G=y, where sizeof(dma_addr_t) != 4
 */
static int ptable_alloc(void) {
	dma_addr_t *pt;
	int i;

	memset(meye.mchip_ptable, 0, sizeof(meye.mchip_ptable));

	meye.mchip_ptable_toc = dma_alloc_coherent(meye.mchip_dev, 
						   PAGE_SIZE, 
						   &meye.mchip_dmahandle,
						   GFP_KERNEL);
	if (!meye.mchip_ptable_toc) {
		meye.mchip_dmahandle = 0;
		return -1;
	}

	pt = meye.mchip_ptable_toc;
	for (i = 0; i < MCHIP_NB_PAGES; i++) {
		meye.mchip_ptable[i] = dma_alloc_coherent(meye.mchip_dev, 
							  PAGE_SIZE,
							  pt,
							  GFP_KERNEL);
		if (!meye.mchip_ptable[i]) {
			int j;
			pt = meye.mchip_ptable_toc;
			for (j = 0; j < i; ++j) {
				dma_free_coherent(meye.mchip_dev,
						  PAGE_SIZE,
						  meye.mchip_ptable[j], *pt);
				pt++;
			}
			dma_free_coherent(meye.mchip_dev,
					  PAGE_SIZE,
					  meye.mchip_ptable_toc,
					  meye.mchip_dmahandle);
			meye.mchip_ptable_toc = 0;
			meye.mchip_dmahandle = 0;
			return -1;
		}
		pt++;
	}
	return 0;
}

static void ptable_free(void) {
	dma_addr_t *pt;
	int i;

	pt = meye.mchip_ptable_toc;
	for (i = 0; i < MCHIP_NB_PAGES; i++) {
		if (meye.mchip_ptable[i])
			dma_free_coherent(meye.mchip_dev, 
					  PAGE_SIZE, 
					  meye.mchip_ptable[i], *pt);
		pt++;
	}

	if (meye.mchip_ptable_toc)
		dma_free_coherent(meye.mchip_dev, 
				  PAGE_SIZE, 
				  meye.mchip_ptable_toc,
				  meye.mchip_dmahandle);

	memset(meye.mchip_ptable, 0, sizeof(meye.mchip_ptable));
	meye.mchip_ptable_toc = 0;
	meye.mchip_dmahandle = 0;
}

/* copy data from ptable into buf */
static void ptable_copy(u8 *buf, int start, int size, int pt_pages) {
	int i;
	
	for (i = 0; i < (size / PAGE_SIZE) * PAGE_SIZE; i += PAGE_SIZE) {
		memcpy(buf + i, meye.mchip_ptable[start++], PAGE_SIZE);
		if (start >= pt_pages)
			start = 0;
	}
	memcpy(buf + i, meye.mchip_ptable[start], size % PAGE_SIZE);
}


/****************************************************************************/
/* JPEG tables at different qualities to load into the VRJ chip             */
/****************************************************************************/

/* return a set of quantisation tables based on a quality from 1 to 10 */
static u16 *jpeg_quantisation_tables(int *size, int quality) {
	static u16 tables0[] = {
		0xdbff, 0x4300, 0xff00, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 
		0xdbff, 0x4300, 0xff01, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 
	};
	static u16 tables1[] = {
		0xdbff, 0x4300, 0x5000, 0x3c37, 0x3c46, 0x5032, 0x4146, 0x5a46, 
		0x5055, 0x785f, 0x82c8, 0x6e78, 0x786e, 0xaff5, 0x91b9, 0xffc8, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 
		0xdbff, 0x4300, 0x5501, 0x5a5a, 0x6978, 0xeb78, 0x8282, 0xffeb, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
		0xffff, 0xffff, 0xffff, 
	};
	static u16 tables2[] = {
		0xdbff, 0x4300, 0x2800, 0x1e1c, 0x1e23, 0x2819, 0x2123, 0x2d23, 
		0x282b, 0x3c30, 0x4164, 0x373c, 0x3c37, 0x587b, 0x495d, 0x9164, 
		0x9980, 0x8f96, 0x8c80, 0xa08a, 0xe6b4, 0xa0c3, 0xdaaa, 0x8aad, 
		0xc88c, 0xcbff, 0xeeda, 0xfff5, 0xffff, 0xc19b, 0xffff, 0xfaff, 
		0xe6ff, 0xfffd, 0xfff8, 
		0xdbff, 0x4300, 0x2b01, 0x2d2d, 0x353c, 0x763c, 0x4141, 0xf876, 
		0x8ca5, 0xf8a5, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 
		0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 
		0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 0xf8f8, 
		0xf8f8, 0xf8f8, 0xfff8, 
	};
	static u16 tables3[] = {
		0xdbff, 0x4300, 0x1b00, 0x1412, 0x1417, 0x1b11, 0x1617, 0x1e17, 
		0x1b1c, 0x2820, 0x2b42, 0x2528, 0x2825, 0x3a51, 0x303d, 0x6042, 
		0x6555, 0x5f64, 0x5d55, 0x6a5b, 0x9978, 0x6a81, 0x9071, 0x5b73, 
		0x855d, 0x86b5, 0x9e90, 0xaba3, 0xabad, 0x8067, 0xc9bc, 0xa6ba, 
		0x99c7, 0xaba8, 0xffa4, 
		0xdbff, 0x4300, 0x1c01, 0x1e1e, 0x2328, 0x4e28, 0x2b2b, 0xa44e, 
		0x5d6e, 0xa46e, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 
		0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 
		0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 
		0xa4a4, 0xa4a4, 0xffa4, 
	};
	static u16 tables4[] = {
		0xdbff, 0x4300, 0x1400, 0x0f0e, 0x0f12, 0x140d, 0x1012, 0x1712, 
		0x1415, 0x1e18, 0x2132, 0x1c1e, 0x1e1c, 0x2c3d, 0x242e, 0x4932, 
		0x4c40, 0x474b, 0x4640, 0x5045, 0x735a, 0x5062, 0x6d55, 0x4556, 
		0x6446, 0x6588, 0x776d, 0x817b, 0x8182, 0x604e, 0x978d, 0x7d8c, 
		0x7396, 0x817e, 0xff7c, 
		0xdbff, 0x4300, 0x1501, 0x1717, 0x1a1e, 0x3b1e, 0x2121, 0x7c3b, 
		0x4653, 0x7c53, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 
		0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 
		0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 0x7c7c, 
		0x7c7c, 0x7c7c, 0xff7c, 
	};
	static u16 tables5[] = {
		0xdbff, 0x4300, 0x1000, 0x0c0b, 0x0c0e, 0x100a, 0x0d0e, 0x120e, 
		0x1011, 0x1813, 0x1a28, 0x1618, 0x1816, 0x2331, 0x1d25, 0x3a28, 
		0x3d33, 0x393c, 0x3833, 0x4037, 0x5c48, 0x404e, 0x5744, 0x3745, 
		0x5038, 0x516d, 0x5f57, 0x6762, 0x6768, 0x4d3e, 0x7971, 0x6470, 
		0x5c78, 0x6765, 0xff63, 
		0xdbff, 0x4300, 0x1101, 0x1212, 0x1518, 0x2f18, 0x1a1a, 0x632f, 
		0x3842, 0x6342, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 
		0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 
		0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 0x6363, 
		0x6363, 0x6363, 0xff63, 
	};
	static u16 tables6[] = {
		0xdbff, 0x4300, 0x0d00, 0x0a09, 0x0a0b, 0x0d08, 0x0a0b, 0x0e0b, 
		0x0d0e, 0x130f, 0x1520, 0x1213, 0x1312, 0x1c27, 0x171e, 0x2e20, 
		0x3129, 0x2e30, 0x2d29, 0x332c, 0x4a3a, 0x333e, 0x4636, 0x2c37, 
		0x402d, 0x4157, 0x4c46, 0x524e, 0x5253, 0x3e32, 0x615a, 0x505a, 
		0x4a60, 0x5251, 0xff4f, 
		0xdbff, 0x4300, 0x0e01, 0x0e0e, 0x1113, 0x2613, 0x1515, 0x4f26, 
		0x2d35, 0x4f35, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 
		0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 
		0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 0x4f4f, 
		0x4f4f, 0x4f4f, 0xff4f, 
	};
	static u16 tables7[] = {
		0xdbff, 0x4300, 0x0a00, 0x0707, 0x0708, 0x0a06, 0x0808, 0x0b08, 
		0x0a0a, 0x0e0b, 0x1018, 0x0d0e, 0x0e0d, 0x151d, 0x1116, 0x2318, 
		0x251f, 0x2224, 0x221f, 0x2621, 0x372b, 0x262f, 0x3429, 0x2129, 
		0x3022, 0x3141, 0x3934, 0x3e3b, 0x3e3e, 0x2e25, 0x4944, 0x3c43, 
		0x3748, 0x3e3d, 0xff3b, 
		0xdbff, 0x4300, 0x0a01, 0x0b0b, 0x0d0e, 0x1c0e, 0x1010, 0x3b1c, 
		0x2228, 0x3b28, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 
		0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 
		0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 0x3b3b, 
		0x3b3b, 0x3b3b, 0xff3b, 
	};
	static u16 tables8[] = {
		0xdbff, 0x4300, 0x0600, 0x0504, 0x0506, 0x0604, 0x0506, 0x0706, 
		0x0607, 0x0a08, 0x0a10, 0x090a, 0x0a09, 0x0e14, 0x0c0f, 0x1710, 
		0x1814, 0x1718, 0x1614, 0x1a16, 0x251d, 0x1a1f, 0x231b, 0x161c, 
		0x2016, 0x202c, 0x2623, 0x2927, 0x292a, 0x1f19, 0x302d, 0x282d, 
		0x2530, 0x2928, 0xff28, 
		0xdbff, 0x4300, 0x0701, 0x0707, 0x080a, 0x130a, 0x0a0a, 0x2813, 
		0x161a, 0x281a, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 
		0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 
		0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 0x2828, 
		0x2828, 0x2828, 0xff28, 
	};
	static u16 tables9[] = {
		0xdbff, 0x4300, 0x0300, 0x0202, 0x0203, 0x0302, 0x0303, 0x0403, 
		0x0303, 0x0504, 0x0508, 0x0405, 0x0504, 0x070a, 0x0607, 0x0c08, 
		0x0c0a, 0x0b0c, 0x0b0a, 0x0d0b, 0x120e, 0x0d10, 0x110e, 0x0b0e, 
		0x100b, 0x1016, 0x1311, 0x1514, 0x1515, 0x0f0c, 0x1817, 0x1416, 
		0x1218, 0x1514, 0xff14, 
		0xdbff, 0x4300, 0x0301, 0x0404, 0x0405, 0x0905, 0x0505, 0x1409, 
		0x0b0d, 0x140d, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 
		0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 
		0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 0x1414, 
		0x1414, 0x1414, 0xff14, 
	};
	static u16 tables10[] = {
		0xdbff, 0x4300, 0x0100, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0xff01, 
		0xdbff, 0x4300, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0101, 0x0101, 0xff01, 
	};

	switch (quality) {
	case 0:
		*size = sizeof(tables0);
		return tables0;
	case 1:
		*size = sizeof(tables1);
		return tables1;
	case 2:
		*size = sizeof(tables2);
		return tables2;
	case 3:
		*size = sizeof(tables3);
		return tables3;
	case 4:
		*size = sizeof(tables4);
		return tables4;
	case 5:
		*size = sizeof(tables5);
		return tables5;
	case 6:
		*size = sizeof(tables6);
		return tables6;
	case 7:
		*size = sizeof(tables7);
		return tables7;
	case 8:
		*size = sizeof(tables8);
		return tables8;
	case 9:
		*size = sizeof(tables9);
		return tables9;
	case 10:
		*size = sizeof(tables10);
		return tables10;
	default:
		printk(KERN_WARNING "meye: invalid quality level %d - using 8\n", quality);
		*size = sizeof(tables8);
		return tables8;
	}
	return NULL;
}

/* return a generic set of huffman tables */
static u16 *jpeg_huffman_tables(int *size) {
	static u16 tables[] = {
		0xC4FF, 0xB500, 0x0010, 0x0102, 0x0303, 0x0402, 0x0503, 0x0405, 
		0x0004, 0x0100, 0x017D, 0x0302, 0x0400, 0x0511, 0x2112, 0x4131, 
		0x1306, 0x6151, 0x2207, 0x1471, 0x8132, 0xA191, 0x2308, 0xB142, 
		0x15C1, 0xD152, 0x24F0, 0x6233, 0x8272, 0x0A09, 0x1716, 0x1918, 
		0x251A, 0x2726, 0x2928, 0x342A, 0x3635, 0x3837, 0x3A39, 0x4443, 
		0x4645, 0x4847, 0x4A49, 0x5453, 0x5655, 0x5857, 0x5A59, 0x6463, 
		0x6665, 0x6867, 0x6A69, 0x7473, 0x7675, 0x7877, 0x7A79, 0x8483, 
		0x8685, 0x8887, 0x8A89, 0x9392, 0x9594, 0x9796, 0x9998, 0xA29A, 
		0xA4A3, 0xA6A5, 0xA8A7, 0xAAA9, 0xB3B2, 0xB5B4, 0xB7B6, 0xB9B8, 
		0xC2BA, 0xC4C3, 0xC6C5, 0xC8C7, 0xCAC9, 0xD3D2, 0xD5D4, 0xD7D6, 
		0xD9D8, 0xE1DA, 0xE3E2, 0xE5E4, 0xE7E6, 0xE9E8, 0xF1EA, 0xF3F2, 
		0xF5F4, 0xF7F6, 0xF9F8, 0xFFFA, 
		0xC4FF, 0xB500, 0x0011, 0x0102, 0x0402, 0x0304, 0x0704, 0x0405, 
		0x0004, 0x0201, 0x0077, 0x0201, 0x1103, 0x0504, 0x3121, 0x1206, 
		0x5141, 0x6107, 0x1371, 0x3222, 0x0881, 0x4214, 0xA191, 0xC1B1, 
		0x2309, 0x5233, 0x15F0, 0x7262, 0x0AD1, 0x2416, 0xE134, 0xF125, 
		0x1817, 0x1A19, 0x2726, 0x2928, 0x352A, 0x3736, 0x3938, 0x433A, 
		0x4544, 0x4746, 0x4948, 0x534A, 0x5554, 0x5756, 0x5958, 0x635A, 
		0x6564, 0x6766, 0x6968, 0x736A, 0x7574, 0x7776, 0x7978, 0x827A, 
		0x8483, 0x8685, 0x8887, 0x8A89, 0x9392, 0x9594, 0x9796, 0x9998, 
		0xA29A, 0xA4A3, 0xA6A5, 0xA8A7, 0xAAA9, 0xB3B2, 0xB5B4, 0xB7B6, 
		0xB9B8, 0xC2BA, 0xC4C3, 0xC6C5, 0xC8C7, 0xCAC9, 0xD3D2, 0xD5D4, 
		0xD7D6, 0xD9D8, 0xE2DA, 0xE4E3, 0xE6E5, 0xE8E7, 0xEAE9, 0xF3F2, 
		0xF5F4, 0xF7F6, 0xF9F8, 0xFFFA, 
		0xC4FF, 0x1F00, 0x0000, 0x0501, 0x0101, 0x0101, 0x0101, 0x0000, 
		0x0000, 0x0000, 0x0000, 0x0201, 0x0403, 0x0605, 0x0807, 0x0A09, 
		0xFF0B, 
		0xC4FF, 0x1F00, 0x0001, 0x0103, 0x0101, 0x0101, 0x0101, 0x0101, 
		0x0000, 0x0000, 0x0000, 0x0201, 0x0403, 0x0605, 0x0807, 0x0A09, 
		0xFF0B
	};

	*size = sizeof(tables);
	return tables;
}

/****************************************************************************/
/* MCHIP low-level functions                                                */
/****************************************************************************/

/* waits for the specified miliseconds */
static inline void wait_ms(unsigned int ms) {
	if (!in_interrupt()) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1 + ms * HZ / 1000);
	}
	else
		mdelay(ms);
}

/* returns the horizontal capture size */
static inline int mchip_hsize(void) {
	return meye.params.subsample ? 320 : 640;
}

/* returns the vertical capture size */
static inline int mchip_vsize(void) {
	return meye.params.subsample ? 240 : 480;
}

/* waits for a register to be available */
static void mchip_sync(int reg) {
	u32 status;
	int i;

	if (reg == MCHIP_MM_FIFO_DATA) {
		for (i = 0; i < MCHIP_REG_TIMEOUT; i++) {
			status = readl(meye.mchip_mmregs + MCHIP_MM_FIFO_STATUS);
			if (!(status & MCHIP_MM_FIFO_WAIT)) {
				printk(KERN_WARNING "meye: fifo not ready\n");
				return;
			}
			if (status & MCHIP_MM_FIFO_READY)
				return;
			udelay(1);
		}
	}
	else if (reg > 0x80) {
		u32 mask = (reg < 0x100) ? MCHIP_HIC_STATUS_MCC_RDY
			                 : MCHIP_HIC_STATUS_VRJ_RDY;
		for (i = 0; i < MCHIP_REG_TIMEOUT; i++) {
			status = readl(meye.mchip_mmregs + MCHIP_HIC_STATUS);
			if (status & mask)
				return;
			udelay(1);
		}
	}
	else
		return;
	printk(KERN_WARNING "meye: mchip_sync() timeout on reg 0x%x status=0x%x\n", reg, status);
}

/* sets a value into the register */
static inline void mchip_set(int reg, u32 v) {
	mchip_sync(reg);
	writel(v, meye.mchip_mmregs + reg);
}

/* get the register value */
static inline u32 mchip_read(int reg) {
	mchip_sync(reg);
	return readl(meye.mchip_mmregs + reg);
}

/* wait for a register to become a particular value */
static inline int mchip_delay(u32 reg, u32 v) {
	int n = 10;
	while (--n && mchip_read(reg) != v) 
		udelay(1);
	return n;
}

/* setup subsampling */
static void mchip_subsample(void) {
	mchip_set(MCHIP_MCC_R_SAMPLING, meye.params.subsample);
	mchip_set(MCHIP_MCC_R_XRANGE, mchip_hsize());
	mchip_set(MCHIP_MCC_R_YRANGE, mchip_vsize());
	mchip_set(MCHIP_MCC_B_XRANGE, mchip_hsize());
	mchip_set(MCHIP_MCC_B_YRANGE, mchip_vsize());
	mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE);
}

/* set the framerate into the mchip */
static void mchip_set_framerate(void) {
	mchip_set(MCHIP_HIC_S_RATE, meye.params.framerate);
}

/* load some huffman and quantisation tables into the VRJ chip ready
   for JPEG compression */
static void mchip_load_tables(void) {
	int i;
	int size;
	u16 *tables;

	tables = jpeg_huffman_tables(&size);
	for (i = 0; i < size / 2; i++)
		writel(tables[i], meye.mchip_mmregs + MCHIP_VRJ_TABLE_DATA);

	tables = jpeg_quantisation_tables(&size, meye.params.quality);
	for (i = 0; i < size / 2; i++)
		writel(tables[i], meye.mchip_mmregs + MCHIP_VRJ_TABLE_DATA);
}

/* setup the VRJ parameters in the chip */
static void mchip_vrj_setup(u8 mode) {

	mchip_set(MCHIP_VRJ_BUS_MODE, 5);
	mchip_set(MCHIP_VRJ_SIGNAL_ACTIVE_LEVEL, 0x1f);
	mchip_set(MCHIP_VRJ_PDAT_USE, 1);
	mchip_set(MCHIP_VRJ_IRQ_FLAG, 0xa0);
	mchip_set(MCHIP_VRJ_MODE_SPECIFY, mode);
	mchip_set(MCHIP_VRJ_NUM_LINES, mchip_vsize());
	mchip_set(MCHIP_VRJ_NUM_PIXELS, mchip_hsize());
	mchip_set(MCHIP_VRJ_NUM_COMPONENTS, 0x1b);
	mchip_set(MCHIP_VRJ_LIMIT_COMPRESSED_LO, 0xFFFF);
	mchip_set(MCHIP_VRJ_LIMIT_COMPRESSED_HI, 0xFFFF);
	mchip_set(MCHIP_VRJ_COMP_DATA_FORMAT, 0xC);
	mchip_set(MCHIP_VRJ_RESTART_INTERVAL, 0);
	mchip_set(MCHIP_VRJ_SOF1, 0x601);
	mchip_set(MCHIP_VRJ_SOF2, 0x1502);
	mchip_set(MCHIP_VRJ_SOF3, 0x1503);
	mchip_set(MCHIP_VRJ_SOF4, 0x1596);
	mchip_set(MCHIP_VRJ_SOS,  0x0ed0);

	mchip_load_tables();
}

/* sets the DMA parameters into the chip */
static void mchip_dma_setup(u32 dma_addr) {
	int i;

	mchip_set(MCHIP_MM_PT_ADDR, dma_addr);
	for (i = 0; i < 4; i++)
		mchip_set(MCHIP_MM_FIR(i), 0);
	meye.mchip_fnum = 0;
}

/* setup for DMA transfers - also zeros the framebuffer */
static int mchip_dma_alloc(void) {
	if (!meye.mchip_dmahandle)
		if (ptable_alloc())
			return -1;
	return 0;
}

/* frees the DMA buffer */
static void mchip_dma_free(void) {
	if (meye.mchip_dmahandle) {
		mchip_dma_setup(0);
		ptable_free();
	}
}

/* stop any existing HIC action and wait for any dma to complete then
   reset the dma engine */
static void mchip_hic_stop(void) {
	int i, j;

	meye.mchip_mode = MCHIP_HIC_MODE_NOOP;
	if (!(mchip_read(MCHIP_HIC_STATUS) & MCHIP_HIC_STATUS_BUSY))
		return;
	for (i = 0; i < 20; ++i) {
		mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_STOP);
		mchip_delay(MCHIP_HIC_CMD, 0);
		for (j = 0; j < 100; ++j) {
			if (mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE))
				return;
			wait_ms(1);
		}
		printk(KERN_ERR "meye: need to reset HIC!\n");
	
		mchip_set(MCHIP_HIC_CTL, MCHIP_HIC_CTL_SOFT_RESET);
		wait_ms(250);
	}
	printk(KERN_ERR "meye: resetting HIC hanged!\n");
}

/****************************************************************************/
/* MCHIP frame processing functions                                         */
/****************************************************************************/

/* get the next ready frame from the dma engine */
static u32 mchip_get_frame(void) {
	u32 v;
	
	v = mchip_read(MCHIP_MM_FIR(meye.mchip_fnum));
	return v;
}

/* frees the current frame from the dma engine */
static void mchip_free_frame(void) {
	mchip_set(MCHIP_MM_FIR(meye.mchip_fnum), 0);
	meye.mchip_fnum++;
	meye.mchip_fnum %= 4;
}

/* read one frame from the framebuffer assuming it was captured using
   a uncompressed transfer */
static void mchip_cont_read_frame(u32 v, u8 *buf, int size) {
	int pt_id;

	pt_id = (v >> 17) & 0x3FF;

	ptable_copy(buf, pt_id, size, MCHIP_NB_PAGES);

}

/* read a compressed frame from the framebuffer */
static int mchip_comp_read_frame(u32 v, u8 *buf, int size) {
	int pt_start, pt_end, trailer;
	int fsize;
	int i;

	pt_start = (v >> 19) & 0xFF;
	pt_end = (v >> 11) & 0xFF;
	trailer = (v >> 1) & 0x3FF;

	if (pt_end < pt_start)
		fsize = (MCHIP_NB_PAGES_MJPEG - pt_start) * PAGE_SIZE +
			pt_end * PAGE_SIZE + trailer * 4;
	else
		fsize = (pt_end - pt_start) * PAGE_SIZE + trailer * 4;

	if (fsize > size) {
		printk(KERN_WARNING "meye: oversized compressed frame %d\n", 
		       fsize);
		return -1;
	}

	ptable_copy(buf, pt_start, fsize, MCHIP_NB_PAGES_MJPEG);


#ifdef MEYE_JPEG_CORRECTION

	/* Some mchip generated jpeg frames are incorrect. In most
	 * (all ?) of those cases, the final EOI (0xff 0xd9) marker 
	 * is not present at the end of the frame.
	 *
	 * Since adding the final marker is not enough to restore
	 * the jpeg integrity, we drop the frame.
	 */

	for (i = fsize - 1; i > 0 && buf[i] == 0xff; i--) ;

	if (i < 2 || buf[i - 1] != 0xff || buf[i] != 0xd9)
		return -1;

#endif

	return fsize;
}

/* take a picture into SDRAM */
static void mchip_take_picture(void) {
	int i;
	
	mchip_hic_stop();
	mchip_subsample();
	mchip_dma_setup(meye.mchip_dmahandle);

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_STILL_CAP);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);

	mchip_delay(MCHIP_HIC_CMD, 0);

	for (i = 0; i < 100; ++i) {
		if (mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE))
			break;
		wait_ms(1);
	}
}

/* dma a previously taken picture into a buffer */
static void mchip_get_picture(u8 *buf, int bufsize) {
	u32 v;
	int i;

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_STILL_OUT);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);

	mchip_delay(MCHIP_HIC_CMD, 0);
	for (i = 0; i < 100; ++i) {
		if (mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE))
			break;
		wait_ms(1);
	}
	for (i = 0; i < 4 ; ++i) {
		v = mchip_get_frame();
		if (v & MCHIP_MM_FIR_RDY) {
			mchip_cont_read_frame(v, buf, bufsize);
			break;
		}
		mchip_free_frame();
	}
}

/* start continuous dma capture */
static void mchip_continuous_start(void) {
	mchip_hic_stop();
	mchip_subsample();
	mchip_set_framerate();
	mchip_dma_setup(meye.mchip_dmahandle);

	meye.mchip_mode = MCHIP_HIC_MODE_CONT_OUT;

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_CONT_OUT);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);

	mchip_delay(MCHIP_HIC_CMD, 0);
}

/* compress one frame into a buffer */
static int mchip_compress_frame(u8 *buf, int bufsize) {
	u32 v;
	int len = -1, i;

	mchip_vrj_setup(0x3f);
	udelay(50);

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_STILL_COMP);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);
	
	mchip_delay(MCHIP_HIC_CMD, 0);
	for (i = 0; i < 100; ++i) {
		if (mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE))
			break;
		wait_ms(1);
	}

	for (i = 0; i < 4 ; ++i) {
		v = mchip_get_frame();
		if (v & MCHIP_MM_FIR_RDY) {
			len = mchip_comp_read_frame(v, buf, bufsize);
			break;
		}
		mchip_free_frame();
	}
	return len;
}

#if 0
/* uncompress one image into a buffer */
static int mchip_uncompress_frame(u8 *img, int imgsize, u8 *buf, int bufsize) {
	mchip_vrj_setup(0x3f);
	udelay(50);

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_STILL_DECOMP);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);
	
	mchip_delay(MCHIP_HIC_CMD, 0);

	return mchip_comp_read_frame(buf, bufsize);
}
#endif

/* start continuous compressed capture */
static void mchip_cont_compression_start(void) {
	mchip_hic_stop();
	mchip_vrj_setup(0x3f);
	mchip_subsample();
	mchip_set_framerate();
	mchip_dma_setup(meye.mchip_dmahandle);

	meye.mchip_mode = MCHIP_HIC_MODE_CONT_COMP;

	mchip_set(MCHIP_HIC_MODE, MCHIP_HIC_MODE_CONT_COMP);
	mchip_set(MCHIP_HIC_CMD, MCHIP_HIC_CMD_START);

	mchip_delay(MCHIP_HIC_CMD, 0);
}

/****************************************************************************/
/* Interrupt handling                                                       */
/****************************************************************************/

static irqreturn_t meye_irq(int irq, void *dev_id, struct pt_regs *regs) {
	u32 v;
	int reqnr;
	v = mchip_read(MCHIP_MM_INTA);

	while (1) {
		v = mchip_get_frame();
		if (!(v & MCHIP_MM_FIR_RDY))
			return IRQ_NONE;
		switch (meye.mchip_mode) {

		case MCHIP_HIC_MODE_CONT_OUT:
			if (!meye_emptyq(&meye.grabq, NULL)) {
				int nr = meye_pullq(&meye.grabq);
				mchip_cont_read_frame(
					v, 
					meye.grab_fbuffer + gbufsize * nr,
					mchip_hsize() * mchip_vsize() * 2);
				meye.grab_buffer[nr].state = MEYE_BUF_DONE;
				wake_up_interruptible(&meye.grabq.proc_list);
			}
			break;

		case MCHIP_HIC_MODE_CONT_COMP:
			if (!meye_emptyq(&meye.grabq, &reqnr)) {
				int size;
				size = mchip_comp_read_frame(
					v,
					meye.grab_fbuffer + gbufsize * reqnr,
					gbufsize);
				if (size == -1)
					break;
				reqnr = meye_pullq(&meye.grabq);
				meye.grab_buffer[reqnr].size = size;
				meye.grab_buffer[reqnr].state = MEYE_BUF_DONE;
				wake_up_interruptible(&meye.grabq.proc_list);
			}
			break;

		default:
			/* do not free frame, since it can be a snap */
			return IRQ_NONE;
		} /* switch */

		mchip_free_frame();
	}
	return IRQ_HANDLED;
}

/****************************************************************************/
/* video4linux integration                                                  */
/****************************************************************************/

static int meye_open(struct inode *inode, struct file *file) {
	int i, err;

	err = video_exclusive_open(inode,file);
	if (err < 0)
		return err;
			
	if (mchip_dma_alloc()) {
		printk(KERN_ERR "meye: mchip framebuffer allocation failed\n");
		video_exclusive_release(inode,file);
		return -ENOBUFS;
	}
	mchip_hic_stop();
	meye_initq(&meye.grabq);
	for (i = 0; i < MEYE_MAX_BUFNBRS; i++)
		meye.grab_buffer[i].state = MEYE_BUF_UNUSED;
	return 0;
}

static int meye_release(struct inode *inode, struct file *file) {
	mchip_hic_stop();
	mchip_dma_free();
	video_exclusive_release(inode,file);
	return 0;
}

static int meye_do_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, void *arg) {

	switch (cmd) {

	case VIDIOCGCAP: {
		struct video_capability *b = arg;
		strcpy(b->name,meye.video_dev->name);
		b->type = VID_TYPE_CAPTURE;
		b->channels = 1;
		b->audios = 0;
		b->maxwidth = 640;
		b->maxheight = 480;
		b->minwidth = 320;
		b->minheight = 240;
		break;
	}

	case VIDIOCGCHAN: {
		struct video_channel *v = arg;
		v->flags = 0;
		v->tuners = 0;
		v->type = VIDEO_TYPE_CAMERA;
		if (v->channel != 0)
			return -EINVAL;
		strcpy(v->name,"Camera");
		break;
	}

	case VIDIOCSCHAN: {
		struct video_channel *v = arg;
		if (v->channel != 0)
			return -EINVAL;
		break;
	}

	case VIDIOCGPICT: {
		struct video_picture *p = arg;
		*p = meye.picture;
		break;
	}

	case VIDIOCSPICT: {
		struct video_picture *p = arg;
		if (p->depth != 2)
			return -EINVAL;
		if (p->palette != VIDEO_PALETTE_YUV422)
			return -EINVAL;
		down(&meye.lock);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERABRIGHTNESS, 
				      p->brightness >> 10);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERAHUE, 
				      p->hue >> 10);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERACOLOR, 
				      p->colour >> 10);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERACONTRAST, 
				      p->contrast >> 10);
		meye.picture = *p;
		up(&meye.lock);
		break;
	}

	case VIDIOCSYNC: {
		int *i = arg;

		if (*i < 0 || *i >= gbuffers)
			return -EINVAL;

		switch (meye.grab_buffer[*i].state) {

		case MEYE_BUF_UNUSED:
			return -EINVAL;
		case MEYE_BUF_USING:
			if (wait_event_interruptible(meye.grabq.proc_list,
						     (meye.grab_buffer[*i].state != MEYE_BUF_USING)))
				return -EINTR;
			/* fall through */
		case MEYE_BUF_DONE:
			meye.grab_buffer[*i].state = MEYE_BUF_UNUSED;
		}
		break;
	}

	case VIDIOCMCAPTURE: {
		struct video_mmap *vm = arg;
		int restart = 0;

		if (vm->frame >= gbuffers || vm->frame < 0)
			return -EINVAL;
		if (vm->format != VIDEO_PALETTE_YUV422)
			return -EINVAL;
		if (vm->height * vm->width * 2 > gbufsize)
			return -EINVAL;
		if (!meye.grab_fbuffer)
			return -EINVAL;
		if (meye.grab_buffer[vm->frame].state != MEYE_BUF_UNUSED)
			return -EBUSY;

		down(&meye.lock);
		if (vm->width == 640 && vm->height == 480) {
			if (meye.params.subsample) {
				meye.params.subsample = 0;
				restart = 1;
			}
		}
		else if (vm->width == 320 && vm->height == 240) {
			if (!meye.params.subsample) {
				meye.params.subsample = 1;
				restart = 1;
			}
		}
		else {
			up(&meye.lock);
			return -EINVAL;
		}

		if (restart || meye.mchip_mode != MCHIP_HIC_MODE_CONT_OUT)
			mchip_continuous_start();
		meye.grab_buffer[vm->frame].state = MEYE_BUF_USING;
		meye_pushq(&meye.grabq, vm->frame);
		up(&meye.lock);
		break;
	}

	case VIDIOCGMBUF: {
		struct video_mbuf *vm = arg;
		int i;

		memset(vm, 0 , sizeof(*vm));
		vm->size = gbufsize * gbuffers;
		vm->frames = gbuffers;
		for (i = 0; i < gbuffers; i++)
			vm->offsets[i] = i * gbufsize;
		break;
	}

	case MEYEIOC_G_PARAMS: {
		struct meye_params *p = arg;
		*p = meye.params;
		break;
	}

	case MEYEIOC_S_PARAMS: {
		struct meye_params *jp = arg;
		if (jp->subsample > 1)
			return -EINVAL;
		if (jp->quality > 10)
			return -EINVAL;
		if (jp->sharpness > 63 || jp->agc > 63 || jp->picture > 63)
			return -EINVAL;
		if (jp->framerate > 31)
			return -EINVAL;
		down(&meye.lock);
		if (meye.params.subsample != jp->subsample ||
		    meye.params.quality != jp->quality)
			mchip_hic_stop();	/* need restart */
		meye.params = *jp;
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERASHARPNESS,
				      meye.params.sharpness);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERAAGC,
				      meye.params.agc);
		sonypi_camera_command(SONYPI_COMMAND_SETCAMERAPICTURE,
				      meye.params.picture);
		up(&meye.lock);
		break;
	}

	case MEYEIOC_QBUF_CAPT: {
		int *nb = arg;

		if (!meye.grab_fbuffer) 
			return -EINVAL;
		if (*nb >= gbuffers)
			return -EINVAL;
		if (*nb < 0) {
			/* stop capture */
			mchip_hic_stop();
			return 0;
		}
		if (meye.grab_buffer[*nb].state != MEYE_BUF_UNUSED)
			return -EBUSY;
		down(&meye.lock);
		if (meye.mchip_mode != MCHIP_HIC_MODE_CONT_COMP)
			mchip_cont_compression_start();
		meye.grab_buffer[*nb].state = MEYE_BUF_USING;
		meye_pushq(&meye.grabq, *nb);
		up(&meye.lock);
		break;
	}

	case MEYEIOC_SYNC: {
		int *i = arg;

		if (*i < 0 || *i >= gbuffers)
			return -EINVAL;

		switch (meye.grab_buffer[*i].state) {

		case MEYE_BUF_UNUSED:
			return -EINVAL;
		case MEYE_BUF_USING:
			if (wait_event_interruptible(meye.grabq.proc_list,
						     (meye.grab_buffer[*i].state != MEYE_BUF_USING)))
				return -EINTR;
			/* fall through */
		case MEYE_BUF_DONE:
			meye.grab_buffer[*i].state = MEYE_BUF_UNUSED;
		}
		*i = meye.grab_buffer[*i].size;
		break;
	}

	case MEYEIOC_STILLCAPT: {

		if (!meye.grab_fbuffer) 
			return -EINVAL;
		if (meye.grab_buffer[0].state != MEYE_BUF_UNUSED)
			return -EBUSY;
		down(&meye.lock);
		meye.grab_buffer[0].state = MEYE_BUF_USING;
		mchip_take_picture();
		mchip_get_picture(
			meye.grab_fbuffer,
			mchip_hsize() * mchip_vsize() * 2);
		meye.grab_buffer[0].state = MEYE_BUF_DONE;
		up(&meye.lock);
		break;
	}

	case MEYEIOC_STILLJCAPT: {
		int *len = arg;

		if (!meye.grab_fbuffer) 
			return -EINVAL;
		if (meye.grab_buffer[0].state != MEYE_BUF_UNUSED)
			return -EBUSY;
		down(&meye.lock);
		meye.grab_buffer[0].state = MEYE_BUF_USING;
		*len = -1;
		while (*len == -1) {
			mchip_take_picture();
			*len = mchip_compress_frame(meye.grab_fbuffer, gbufsize);
		}
		meye.grab_buffer[0].state = MEYE_BUF_DONE;
		up(&meye.lock);
		break;
	}

	default:
		return -ENOIOCTLCMD;
		
	} /* switch */

	return 0;
}

static int meye_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	return video_usercopy(inode, file, cmd, arg, meye_do_ioctl);
}

static int meye_mmap(struct file *file, struct vm_area_struct *vma) {
	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long page, pos;

	down(&meye.lock);
	if (size > gbuffers * gbufsize) {
		up(&meye.lock);
		return -EINVAL;
	}
	if (!meye.grab_fbuffer) {
		/* lazy allocation */
		meye.grab_fbuffer = rvmalloc(gbuffers*gbufsize);
		if (!meye.grab_fbuffer) {
			printk(KERN_ERR "meye: v4l framebuffer allocation failed\n");
			up(&meye.lock);
			return -ENOMEM;
		}
	}
	pos = (unsigned long)meye.grab_fbuffer;

	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED)) {
			up(&meye.lock);
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	up(&meye.lock);
	return 0;
}

static struct file_operations meye_fops = {
	.owner		= THIS_MODULE,
	.open		= meye_open,
	.release	= meye_release,
	.mmap		= meye_mmap,
	.ioctl		= meye_ioctl,
	.llseek		= no_llseek,
};

static struct video_device meye_template = {
	.owner		= THIS_MODULE,
	.name		= "meye",
	.type		= VID_TYPE_CAPTURE,
	.hardware	= VID_HARDWARE_MEYE,
	.fops		= &meye_fops,
	.release	= video_device_release,
	.minor		= -1,
};

#ifdef CONFIG_PM
static int meye_suspend(struct pci_dev *pdev, u32 state)
{
	pci_save_state(pdev, meye.pm_state);
	meye.pm_mchip_mode = meye.mchip_mode;
	mchip_hic_stop();
	mchip_set(MCHIP_MM_INTA, 0x0);
	return 0;
}

static int meye_resume(struct pci_dev *pdev)
{
	pci_restore_state(pdev, meye.pm_state);
	pci_write_config_word(meye.mchip_dev, MCHIP_PCI_SOFTRESET_SET, 1);

	mchip_delay(MCHIP_HIC_CMD, 0);
	mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE);
	wait_ms(1);
	mchip_set(MCHIP_VRJ_SOFT_RESET, 1);
	wait_ms(1);
	mchip_set(MCHIP_MM_PCI_MODE, 5);
	wait_ms(1);
	mchip_set(MCHIP_MM_INTA, MCHIP_MM_INTA_HIC_1_MASK);

	switch (meye.pm_mchip_mode) {
	case MCHIP_HIC_MODE_CONT_OUT:
		mchip_continuous_start();
		break;
	case MCHIP_HIC_MODE_CONT_COMP:
		mchip_cont_compression_start();
		break;
	}
	return 0;
}
#endif

static int __devinit meye_probe(struct pci_dev *pcidev, 
		                const struct pci_device_id *ent) {
	int ret;
	unsigned long mchip_adr;
	u8 revision;

	if (meye.mchip_dev != NULL) {
		printk(KERN_ERR "meye: only one device allowed!\n");
		ret = -EBUSY;
		goto out1;
	}

	meye.mchip_dev = pcidev;
	meye.video_dev = video_device_alloc();
	if (!meye.video_dev) {
		printk(KERN_ERR "meye: video_device_alloc() failed!\n");
		ret = -EBUSY;
		goto out1;
	}
	memcpy(meye.video_dev, &meye_template, sizeof(meye_template));

	sonypi_camera_command(SONYPI_COMMAND_SETCAMERA, 1);

	if ((ret = pci_enable_device(meye.mchip_dev))) {
		printk(KERN_ERR "meye: pci_enable_device failed\n");
		goto out2;
	}

	meye.mchip_irq = pcidev->irq;
	mchip_adr = pci_resource_start(meye.mchip_dev,0);
	if (!mchip_adr) {
		printk(KERN_ERR "meye: mchip has no device base address\n");
		ret = -EIO;
		goto out3;
	}
	if (!request_mem_region(pci_resource_start(meye.mchip_dev, 0),
			        pci_resource_len(meye.mchip_dev, 0),
				"meye")) {
		ret = -EIO;
		printk(KERN_ERR "meye: request_mem_region failed\n");
		goto out3;
	}

	pci_read_config_byte(meye.mchip_dev, PCI_REVISION_ID, &revision);

	pci_set_master(meye.mchip_dev);

	pci_write_config_byte(meye.mchip_dev, PCI_CACHE_LINE_SIZE, 8);
	pci_write_config_byte(meye.mchip_dev, PCI_LATENCY_TIMER, 64);

	if ((ret = request_irq(meye.mchip_irq, meye_irq, 
			       SA_INTERRUPT | SA_SHIRQ, "meye", meye_irq))) {
		printk(KERN_ERR "meye: request_irq failed (ret=%d)\n", ret);
		goto out4;
	}

	meye.mchip_mmregs = ioremap(mchip_adr, MCHIP_MM_REGS);
	if (!meye.mchip_mmregs) {
		printk(KERN_ERR "meye: ioremap failed\n");
		ret = -EIO;
		goto out5;
	}
	
	/* Ask the camera to perform a soft reset. */
	pci_write_config_word(meye.mchip_dev, MCHIP_PCI_SOFTRESET_SET, 1);

	mchip_delay(MCHIP_HIC_CMD, 0);
	mchip_delay(MCHIP_HIC_STATUS, MCHIP_HIC_STATUS_IDLE);

	wait_ms(1);
	mchip_set(MCHIP_VRJ_SOFT_RESET, 1);

	wait_ms(1);
	mchip_set(MCHIP_MM_PCI_MODE, 5);

	wait_ms(1);
	mchip_set(MCHIP_MM_INTA, MCHIP_MM_INTA_HIC_1_MASK);

	if (video_register_device(meye.video_dev, VFL_TYPE_GRABBER, video_nr) < 0) {

		printk(KERN_ERR "meye: video_register_device failed\n");
		ret = -EIO;
		goto out6;
	}
	
	printk(KERN_INFO "meye: Motion Eye Camera Driver v%d.%d.\n",
	       MEYE_DRIVER_MAJORVERSION,
	       MEYE_DRIVER_MINORVERSION);
	printk(KERN_INFO "meye: mchip KL5A72002 rev. %d, base %lx, irq %d\n", 
		revision, mchip_adr, meye.mchip_irq);

	/* init all fields */
	init_MUTEX(&meye.lock);

	meye.picture.depth = 2;
	meye.picture.palette = VIDEO_PALETTE_YUV422;
	meye.picture.brightness = 32 << 10;
	meye.picture.hue = 32 << 10;
	meye.picture.colour = 32 << 10;
	meye.picture.contrast = 32 << 10;
	meye.picture.whiteness = 0;
	meye.params.subsample = 0;
	meye.params.quality = 7;
	meye.params.sharpness = 32;
	meye.params.agc = 48;
	meye.params.picture = 0;
	meye.params.framerate = 0;
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERABRIGHTNESS, 32);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERAHUE, 32);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERACOLOR, 32);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERACONTRAST, 32);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERASHARPNESS, 32);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERAPICTURE, 0);
	sonypi_camera_command(SONYPI_COMMAND_SETCAMERAAGC, 48);

	return 0;
out6:
	iounmap(meye.mchip_mmregs);
out5:
	free_irq(meye.mchip_irq, meye_irq);
out4:
	release_mem_region(pci_resource_start(meye.mchip_dev, 0),
			   pci_resource_len(meye.mchip_dev, 0));
out3:
	pci_disable_device(meye.mchip_dev);
out2:
	video_device_release(meye.video_dev);
	meye.video_dev = NULL;

	sonypi_camera_command(SONYPI_COMMAND_SETCAMERA, 0);
out1:
	return ret;
}

static void __devexit meye_remove(struct pci_dev *pcidev) {

	video_unregister_device(meye.video_dev);

	mchip_hic_stop();

	mchip_dma_free();

	/* disable interrupts */
	mchip_set(MCHIP_MM_INTA, 0x0);

	free_irq(meye.mchip_irq, meye_irq);

	iounmap(meye.mchip_mmregs);

	release_mem_region(pci_resource_start(meye.mchip_dev, 0),
			   pci_resource_len(meye.mchip_dev, 0));

	pci_disable_device(meye.mchip_dev);

	if (meye.grab_fbuffer)
		rvfree(meye.grab_fbuffer, gbuffers*gbufsize);

	sonypi_camera_command(SONYPI_COMMAND_SETCAMERA, 0);

	printk(KERN_INFO "meye: removed\n");
}

static struct pci_device_id meye_pci_tbl[] = {
	{ PCI_VENDOR_ID_KAWASAKI, PCI_DEVICE_ID_MCHIP_KL5A72002, 
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ }
};

MODULE_DEVICE_TABLE(pci, meye_pci_tbl);

static struct pci_driver meye_driver = {
	.name		= "meye",
	.id_table	= meye_pci_tbl,
	.probe		= meye_probe,
	.remove		= __devexit_p(meye_remove),
#ifdef CONFIG_PM
	.suspend	= meye_suspend,
	.resume		= meye_resume,
#endif
};

static int __init meye_init_module(void) {
	if (gbuffers < 2)
		gbuffers = 2;
	if (gbuffers > MEYE_MAX_BUFNBRS)
		gbuffers = MEYE_MAX_BUFNBRS;
	if (gbufsize < 0 || gbufsize > MEYE_MAX_BUFSIZE)
		gbufsize = MEYE_MAX_BUFSIZE;
	printk(KERN_INFO "meye: using %d buffers with %dk (%dk total) for capture\n",
	       gbuffers, gbufsize/1024, gbuffers*gbufsize/1024);
	return pci_module_init(&meye_driver);
}

static void __exit meye_cleanup_module(void) {
	pci_unregister_driver(&meye_driver);
}

#ifndef MODULE
static int __init meye_setup(char *str) {
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (ints[0] <= 0) 
		goto out;
	gbuffers = ints[1];
	if (ints[0] == 1)
		goto out;
	gbufsize = ints[2];
	if (ints[0] == 2)
		goto out;
	video_nr = ints[3];
out:
	return 1;
}

__setup("meye=", meye_setup);
#endif

MODULE_AUTHOR("Stelian Pop <stelian@popies.net>");
MODULE_DESCRIPTION("video4linux driver for the MotionEye camera");
MODULE_LICENSE("GPL");

MODULE_PARM(gbuffers,"i");
MODULE_PARM_DESC(gbuffers,"number of capture buffers, default is 2 (32 max)");
MODULE_PARM(gbufsize,"i");
MODULE_PARM_DESC(gbufsize,"size of the capture buffers, default is 614400");
MODULE_PARM(video_nr,"i");
MODULE_PARM_DESC(video_nr,"video device to register (0=/dev/video0, etc)");

/* Module entry points */
module_init(meye_init_module);
module_exit(meye_cleanup_module);
