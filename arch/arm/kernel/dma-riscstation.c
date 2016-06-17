/*
 *  linux/arch/arm/kernel/dma-riscstation.c
 *
 *  Copyright (C) 1998 Russell King
 *  Copyright (C) 2002 Ben Dooks / Simtec Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  DMA functions specific to RiscStatiun architecture
 *
 * sliced down version of the RiscPC DMA code by RMK
 */
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/page.h>
#include <asm/dma.h>
#include <asm/fiq.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

#include <asm/mach/dma.h>
#include <asm/hardware/iomd.h>

static struct fiq_handler fh = {
	name: "floppydma"
};

static void floppy_enable_dma(dmach_t channel, dma_t *dma)
{
	void *fiqhandler_start;
	unsigned int fiqhandler_length;
	struct pt_regs regs;

	if (dma->dma_mode == DMA_MODE_READ) {
		extern unsigned char floppy_fiqin_start, floppy_fiqin_end;
		fiqhandler_start = &floppy_fiqin_start;
		fiqhandler_length = &floppy_fiqin_end - &floppy_fiqin_start;
	} else {
		extern unsigned char floppy_fiqout_start, floppy_fiqout_end;
		fiqhandler_start = &floppy_fiqout_start;
		fiqhandler_length = &floppy_fiqout_end - &floppy_fiqout_start;
	}

	regs.ARM_r9  = dma->buf.length;
	regs.ARM_r10 = (unsigned long)dma->buf.address;
	regs.ARM_fp  = FLOPPYDMA_BASE;

	if (claim_fiq(&fh)) {
		printk("floppydma: couldn't claim FIQ.\n");
		return;
	}

	set_fiq_handler(fiqhandler_start, fiqhandler_length);
	set_fiq_regs(&regs);
	enable_fiq(dma->dma_irq);
}

static void floppy_disable_dma(dmach_t channel, dma_t *dma)
{
	disable_fiq(dma->dma_irq);
	release_fiq(&fh);
}

static int floppy_get_residue(dmach_t channel, dma_t *dma)
{
	struct pt_regs regs;
	get_fiq_regs(&regs);
	return regs.ARM_r9;
}

static struct dma_ops floppy_dma_ops = {
	type:		"FIQDMA",
	enable:		floppy_enable_dma,
	disable:	floppy_disable_dma,
	residue:	floppy_get_residue,
};

/*
 * This is virtual DMA - we don't need anything here.
 */
static void sound_enable_disable_dma(dmach_t channel, dma_t *dma)
{
}

static struct dma_ops sound_dma_ops = {
	type:		"VIRTUAL",
	enable:		sound_enable_disable_dma,
	disable:	sound_enable_disable_dma,
};

void __init arch_dma_init(dma_t *dma)
{
#if 0
	iomd_writeb(0, IOMD_IO0CR);
	iomd_writeb(0, IOMD_IO1CR);
	iomd_writeb(0, IOMD_IO2CR);
	iomd_writeb(0, IOMD_IO3CR);

	iomd_writeb(0xa0, IOMD_DMATCR);
#endif

	dma[DMA_VIRTUAL_FLOPPY].dma_irq	= FIQ_FLOPPYDATA;
	dma[DMA_VIRTUAL_FLOPPY].d_ops	= &floppy_dma_ops;
	dma[DMA_VIRTUAL_SOUND].d_ops	= &sound_dma_ops;
}
