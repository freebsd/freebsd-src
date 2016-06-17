/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2001 Ralf Baechle
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/jazz.h>

extern asmlinkage void jazz_handle_int(void);

/*
 * On systems with i8259-style interrupt controllers we assume for
 * driver compatibility reasons interrupts 0 - 15 to be the i8259
 * interrupts even if the hardware uses a different interrupt numbering.
 */
void __init init_IRQ (void)
{
	int i;

	set_except_vector(0, jazz_handle_int);

	init_generic_irq();
	init_i8259_irqs();			/* Integrated i8259  */
#if 0
	init_jazz_irq();

	/* Actually we've got more interrupts to handle ...  */
	for (i = PCIMT_IRQ_INT2; i <= PCIMT_IRQ_ETHERNET; i++) {
		irq_desc[i].status     = IRQ_DISABLED;
		irq_desc[i].action     = 0;
		irq_desc[i].depth      = 1;
		irq_desc[i].handler    = &pciasic_irq_type;
	}
#endif
}
