/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Access the floppy hardware on PC style hardware
 *
 * Copyright (C) 1996, 1997, 1998 by Ralf Baechle
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/bootinfo.h>
#include <asm/cachectl.h>
#include <asm/dma.h>
#include <asm/floppy.h>
#include <asm/keyboard.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mc146818rtc.h>
#include <asm/pgtable.h>

/*
 * How to access the FDC's registers.
 */
static unsigned char std_fd_inb(unsigned int port)
{
	return inb_p(port);
}

static void std_fd_outb(unsigned char value, unsigned int port)
{
	outb_p(value, port);
}

/*
 * How to access the floppy DMA functions.
 */
static void std_fd_enable_dma(int channel)
{
	enable_dma(channel);
}

static void std_fd_disable_dma(int channel)
{
	disable_dma(channel);
}

static int std_fd_request_dma(int channel)
{
	return request_dma(channel, "floppy");
}

static void std_fd_free_dma(int channel)
{
	free_dma(channel);
}

static void std_fd_clear_dma_ff(int channel)
{
	clear_dma_ff(channel);
}

static void std_fd_set_dma_mode(int channel, char mode)
{
	set_dma_mode(channel, mode);
}

static void std_fd_set_dma_addr(int channel, unsigned int addr)
{
	set_dma_addr(channel, addr);
}

static void std_fd_set_dma_count(int channel, unsigned int count)
{
	set_dma_count(channel, count);
}

static int std_fd_get_dma_residue(int channel)
{
	return get_dma_residue(channel);
}

static void std_fd_enable_irq(int irq)
{
	enable_irq(irq);
}

static void std_fd_disable_irq(int irq)
{
	disable_irq(irq);
}

static unsigned long std_fd_getfdaddr1(void)
{
	return 0x3f0;
}

static unsigned long std_fd_dma_mem_alloc(unsigned long size)
{
	unsigned long mem;

	mem = __get_dma_pages(GFP_KERNEL,get_order(size));

	return mem;
}

static void std_fd_dma_mem_free(unsigned long addr, unsigned long size)
{
	free_pages(addr, get_order(size));
}

static unsigned long std_fd_drive_type(unsigned long n)
{
	if (n == 0)
		return 4;	/* 3,5", 1.44mb */

	return 0;
}

struct fd_ops std_fd_ops = {
	/*
	 * How to access the floppy controller's ports
	 */
	std_fd_inb,
	std_fd_outb,
	/*
	 * How to access the floppy DMA functions.
	 */
	std_fd_enable_dma,
	std_fd_disable_dma,
	std_fd_request_dma,
	std_fd_free_dma,
	std_fd_clear_dma_ff,
	std_fd_set_dma_mode,
	std_fd_set_dma_addr,
	std_fd_set_dma_count,
	std_fd_get_dma_residue,
	std_fd_enable_irq,
	std_fd_disable_irq,
        std_fd_getfdaddr1,
        std_fd_dma_mem_alloc,
        std_fd_dma_mem_free,
	std_fd_drive_type
};
