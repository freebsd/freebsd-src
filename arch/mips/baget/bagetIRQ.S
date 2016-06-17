/*
 * bagetIRQ.S: Interrupt exception dispatch code for Baget/MIPS
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/addrspace.h>

	.text
	.set    mips1
	.set    reorder
	.set    macro
	.set    noat
	.align	5

NESTED(bagetIRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI				# Important: mark KERNEL mode !

	la      a1, baget_interrupt
	.set	push
	.set    noreorder
	jal	a1
	.set    pop
	move	a0, sp

	la      a1, ret_from_irq
	jr	a1
END(bagetIRQ)

#define DBE_HANDLER       0x1C

NESTED(try_read, PT_SIZE, sp)
	mfc0	t3, CP0_STATUS		# save flags and
	CLI				#  disable interrupts

	li	t0, KSEG2
	sltu    t1, t0, a0              # Is it KSEG2 address ?
	beqz	t1, mapped              # No - already mapped !

	move    t0, a0
	ori	t0, 0xfff
	xori    t0, 0xfff               # round address to page

	ori     t1, t0, 0xf00           # prepare EntryLo (N,V,D,G)

	mfc0    t2,   CP0_ENTRYHI       # save ASID value
	mtc0	zero, CP0_INDEX
	mtc0	t0,   CP0_ENTRYHI       # Load MMU values ...
	mtc0    t1,   CP0_ENTRYLO0
	nop                             # let it understand
	nop
	tlbwi				# ... and write ones
	nop
	nop
	mtc0    t2,  CP0_ENTRYHI

mapped:
	la	t0, exception_handlers
	lw	t1, DBE_HANDLER(t0)	# save real handler
	la	t2, dbe_handler
	sw	t2, DBE_HANDLER(t0)	# set temporary local handler
	li	v0, -1			# default (failure) value

	li	t2, 1
	beq	t2, a1, 1f
	li	t2, 2
	beq	t2, a1, 2f
	li	t2, 4
	beq	t2, a1, 4f
	b	out

1:	lbu	v0, (a0)		# byte
	b	out

2:	lhu	v0, (a0)		# short
	b	out

4:	lw	v0, (a0)		# word

out:
	sw	t1, DBE_HANDLER(t0)	# restore real handler
	mtc0	t3, CP0_STATUS		# restore CPU flags
	jr	ra

dbe_handler:
	li	v0, -1			# mark our failure
	.set	push
	.set	noreorder
	b	out			# "no problems !"
	rfe				#   return from trap
	.set	pop
END(try_read)
