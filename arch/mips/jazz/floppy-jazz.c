/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Low-level floppy stuff for Jazz family machines.
 *
 * Copyright (C) 1998 by Ralf Baechle
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/addrspace.h>
#include <asm/jazz.h>
#include <asm/jazzdma.h>
#include <asm/keyboard.h>
#include <asm/pgtable.h>
#include <asm/floppy.h>

static unsigned char jazz_fd_inb(unsigned int port)
{
	unsigned char c;

	c = *(volatile unsigned char *) port;
	udelay(1);

	return c;
}

static void jazz_fd_outb(unsigned char value, unsigned int port)
{
	*(volatile unsigned char *) port = value;
}

/*
 * How to access the floppy DMA functions.
 */
static void jazz_fd_enable_dma(int channel)
{
	vdma_enable(JAZZ_FLOPPY_DMA);
}

static void jazz_fd_disable_dma(int channel)
{
	vdma_disable(JAZZ_FLOPPY_DMA);
}

static int jazz_fd_request_dma(int channel)
{
	return 0;
}

static void jazz_fd_free_dma(int channel)
{
}

static void jazz_fd_clear_dma_ff(int channel)
{
}

static void jazz_fd_set_dma_mode(int channel, char mode)
{
	vdma_set_mode(JAZZ_FLOPPY_DMA, mode);
}

static void jazz_fd_set_dma_addr(int channel, unsigned int a)
{
	vdma_set_addr(JAZZ_FLOPPY_DMA, vdma_phys2log(PHYSADDR(a)));
}

static void jazz_fd_set_dma_count(int channel, unsigned int count)
{
	vdma_set_count(JAZZ_FLOPPY_DMA, count);
}

static int jazz_fd_get_dma_residue(int channel)
{
	return vdma_get_residue(JAZZ_FLOPPY_DMA);
}

static void jazz_fd_enable_irq(int irq)
{
}

static void jazz_fd_disable_irq(int irq)
{
}

static unsigned long jazz_fd_getfdaddr1(void)
{
	return JAZZ_FDC_BASE;
}

static unsigned long jazz_fd_dma_mem_alloc(unsigned long size)
{
	unsigned long mem;

	mem = __get_dma_pages(GFP_KERNEL, get_order(size));
	if(!mem)
		return 0;
	vdma_alloc(PHYSADDR(mem), size);	/* XXX error checking */

	return mem;
}

static void jazz_fd_dma_mem_free(unsigned long addr,
                                        unsigned long size)
{
	vdma_free(vdma_phys2log(PHYSADDR(addr)));
	free_pages(addr, get_order(size));
}

static unsigned long jazz_fd_drive_type(unsigned long n)
{
	/* XXX This is wrong for machines with ED 2.88mb disk drives like the
	   Olivetti M700.  Anyway, we should suck this from the ARC
	   firmware.  */
	if (n == 0)
		return 4;	/* 3,5", 1.44mb */

	return 0;
}

struct fd_ops jazz_fd_ops = {
	/*
	 * How to access the floppy controller's ports
	 */
	jazz_fd_inb,
	jazz_fd_outb,
	/*
	 * How to access the floppy DMA functions.
	 */
	jazz_fd_enable_dma,
	jazz_fd_disable_dma,
	jazz_fd_request_dma,
	jazz_fd_free_dma,
	jazz_fd_clear_dma_ff,
	jazz_fd_set_dma_mode,
	jazz_fd_set_dma_addr,
	jazz_fd_set_dma_count,
	jazz_fd_get_dma_residue,
	jazz_fd_enable_irq,
	jazz_fd_disable_irq,
	jazz_fd_getfdaddr1,
	jazz_fd_dma_mem_alloc,
	jazz_fd_dma_mem_free,
	jazz_fd_drive_type
};
