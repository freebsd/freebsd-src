/*
 * linux/include/asm-sh/irq_microdev.h
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * IRQ functions for the SuperH SH4-202 MicroDev board.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */


#ifndef _ASM_SH_IRQ_MICRODEV_H
#define _ASM_SH_IRQ_MICRODEV_H

extern void __init init_microdev_irq(void);


	/*
	 *	The following are useful macros for manipulating the
	 *	interrupt controller (INTC) on the CPU-board FPGA.
	 *	It should be noted that there is an INTC on the FPGA,
	 *	and a seperate INTC on the SH4-202 core - these are
	 *	two different things, both of which need to be prorammed
	 *	to correctly route - unfortunately, they have the
	 *	same name and abbreviations!
	 */
#define	MICRODEV_FPGA_INTC_BASE		0xa6110000ul				/* INTC base address on CPU-board FPGA */
#define	MICRODEV_FPGA_INTENB_REG	(MICRODEV_FPGA_INTC_BASE+0ul)		/* Interrupt Enable Register on INTC on CPU-board FPGA */
#define	MICRODEV_FPGA_INTDSB_REG	(MICRODEV_FPGA_INTC_BASE+8ul)		/* Interrupt Disable Register on INTC on CPU-board FPGA */
#define	MICRODEV_FPGA_INTC_MASK(n)	(1ul<<(n))				/* Interupt mask to enable/disable INTC in CPU-board FPGA */
#define	MICRODEV_FPGA_INTPRI_REG(n)	(MICRODEV_FPGA_INTC_BASE+0x10+((n)/8)*8)/* Interrupt Priority Register on INTC on CPU-board FPGA */
#define	MICRODEV_FPGA_INTPRI_LEVEL(n,x)	((x)<<(((n)%8)*4))			/* MICRODEV_FPGA_INTPRI_LEVEL(int_number, int_level) */
#define	MICRODEV_FPGA_INTPRI_MASK(n)	(MICRODEV_FPGA_INTPRI_LEVEL((n),0xful))	/* Interrupt Priority Mask on INTC on CPU-board FPGA */


#endif /* _ASM_SH_IRQ_MICRODEV_H */
