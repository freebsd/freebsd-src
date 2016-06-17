/*
 *  arch/mips/philips/nino/int-handler.S
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Interrupt exception dispatch code for Philips Nino
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	/*
	 * Here is the table of interrupts for the Philips Nino
	 * which uses the Philips PR31700/Toshiba TMPR3912 core.
	 *
	 *   MIPS IRQ             Description
	 *   --------	--------------------------------
	 *       0	SW0 interrupt (unused)
	 *       1	SW1 interrupt (unused)
	 *       2
	 *       3
	 *       4	PR31700 low priority interrupts
	 *       5
	 *       6	PR31700 high priority interrupts
	 *       7
	 */

	.text
	.set	noreorder
	.set	noat
	.align	5
	NESTED(ninoIRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI
	.set	at
	mfc0	s0, CP0_CAUSE		# determine cause

	andi	a0, s0, CAUSEF_IP6
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP4	# delay slot
	move    a0, sp
	jal	irq6_dispatch
	nop				# delay slot
	j	ret_from_irq
	nop				# delay slot

1:
	beq	a0, zero, 1f
	nop				# delay slot
	move	a0, sp
	jal	irq4_dispatch
	nop				# delay slot
	j	ret_from_irq
	nop				# delay slot

1:
	/* We should never get here */
	move	a0, sp
	j	irq_bad
	nop
	END(ninoIRQ)
