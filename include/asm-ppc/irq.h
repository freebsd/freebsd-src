#ifdef __KERNEL__
#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#include <linux/config.h>
#include <asm/machdep.h>		/* ppc_md */
#include <asm/atomic.h>

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

/*
 * These constants are used for passing information about interrupt
 * signal polarity and level/edge sensing to the low-level PIC chip
 * drivers.
 */
#define IRQ_SENSE_MASK		0x1
#define IRQ_SENSE_LEVEL		0x1	/* interrupt on active level */
#define IRQ_SENSE_EDGE		0x0	/* interrupt triggered by edge */

#define IRQ_POLARITY_MASK	0x2
#define IRQ_POLARITY_POSITIVE	0x2	/* high level or low->high edge */
#define IRQ_POLARITY_NEGATIVE	0x0	/* low level or high->low edge */

#if defined(CONFIG_40x)
#include <asm/ibm4xx.h>

#ifndef NR_BOARD_IRQS
#define NR_BOARD_IRQS 0
#endif

#ifndef UIC_WIDTH /* Number of interrupts per device */
#define UIC_WIDTH 32
#endif

#ifndef NR_UICS /* number  of UIC devices */
#define NR_UICS 1
#endif

#if defined (CONFIG_403)
/*
 * The PowerPC 403 cores' Asynchronous Interrupt Controller (AIC) has
 * 32 possible interrupts, a majority of which are not implemented on
 * all cores. There are six configurable, external interrupt pins and
 * there are eight internal interrupts for the on-chip serial port
 * (SPU), DMA controller, and JTAG controller.
 *
 */

#define	NR_AIC_IRQS 32
#define	NR_IRQS	 (NR_AIC_IRQS + NR_BOARD_IRQS)

#elif !defined (CONFIG_403)

/*
 *  The PowerPC 405 cores' Universal Interrupt Controller (UIC) has 32
 * possible interrupts as well. There are seven, configurable external
 * interrupt pins and there are 17 internal interrupts for the on-chip
 * serial port, DMA controller, on-chip Ethernet controller, PCI, etc.
 *
 */


#define NR_UIC_IRQS UIC_WIDTH
#define NR_IRQS		((NR_UIC_IRQS * NR_UICS) + NR_BOARD_IRQS)
#endif
static __inline__ int
irq_cannonicalize(int irq)
{
	return (irq);
}

#elif defined(CONFIG_44x)
#include <asm/ibm44x.h>

#define	NR_UIC_IRQS	32
#define	NR_IRQS		((NR_UIC_IRQS * NR_UICS) + NR_BOARD_IRQS)

static __inline__ int
irq_cannonicalize(int irq)
{
	return (irq);
}

#elif defined(CONFIG_8xx)

/* Now include the board configuration specific associations.
*/
#include <asm/mpc8xx.h>

/* The MPC8xx cores have 16 possible interrupts.  There are eight
 * possible level sensitive interrupts assigned and generated internally
 * from such devices as CPM, PCMCIA, RTC, PIT, TimeBase and Decrementer.
 * There are eight external interrupts (IRQs) that can be configured
 * as either level or edge sensitive.
 *
 * On some implementations, there is also the possibility of an 8259
 * through the PCI and PCI-ISA bridges.
 *
 * We are "flattening" the interrupt vectors of the cascaded CPM
 * and 8259 interrupt controllers so that we can uniquely identify
 * any interrupt source with a single integer.
 */
#define NR_SIU_INTS	16
#define NR_CPM_INTS	32
#ifndef NR_8259_INTS
#define NR_8259_INTS 0
#endif

#define SIU_IRQ_OFFSET		0
#define CPM_IRQ_OFFSET		(SIU_IRQ_OFFSET + NR_SIU_INTS)
#define I8259_IRQ_OFFSET	(CPM_IRQ_OFFSET + NR_CPM_INTS)

#define NR_IRQS	(NR_SIU_INTS + NR_CPM_INTS + NR_8259_INTS)

/* These values must be zero-based and map 1:1 with the SIU configuration.
 * They are used throughout the 8xx I/O subsystem to generate
 * interrupt masks, flags, and other control patterns.  This is why the
 * current kernel assumption of the 8259 as the base controller is such
 * a pain in the butt.
 */
#define	SIU_IRQ0	(0)	/* Highest priority */
#define	SIU_LEVEL0	(1)
#define	SIU_IRQ1	(2)
#define	SIU_LEVEL1	(3)
#define	SIU_IRQ2	(4)
#define	SIU_LEVEL2	(5)
#define	SIU_IRQ3	(6)
#define	SIU_LEVEL3	(7)
#define	SIU_IRQ4	(8)
#define	SIU_LEVEL4	(9)
#define	SIU_IRQ5	(10)
#define	SIU_LEVEL5	(11)
#define	SIU_IRQ6	(12)
#define	SIU_LEVEL6	(13)
#define	SIU_IRQ7	(14)
#define	SIU_LEVEL7	(15)

/* The internal interrupts we can configure as we see fit.
 * My personal preference is CPM at level 2, which puts it above the
 * MBX PCI/ISA/IDE interrupts.
 */
