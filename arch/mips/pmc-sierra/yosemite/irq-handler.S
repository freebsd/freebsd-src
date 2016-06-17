/*
 * Copyright 2003 PMC-Sierra Inc.
 * Author: Manish Lachwani (lachwani@pmc-sierra.com
 *
 * First-level interrupt router for the PMC-Sierra Titan board
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define __ASSEMBLY__
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

/*
 * IRQ router for the Titan board
 */

		.align	5
		NESTED(titan_handle_int, PT_SIZE, sp)
		SAVE_ALL
		CLI
		.set	at
		mfc0	t0, CP0_CAUSE
		mfc0	t2, CP0_STATUS

		and	t0, t2
	
		andi	t1, t0, STATUSF_IP0	/* INTB0 hardware line */
		bnez	t1, ll_pcia_irq		/* 64-bit PCI */
		andi	t1, t0, STATUSF_IP1	/* INTB1 hardware line */
		bnez	t1, ll_pcib_irq		/* second 64-bit PCI slot */
		andi	t1, t0, STATUSF_IP2	/* INTB2 hardware line */
		bnez	t1, ll_duart_irq	/* UART	*/
		andi	t1, t0, STATUSF_IP3	/* INTB3 hardware line*/
		bnez	t1, ll_ht_smp_irq		/* Hypertransport */
		andi	t1, t0, STATUSF_IP5	/* INTB5 hardware line */
		bnez	t1, ll_timer_irq	/* Timer */

		nop
		nop

		/* Extended interrupts */
		mfc0	t0, CPU_CAUSE
		cfc0	t1, CP0_S1_INTCONTROL

		sll	t2, t1, 8
		
		and	t0, t2
		srl	t0, t0, 16

		
		andi	t1, t0, STATUSF_IP6	/* INTB6 hardware line */
		bnez	t1, ll_phy0_irq		/* Ethernet port 0 */
		andi	t1, t0, STATUSF_IP7	/* INTB7 hardware line */
		bnez	t1, ll_phy1_irq		/* Ethernet port 1 */
		andi	t1, t0, STATUSF_IP8	/* INTB8 hardware line */
		bnez	t1, ll_phy2_irq		/* Ethernet Port 2 */

		nop
		nop

		.set	reorder

		/* No handler */
		j	spurious_interrupt
		nop
		END(titan_handle_int)

		.align	5

/* Individual Handlers */

ll_pcia_irq:
		li	a0, 1
		move	a2, sp
		jal	do_IRQ
		j	ret_from_irq

ll_pcib_irq:
		li      a0, 2
                move    a1, sp
                jal     do_IRQ
                j       ret_from_irq

ll_duart_irq:
		li	a0, 3
		move	a1, sp
		jal	do_IRQ
		j	ret_from_irq

ll_ht_irq:
		li	a0, 4
		move	a1, sp
		jal	ll_ht_smp_irq_handler 	/* Detailed HT & SMP IRQ handling */
		j	ret_from_irq

ll_timer_irq:
		li	a0, 5
		move	a1, sp
		jal	do_IRQ
		j	ret_from_irq

ll_phy0_irq:
		li	a0, 6
		move	a1, sp
		jal	do_IRQ
		j	ret_from_irq	

ll_phy1_irq:
		li	a0, 7
		move	a1, sp
		jal	do_IRQ
		j	ret_from_irq

ll_phy2_irq:
		li	a0, 8
		move	a1, sp
		jal	do_IRQ
		j	ret_from_irq


