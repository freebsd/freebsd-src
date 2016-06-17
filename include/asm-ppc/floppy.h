/*
 * Architecture specific parts of the Floppy driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995
 */
#ifdef __KERNEL__
#ifndef __ASM_PPC_FLOPPY_H
#define __ASM_PPC_FLOPPY_H

#define fd_inb(port)			inb_p(port)
#define fd_outb(value,port)		outb_p(value,port)

#define fd_enable_dma()         enable_dma(FLOPPY_DMA)
#define fd_disable_dma()        disable_dma(FLOPPY_DMA)
#define fd_request_dma()        request_dma(FLOPPY_DMA,"floppy")
#define fd_free_dma()           free_dma(FLOPPY_DMA)
#define fd_clear_dma_ff()       clear_dma_ff(FLOPPY_DMA)
#define fd_set_dma_mode(mode)   set_dma_mode(FLOPPY_DMA,mode)
#define fd_set_dma_addr(addr)   set_dma_addr(FLOPPY_DMA,(unsigned int)virt_to_bus(addr))
#define fd_set_dma_count(count) set_dma_count(FLOPPY_DMA,count)
#define fd_enable_irq()         enable_irq(FLOPPY_IRQ)
#define fd_disable_irq()        disable_irq(FLOPPY_IRQ)
#if CONFIG_NOT_COHERENT_CACHE
#define fd_cacheflush(addr,size) dma_cache_wback_inv(addr, size)
#else
#define fd_cacheflush(addr,size) /* nothing */
#endif
#define fd_request_irq()        request_irq(FLOPPY_IRQ, floppy_interrupt, \
					    SA_INTERRUPT|SA_SAMPLE_RANDOM, \
				            "floppy", NULL)
#define fd_free_irq()           free_irq(FLOPPY_IRQ, NULL);

static int FDC1 = 0x3f0;
static int FDC2 = -1;

/*
 * Again, the CMOS information not available
 */
#define FLOPPY0_TYPE 6
#define FLOPPY1_TYPE 0

#define N_FDC 2			/* Don't change this! */
#define N_DRIVE 8

#define FLOPPY_MOTOR_MASK 0xf0

/*
 * The PowerPC has no problems with floppy DMA crossing 64k borders.
 */
#define CROSS_64KB(a,s)	(0)

#endif /* __ASM_PPC_FLOPPY_H */

#define EXTRA_FLOPPY_PARAMS

#endif /* __KERNEL__ */
