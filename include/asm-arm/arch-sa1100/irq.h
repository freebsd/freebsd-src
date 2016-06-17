/*
 * linux/include/asm-arm/arch-sa1100/irq.h
 * 
 * Author: Nicolas Pitre
 */

#define fixup_irq(x)	(x)

/*
 * This prototype is required for cascading of multiplexed interrupts.
 * Since it doesn't exist elsewhere, we'll put it here for now.
 */
extern void do_IRQ(int irq, struct pt_regs *regs);