#ifndef PIT_INTERRUPT
#define PIT_INTERRUPT		SIU_LEVEL0
#endif
#ifndef	CPM_INTERRUPT
#define CPM_INTERRUPT		SIU_LEVEL2
#endif
#ifndef	PCMCIA_INTERRUPT
#define PCMCIA_INTERRUPT	SIU_LEVEL6
#endif
#ifndef	DEC_INTERRUPT
#define DEC_INTERRUPT		SIU_LEVEL7
#endif

/* Some internal interrupt registers use an 8-bit mask for the interrupt
 * level instead of a number.
 */
#define	mk_int_int_mask(IL) (1 << (7 - (IL/2)))

/* always the same on 8xx -- Cort */
static __inline__ int irq_cannonicalize(int irq)
{
	return irq;
}

#else /* CONFIG_40x + CONFIG_8xx */
/*
 * this is the # irq's for all ppc arch's (pmac/chrp/prep)
 * so it is the max of them all
 */
#define NR_IRQS			256

#ifndef CONFIG_8260

#define NUM_8259_INTERRUPTS	16

#else /* CONFIG_8260 */

/* The 8260 has an internal interrupt controller with a maximum of
 * 64 IRQs.  We will use NR_IRQs from above since it is large enough.
 * Don't be confused by the 8260 documentation where they list an
 * "interrupt number" and "interrupt vector".  We are only interested
 * in the interrupt vector.  There are "reserved" holes where the
 * vector number increases, but the interrupt number in the table does not.
 * (Document errata updates have fixed this...make sure you have up to
 * date processor documentation -- Dan).
 */
#define NR_SIU_INTS	64

#define	SIU_INT_ERROR		((uint)0x00)
#define	SIU_INT_I2C		((uint)0x01)
#define	SIU_INT_SPI		((uint)0x02)
#define	SIU_INT_RISC		((uint)0x03)
#define	SIU_INT_SMC1		((uint)0x04)
#define	SIU_INT_SMC2		((uint)0x05)
#define	SIU_INT_IDMA1		((uint)0x06)
#define	SIU_INT_IDMA2		((uint)0x07)
#define	SIU_INT_IDMA3		((uint)0x08)
#define	SIU_INT_IDMA4		((uint)0x09)
#define	SIU_INT_SDMA		((uint)0x0a)
#define	SIU_INT_TIMER1		((uint)0x0c)
#define	SIU_INT_TIMER2		((uint)0x0d)
#define	SIU_INT_TIMER3		((uint)0x0e)
#define	SIU_INT_TIMER4		((uint)0x0f)
#define	SIU_INT_TMCNT		((uint)0x10)
#define	SIU_INT_PIT		((uint)0x11)
#define	SIU_INT_IRQ1		((uint)0x13)
#define	SIU_INT_IRQ2		((uint)0x14)
#define	SIU_INT_IRQ3		((uint)0x15)
#define	SIU_INT_IRQ4		((uint)0x16)
#define	SIU_INT_IRQ5		((uint)0x17)
#define	SIU_INT_IRQ6		((uint)0x18)
#define	SIU_INT_IRQ7		((uint)0x19)
#define	SIU_INT_FCC1		((uint)0x20)
#define	SIU_INT_FCC2		((uint)0x21)
#define	SIU_INT_FCC3		((uint)0x22)
#define	SIU_INT_MCC1		((uint)0x24)
#define	SIU_INT_MCC2		((uint)0x25)
#define	SIU_INT_SCC1		((uint)0x28)
#define	SIU_INT_SCC2		((uint)0x29)
#define	SIU_INT_SCC3		((uint)0x2a)
#define	SIU_INT_SCC4		((uint)0x2b)
#define	SIU_INT_PC15		((uint)0x30)
#define	SIU_INT_PC14		((uint)0x31)
#define	SIU_INT_PC13		((uint)0x32)
#define	SIU_INT_PC12		((uint)0x33)
#define	SIU_INT_PC11		((uint)0x34)
#define	SIU_INT_PC10		((uint)0x35)
#define	SIU_INT_PC9		((uint)0x36)
#define	SIU_INT_PC8		((uint)0x37)
#define	SIU_INT_PC7		((uint)0x38)
#define	SIU_INT_PC6		((uint)0x39)
#define	SIU_INT_PC5		((uint)0x3a)
#define	SIU_INT_PC4		((uint)0x3b)
#define	SIU_INT_PC3		((uint)0x3c)
#define	SIU_INT_PC2		((uint)0x3d)
#define	SIU_INT_PC1		((uint)0x3e)
#define	SIU_INT_PC0		((uint)0x3f)

#endif /* CONFIG_8260 */

/*
 * This gets called from serial.c, which is now used on
 * powermacs as well as prep/chrp boxes.
 * Prep and chrp both have cascaded 8259 PICs.
 */
static __inline__ int irq_cannonicalize(int irq)
{
	if (ppc_md.irq_cannonicalize)
		return ppc_md.irq_cannonicalize(irq);
	return irq;
}

#endif

#define NR_MASK_WORDS	((NR_IRQS + 31) / 32)
/* pedantic: these are long because they are used with set_bit --RR */
extern unsigned long ppc_cached_irq_mask[NR_MASK_WORDS];
extern unsigned long ppc_lost_interrupts[NR_MASK_WORDS];
extern atomic_t ppc_n_lost_interrupts;

#endif /* _ASM_IRQ_H */
#endif /* __KERNEL__ */
