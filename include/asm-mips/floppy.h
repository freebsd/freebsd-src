/*
 * Architecture specific parts of the Floppy driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 2000 Ralf Baechle
 */
#ifndef _ASM_FLOPPY_H
#define _ASM_FLOPPY_H

struct fd_ops {
	unsigned char (*fd_inb)(unsigned int port);
	void (*fd_outb)(unsigned char value, unsigned int port);

	/*
	 * How to access the floppy DMA functions.
	 */
	void (*fd_enable_dma)(int channel);
	void (*fd_disable_dma)(int channel);
	int (*fd_request_dma)(int channel);
	void (*fd_free_dma)(int channel);
	void (*fd_clear_dma_ff)(int channel);
	void (*fd_set_dma_mode)(int channel, char mode);
	void (*fd_set_dma_addr)(int channel, unsigned int a);
	void (*fd_set_dma_count)(int channel, unsigned int count);
	int (*fd_get_dma_residue)(int channel);
	void (*fd_enable_irq)(int irq);
	void (*fd_disable_irq)(int irq);
	unsigned long (*fd_getfdaddr1)(void);
	unsigned long (*fd_dma_mem_alloc)(unsigned long size);
	void (*fd_dma_mem_free)(unsigned long addr, unsigned long size);
	unsigned long (*fd_drive_type)(unsigned long);
};

extern struct fd_ops *fd_ops;

#define fd_inb(port)		fd_ops->fd_inb(port)
#define fd_outb(value,port)	fd_ops->fd_outb(value,port)

#define fd_enable_dma()		fd_ops->fd_enable_dma(FLOPPY_DMA)
#define fd_disable_dma()	fd_ops->fd_disable_dma(FLOPPY_DMA)
#define fd_request_dma()	fd_ops->fd_request_dma(FLOPPY_DMA)
#define fd_free_dma()		fd_ops->fd_free_dma(FLOPPY_DMA)
#define fd_clear_dma_ff()	fd_ops->fd_clear_dma_ff(FLOPPY_DMA)
#define fd_set_dma_mode(mode)	fd_ops->fd_set_dma_mode(FLOPPY_DMA, mode)
#define fd_set_dma_addr(addr)	fd_ops->fd_set_dma_addr(FLOPPY_DMA, \
				                       virt_to_bus(addr))
#define fd_set_dma_count(count)	fd_ops->fd_set_dma_count(FLOPPY_DMA,count)
#define fd_get_dma_residue()	fd_ops->fd_get_dma_residue(FLOPPY_DMA)

#define fd_enable_irq()		fd_ops->fd_enable_irq(FLOPPY_IRQ)
#define fd_disable_irq()	fd_ops->fd_disable_irq(FLOPPY_IRQ)
#define fd_request_irq()	request_irq(FLOPPY_IRQ, floppy_interrupt, \
				            SA_INTERRUPT | SA_SAMPLE_RANDOM, \
				            "floppy", NULL)
#define fd_free_irq()		free_irq(FLOPPY_IRQ, NULL);
#define fd_dma_mem_alloc(size)	fd_ops->fd_dma_mem_alloc(size)
#define fd_dma_mem_free(mem,size) fd_ops->fd_dma_mem_free(mem,size)
#define fd_drive_type(n)	fd_ops->fd_drive_type(n)
#define fd_cacheflush(addr,size) dma_cache_wback_inv(addr,size)

#define MAX_BUFFER_SECTORS 24


/*
 * And on Mips's the CMOS info fails also ...
 *
 * FIXME: This information should come from the ARC configuration tree
 *        or whereever a particular machine has stored this ...
 */
#define FLOPPY0_TYPE 			fd_drive_type(0)
#define FLOPPY1_TYPE			fd_drive_type(1)

#define FDC1			fd_ops->fd_getfdaddr1();

#define N_FDC 1			/* do you *really* want a second controller? */
#define N_DRIVE 8

#define FLOPPY_MOTOR_MASK 0xf0

/*
 * The DMA channel used by the floppy controller cannot access data at
 * addresses >= 16MB
 *
 * Went back to the 1MB limit, as some people had problems with the floppy
 * driver otherwise. It doesn't matter much for performance anyway, as most
 * floppy accesses go through the track buffer.
 *
 * On MIPSes using vdma, this actually means that *all* transfers go thru
 * the * track buffer since 0x1000000 is always smaller than KSEG0/1.
 * Actually this needs to be a bit more complicated since the so much different
 * hardware available with MIPS CPUs ...
 */
#define CROSS_64KB(a,s) ((unsigned long)(a)/K_64 != ((unsigned long)(a) + (s) - 1) / K_64)

#define EXTRA_FLOPPY_PARAMS

#endif /* _ASM_FLOPPY_H */
