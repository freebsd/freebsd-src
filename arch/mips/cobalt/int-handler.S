/*
 * Cobalt interrupt handler
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997 by Ralf Baechle
 * Copyright (C) 2001, 2002, 2003 by Liam Davies (ldavies@agile.tv)
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/cobalt/cobalt.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

/*
 * cobalt_handle_int: Interrupt handler for Cobalt boards
 */
		.text
		.set	noreorder
		.set	noat
		.align	5
		NESTED(cobalt_handle_int, PT_SIZE, sp)
		SAVE_ALL
		CLI
		.set	at

		/*
		 * Get pending Interrupts
		 */
		mfc0	s0,CP0_CAUSE	# get raw irq status
		mfc0	a0,CP0_STATUS	# get irq mask
		and	s0,s0,a0	# compute masked irq status

		andi	a0,s0,CAUSEF_IP2	/* Check for Galileo timer */
		beq	a0,zero,1f
		 andi	a0,s0,CAUSEF_IP6	/* Check for Via chip */

		/* Galileo interrupt */
		jal	galileo_irq
		 move	a0,sp
		j	ret_from_irq
		 nop

1:
		beq	a0,zero,1f		/* Check IP6 */
		 andi	a0,s0,CAUSEF_IP3

		/* Via interrupt */
		jal	via_irq
		 move	a0,sp
		j	ret_from_irq
		 nop

1:
		beq	a0,zero,1f		/* Check IP3 */
		 andi	a0,s0,CAUSEF_IP4

		/* Ethernet 0 interrupt */
		li	a0,COBALT_ETH0_IRQ
		jal     do_IRQ
		 move	a1,sp

		j	ret_from_irq
		 nop

1:
		beq	a0,zero,1f		/* Check IP4 */
		 andi	a0,s0,CAUSEF_IP5

		/* Ethernet 1 interrupt */
		li	a0,COBALT_ETH1_IRQ
		jal     do_IRQ
		 move	a1,sp

		j	ret_from_irq
		 nop
1:
		beq	a0,zero,1f		/* Check IP5 */
		 andi	a0,s0,CAUSEF_IP7

		/* Serial interrupt */
		li	a0,COBALT_SERIAL_IRQ
		jal     do_IRQ
		 move	a1,sp

		j	ret_from_irq
		 nop
1:
		beq	a0,zero,1f		/* Check IP7 */
		  nop

		/* PCI interrupt */
		li	a0,COBALT_QUBE_SLOT_IRQ
		jal     do_IRQ
		 move	a1,sp

1:
		j	ret_from_irq
		 nop

		END(cobalt_handle_int)

