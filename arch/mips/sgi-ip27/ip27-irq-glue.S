/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set	noat
	.align	5
NESTED(ip27_irq, PT_SIZE, sp)
	SAVE_ALL
	CLI
	.set	at

	/* IP27 may signal interrupt which we're not interested in.
	   Mask them out.  */
	mfc0	s0, CP0_CAUSE
	mfc0	t0, CP0_STATUS
	and	s0, t0

	/* First check for RT interrupt.  */
	andi	a0, s0, CAUSEF_IP4
	beqz	a0, 1f

	/* Ok, a timer interrupt. */
	move	a0, sp
	jal	rt_timer_interrupt

	j	ret_from_irq

1:	andi	a0, s0, (CAUSEF_IP2 | CAUSEF_IP3)
	beqz	a0, 1f

	/* ... a device interrupt ...  */
	move	a0, sp
	jal	ip27_do_irq

	j	ret_from_irq

1:
#if 1
	mfc0	a1, CP0_STATUS
	srl	a1, a1, 8
	andi	a1, 0xff

	mfc0	a2, CP0_CAUSE
	srl	a2, a2, 8
	andi	a2, 0xff

	move	a3, s0
	PRINT("Spurious interrupt, c0_status = %02x, c0_cause = %02x, pending %02x.\n")
	ld	a1, PT_EPC(sp)
0:	b	0b
#endif

	j	ret_from_irq
	END(ip27_irq)
