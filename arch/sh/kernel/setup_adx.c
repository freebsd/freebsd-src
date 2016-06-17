/* 
 * linux/arch/sh/kernel/setup_adx.c
 *
 * Copyright (C) 2001 A&D Co., Ltd.
 *
 * I/O routine and setup routines for A&D ADX Board
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/irq.h>
#include <linux/module.h>

void setup_adx(void)
{
	/* nothing to do just yet */
/*	printk("setup_adx()\n");*/
}

void init_adx_IRQ(void)
{
	int i;

/*	printk("init_adx_IRQ()\n");*/
	/* setup irq_mask_register */
	irq_mask_register = (unsigned short *)0xa6000008;

	/* cover all external interrupt area by maskreg_irq_type
	 * (Actually, irq15 doesn't exist)
	 */
	for (i = 0; i < 16; i++) {
		make_maskreg_irq(i);
		disable_irq(i);
	}
}
