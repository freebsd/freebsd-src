/* $FreeBSD$ */
/* $NetBSD: pci_eb164_intr.s,v 1.5 1997/09/02 13:19:42 thorpej Exp $ */

/*-
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * The description of how to enable and disable interrupts in the
 * AlphaPC 164 motherboard technical reference manual is incorrect,
 * at least for the OSF/1 PALcode.
 *
 * These functions were written by disassembling a Digital UNIX kernel's
 * eb164_intrdsabl and eb164_intrenabl functions (because they had
 * interesting names), and then playing with them to see how to call
 * them correctly.
 *
 * It looks like the right thing to do is to call them with the interrupt
 * request that you want to enable or disable (presumably in the range
 * 0 -> 23, since there are 3 8-bit interrupt-enable bits in the
 * interrupt mask PLD).
 */

#include <machine/asm.h>

__KERNEL_RCSID(0, "$NetBSD: pci_eb164_intr.s,v 1.5 1997/09/02 13:19:42 thorpej Exp $");

	.text
LEAF(eb164_intr_enable,1)
	mov	a0, a1
	ldiq	a0, 0x34
	call_pal PAL_cserve
	RET
	END(eb164_intr_enable)

	.text
LEAF(eb164_intr_disable,1)
	mov	a0, a1
	ldiq	a0, 0x35
	call_pal PAL_cserve
	RET
	END(eb164_intr_disable)

	.text
LEAF(eb164_intr_enable_icsr,1)
	mov	a0, a1
	ldiq	a0, 0x34
	call_pal PAL_cserve
	ldiq	a0, 0x08	/* Allow PALRES */
	call_pal PAL_cserve
	.long	0x66100118	/* hw_mfpr a0, icsr */
	ldah	a1, 0x0020	/* IMSK1 */
	or	a0, a1, a0
	xor	a0, a1, a0
	.long	0x76100118	/* hw_mtpr a0, icsr */
	ldiq	a0, 0x09	/* Disable PALRES */
	call_pal PAL_cserve
	RET
	END(eb164_intr_enable_icsr)

	.text
LEAF(eb164_intr_disable_icsr,1)
	ldiq	a0, 0x08	/* Allow PALRES */
	call_pal PAL_cserve
	.long	0x66100118	/* hw_mfpr a0, icsr */
	ldah	a1, 0x0020	/* IMSK1 */
	or	a0, a1, a0
	.long	0x76100118	/* hw_mtpr a0, icsr */
	ldiq	a0, 0x09	/* Disable PALRES */
	call_pal PAL_cserve
	RET
	END(eb164_intr_disable_icsr)
