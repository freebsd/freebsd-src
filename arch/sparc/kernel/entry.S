/* $Id: entry.S,v 1.170 2001/11/13 00:57:05 davem Exp $
 * arch/sparc/kernel/entry.S:  Sparc trap low-level entry points.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996 Eddie C. Dost   (ecd@skynet.be)
 * Copyright (C) 1996 Miguel de Icaza (miguel@nuclecu.unam.mx)
 * Copyright (C) 1996-1999 Jakub Jelinek   (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997 Anton Blanchard (anton@progsoc.uts.edu.au)
 */

#include <linux/config.h>
#include <linux/errno.h>

#include <asm/head.h>
#include <asm/asi.h>
#include <asm/smp.h>
#include <asm/kgdb.h>
#include <asm/contregs.h>
#include <asm/ptrace.h>
#include <asm/psr.h>
#include <asm/cprefix.h>
#include <asm/vaddrs.h>
#include <asm/memreg.h>
#include <asm/page.h>
#ifdef CONFIG_SUN4
#include <asm/pgtsun4.h>
#else
#include <asm/pgtsun4c.h>
#endif
#include <asm/winmacro.h>
#include <asm/signal.h>
#include <asm/obio.h>
#include <asm/mxcc.h>

#include <asm/asmmacro.h>

#define curptr      g6

#define NR_SYSCALLS 256      /* Each OS is different... */

/* These are just handy. */
#define _SV	save	%sp, -STACKFRAME_SZ, %sp
#define _RS     restore 

#define FLUSH_ALL_KERNEL_WINDOWS \
	_SV; _SV; _SV; _SV; _SV; _SV; _SV; \
	_RS; _RS; _RS; _RS; _RS; _RS; _RS;

/* First, KGDB low level things.  This is a rewrite
 * of the routines found in the sparc-stub.c asm() statement
 * from the gdb distribution.  This is also dual-purpose
 * as a software trap for userlevel programs.
 */
	.data
	.align	4

in_trap_handler:
	.word	0

	.text
	.align	4

! This function is called when any SPARC trap (except window overflow or
! underflow) occurs.  It makes sure that the invalid register window is still
! available before jumping into C code.  It will also restore the world if you
! return from handle_exception.

	.globl	C_LABEL(trap_low)
C_LABEL(trap_low):
	rd	%wim, %l3
	SAVE_ALL

	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	inc	%l5
	st	%l5, [%lo(in_trap_handler) + %l4]

	/* Make sure kgdb sees the same state we just saved. */
	LOAD_PT_GLOBALS(sp)
	LOAD_PT_INS(sp)
	ld	[%sp + STACKFRAME_SZ + PT_Y], %l4
	ld	[%sp + STACKFRAME_SZ + PT_WIM], %l3
	ld	[%sp + STACKFRAME_SZ + PT_PSR], %l0
	ld	[%sp + STACKFRAME_SZ + PT_PC], %l1
	ld	[%sp + STACKFRAME_SZ + PT_NPC], %l2
	rd	%tbr, %l5	/* Never changes... */

	/* Make kgdb exception frame. */	
	sub	%sp,(16+1+6+1+72)*4,%sp	! Make room for input & locals
 					! + hidden arg + arg spill
					! + doubleword alignment
					! + registers[72] local var
	SAVE_KGDB_GLOBALS(sp)
	SAVE_KGDB_INS(sp)
	SAVE_KGDB_SREGS(sp, l4, l0, l3, l5, l1, l2)

	/* We are increasing PIL, so two writes. */
	or	%l0, PSR_PIL, %l0
	wr	%l0, 0, %psr
	WRITE_PAUSE
	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	call	C_LABEL(handle_exception)
	 add	%sp, STACKFRAME_SZ, %o0	! Pass address of registers

	/* Load new kgdb register set. */
	LOAD_KGDB_GLOBALS(sp)
	LOAD_KGDB_INS(sp)
	LOAD_KGDB_SREGS(sp, l4, l0, l3, l5, l1, l2)
	wr      %l4, 0x0, %y

	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	dec	%l5
	st	%l5, [%lo(in_trap_handler) + %l4]

	add	%sp,(16+1+6+1+72)*4,%sp	! Undo the kgdb trap frame.

	/* Now take what kgdb did and place it into the pt_regs
	 * frame which SparcLinux RESTORE_ALL understands.,
	 */
	STORE_PT_INS(sp)
	STORE_PT_GLOBALS(sp)
	STORE_PT_YREG(sp, g2)
	STORE_PT_PRIV(sp, l0, l1, l2)

	RESTORE_ALL


#ifdef CONFIG_BLK_DEV_FD
	.text
	.align	4
	.globl	C_LABEL(floppy_hardint)
C_LABEL(floppy_hardint):
	/*
	 * This code cannot touch registers %l0 %l1 and %l2
	 * because SAVE_ALL depends on their values. It depends
	 * on %l3 also, but we regenerate it before a call.
	 * Other registers are:
	 * %l3 -- base address of fdc registers
	 * %l4 -- pdma_vaddr
	 * %l5 -- scratch for ld/st address
	 * %l6 -- pdma_size
	 * %l7 -- scratch [floppy byte, ld/st address, aux. data]
	 */

	/* Do we have work to do? */
	sethi	%hi(C_LABEL(doing_pdma)), %l7
	ld	[%l7 + %lo(C_LABEL(doing_pdma))], %l7
	cmp	%l7, 0
	be	floppy_dosoftint
	 nop

	/* Load fdc register base */
	sethi	%hi(C_LABEL(fdc_status)), %l3
	ld	[%l3 + %lo(C_LABEL(fdc_status))], %l3

	/* Setup register addresses */
	sethi	%hi(C_LABEL(pdma_vaddr)), %l5	! transfer buffer
	ld	[%l5 + %lo(C_LABEL(pdma_vaddr))], %l4
	sethi	%hi(C_LABEL(pdma_size)), %l5	! bytes to go
	ld	[%l5 + %lo(C_LABEL(pdma_size))], %l6
next_byte:
  	ldub	[%l3], %l7

	andcc	%l7, 0x80, %g0		! Does fifo still have data
	bz	floppy_fifo_emptied	! fifo has been emptied...
	 andcc	%l7, 0x20, %g0		! in non-dma mode still?
	bz	floppy_overrun		! nope, overrun
	 andcc	%l7, 0x40, %g0		! 0=write 1=read
	bz	floppy_write
	 sub	%l6, 0x1, %l6

	/* Ok, actually read this byte */
	ldub	[%l3 + 1], %l7
	orcc	%g0, %l6, %g0
	stb	%l7, [%l4]
	bne	next_byte
	 add	%l4, 0x1, %l4

	b	floppy_tdone
	 nop

floppy_write:
	/* Ok, actually write this byte */
	ldub	[%l4], %l7
	orcc	%g0, %l6, %g0
	stb	%l7, [%l3 + 1]
	bne	next_byte
	 add	%l4, 0x1, %l4

	/* fall through... */
floppy_tdone:
	sethi	%hi(C_LABEL(pdma_vaddr)), %l5
	st	%l4, [%l5 + %lo(C_LABEL(pdma_vaddr))]
	sethi	%hi(C_LABEL(pdma_size)), %l5
	st	%l6, [%l5 + %lo(C_LABEL(pdma_size))]
	/* Flip terminal count pin */
	set	C_LABEL(auxio_register), %l7
	ld	[%l7], %l7

	set	C_LABEL(sparc_cpu_model), %l5
	ld	[%l5], %l5
	subcc   %l5, 1, %g0		/* enum { sun4c = 1 }; */
	be	1f
	 ldub	[%l7], %l5

	or	%l5, 0xc2, %l5
	stb	%l5, [%l7]
	andn    %l5, 0x02, %l5
	b	2f
	 nop

1:
	or      %l5, 0xf4, %l5
	stb     %l5, [%l7]
	andn    %l5, 0x04, %l5

2:
	/* Kill some time so the bits set */
	WRITE_PAUSE
	WRITE_PAUSE

	stb     %l5, [%l7]

	/* Prevent recursion */
	sethi	%hi(C_LABEL(doing_pdma)), %l7
	b	floppy_dosoftint
	 st	%g0, [%l7 + %lo(C_LABEL(doing_pdma))]

	/* We emptied the FIFO, but we haven't read everything
	 * as of yet.  Store the current transfer address and
	 * bytes left to read so we can continue when the next
	 * fast IRQ comes in.
	 */
floppy_fifo_emptied:
	sethi	%hi(C_LABEL(pdma_vaddr)), %l5
	st	%l4, [%l5 + %lo(C_LABEL(pdma_vaddr))]
	sethi	%hi(C_LABEL(pdma_size)), %l7
	st	%l6, [%l7 + %lo(C_LABEL(pdma_size))]

	/* Restore condition codes */
	wr	%l0, 0x0, %psr
	WRITE_PAUSE

	jmp	%l1
	rett	%l2

floppy_overrun:
	sethi	%hi(C_LABEL(pdma_vaddr)), %l5
	st	%l4, [%l5 + %lo(C_LABEL(pdma_vaddr))]
	sethi	%hi(C_LABEL(pdma_size)), %l5
	st	%l6, [%l5 + %lo(C_LABEL(pdma_size))]
	/* Prevent recursion */
	sethi	%hi(C_LABEL(doing_pdma)), %l7
	st	%g0, [%l7 + %lo(C_LABEL(doing_pdma))]

	/* fall through... */
floppy_dosoftint:
	rd	%wim, %l3
	SAVE_ALL

	/* Set all IRQs off. */
	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE

	mov	11, %o0			! floppy irq level (unused anyway)
	mov	%g0, %o1		! devid is not used in fast interrupts
	call	C_LABEL(sparc_floppy_irq)
	 add	%sp, STACKFRAME_SZ, %o2	! struct pt_regs *regs

	RESTORE_ALL
	
#endif /* (CONFIG_BLK_DEV_FD) */

	/* Bad trap handler */
	.globl	bad_trap_handler
bad_trap_handler:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	mov	%l7, %o0		! trap number
	mov	%l0, %o1		! psr
	call	C_LABEL(do_hw_interrupt)
	 mov	%l1, %o2		! pc

	RESTORE_ALL
	
/* For now all IRQ's not registered get sent here. handler_irq() will
 * see if a routine is registered to handle this interrupt and if not
 * it will say so on the console.
 */

	.align	4
	.globl	real_irq_entry, patch_handler_irq
real_irq_entry:
	SAVE_ALL

#ifdef CONFIG_SMP
	.globl	patchme_maybe_smp_msg

	cmp	%l7, 12
patchme_maybe_smp_msg:
	bgu	maybe_smp4m_msg
	 nop
#endif

real_irq_continue:
	or	%l0, PSR_PIL, %g2
	wr	%g2, 0x0, %psr
	WRITE_PAUSE
	wr	%g2, PSR_ET, %psr
	WRITE_PAUSE
	mov	%l7, %o0		! irq level
patch_handler_irq:
	call	C_LABEL(handler_irq)
	 add	%sp, STACKFRAME_SZ, %o1	! pt_regs ptr
	or	%l0, PSR_PIL, %g2	! restore PIL after handler_irq
	wr	%g2, PSR_ET, %psr	! keep ET up
	WRITE_PAUSE

	RESTORE_ALL

#ifdef CONFIG_SMP
	/* SMP per-cpu ticker interrupts are handled specially. */
smp4m_ticker:
	bne	real_irq_continue+4
	 or	%l0, PSR_PIL, %g2
	wr	%g2, 0x0, %psr
	WRITE_PAUSE
	wr	%g2, PSR_ET, %psr
	WRITE_PAUSE
	call	C_LABEL(smp4m_percpu_timer_interrupt)
	 add	%sp, STACKFRAME_SZ, %o0
	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE
	RESTORE_ALL

	/* Here is where we check for possible SMP IPI passed to us
	 * on some level other than 15 which is the NMI and only used
	 * for cross calls.  That has a separate entry point below.
	 */
maybe_smp4m_msg:
	GET_PROCESSOR_MID(o3, o2)
	set	C_LABEL(sun4m_interrupts), %l5
	ld	[%l5], %o5
	sethi	%hi(0x60000000), %o4
	sll	%o3, 12, %o3
	ld	[%o5 + %o3], %o1
	andcc	%o1, %o4, %g0
	be,a	smp4m_ticker
	 cmp	%l7, 14
	cmp	%l7, 13
	add	%o5, %o3, %o5
	bne,a	1f
	 sethi	%hi(0x40000000), %o2
	sethi	%hi(0x20000000), %o2
1:
	st	%o2, [%o5 + 0x4]
	WRITE_PAUSE
	ld	[%o5], %g0
	WRITE_PAUSE
	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE
	cmp	%l7, 13
	bne	2f
	 nop
	call	C_LABEL(smp_reschedule_irq)
	 add	%o7, 8, %o7
2:
	call	C_LABEL(smp_stop_cpu_irq)
	 nop
	RESTORE_ALL

	.align	4
	.globl	linux_trap_ipi15_sun4m
linux_trap_ipi15_sun4m:
	SAVE_ALL
	sethi	%hi(0x80000000), %o2
	GET_PROCESSOR_MID(o0, o1)
	set	C_LABEL(sun4m_interrupts), %l5
	ld	[%l5], %o5
	sll	%o0, 12, %o0
	add	%o5, %o0, %o5
	ld	[%o5], %o3
	andcc	%o3, %o2, %g0
	be	1f			! Must be an NMI async memory error
	 st	%o2, [%o5 + 4]
	WRITE_PAUSE
	ld	[%o5], %g0
	WRITE_PAUSE
	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE
	call	C_LABEL(smp4m_cross_call_irq)
	 nop
	b	ret_trap_lockless_ipi
	 clr	%l6
1:
	/* NMI async memory error handling. */
	sethi	%hi(0x80000000), %l4
	sethi	%hi(0x4000), %o3
	sub	%o5, %o0, %o5
	add	%o5, %o3, %l5
	st	%l4, [%l5 + 0xc]
	WRITE_PAUSE
	ld	[%l5], %g0
	WRITE_PAUSE
	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE
	call	C_LABEL(sun4m_nmi)
	 nop
	st	%l4, [%l5 + 0x8]
	WRITE_PAUSE
	ld	[%l5], %g0
	WRITE_PAUSE
	RESTORE_ALL

	.globl	smp4d_ticker
	/* SMP per-cpu ticker interrupts are handled specially. */
smp4d_ticker:
	SAVE_ALL
	or	%l0, PSR_PIL, %g2
	sethi	%hi(CC_ICLR), %o0
	sethi	%hi(1 << 14), %o1
	or	%o0, %lo(CC_ICLR), %o0
	stha	%o1, [%o0] ASI_M_MXCC	/* Clear PIL 14 in MXCC's ICLR */
	wr	%g2, 0x0, %psr
	WRITE_PAUSE
	wr	%g2, PSR_ET, %psr
	WRITE_PAUSE
	call	C_LABEL(smp4d_percpu_timer_interrupt)
	 add	%sp, STACKFRAME_SZ, %o0
	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE
	RESTORE_ALL

	.align	4
	.globl	linux_trap_ipi15_sun4d
linux_trap_ipi15_sun4d:
	SAVE_ALL
	sethi	%hi(CC_BASE), %o4
	sethi	%hi(MXCC_ERR_ME|MXCC_ERR_PEW|MXCC_ERR_ASE|MXCC_ERR_PEE), %o2
	or	%o4, (CC_EREG - CC_BASE), %o0
	ldda	[%o0] ASI_M_MXCC, %o0
	andcc	%o0, %o2, %g0
	bne	1f
	 sethi	%hi(BB_STAT2), %o2
	lduba	[%o2] ASI_M_CTL, %o2
	andcc	%o2, BB_STAT2_MASK, %g0
	bne	2f
	 or	%o4, (CC_ICLR - CC_BASE), %o0
	sethi	%hi(1 << 15), %o1
	stha	%o1, [%o0] ASI_M_MXCC	/* Clear PIL 15 in MXCC's ICLR */
	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE
	call	C_LABEL(smp4d_cross_call_irq)
	 nop
	b	ret_trap_lockless_ipi
	 clr	%l6

1:	/* MXCC error */
2:	/* BB error */
	/* Disable PIL 15 */
	set	CC_IMSK, %l4
	lduha	[%l4] ASI_M_MXCC, %l5
	sethi	%hi(1 << 15), %l7
	or	%l5, %l7, %l5
	stha	%l5, [%l4] ASI_M_MXCC
	/* FIXME */
1:	b,a	1b

#endif /* CONFIG_SMP */

	/* This routine handles illegal instructions and privileged
	 * instruction attempts from user code.
	 */
	.align	4
	.globl	bad_instruction
bad_instruction:
	sethi	%hi(0xc1f80000), %l4
	ld	[%l1], %l5
	sethi	%hi(0x81d80000), %l7
	and	%l5, %l4, %l5
	cmp	%l5, %l7
	be	1f
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(do_illegal_instruction)
	 mov	%l0, %o3

	RESTORE_ALL

1:	/* unimplemented flush - just skip */
	jmpl	%l2, %g0
	 rett	%l2 + 4

	.align	4
	.globl	priv_instruction
priv_instruction:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(do_priv_instruction)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles unaligned data accesses. */
	.align	4
	.globl	mna_handler
mna_handler:
	andcc	%l0, PSR_PS, %g0
	be	mna_fromuser
	 nop

	SAVE_ALL

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	ld	[%l1], %o1
	call	C_LABEL(kernel_unaligned_trap)
	 add	%sp, STACKFRAME_SZ, %o0

	RESTORE_ALL

mna_fromuser:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	ld	[%l1], %o1
	call	C_LABEL(user_unaligned_trap)
	 add	%sp, STACKFRAME_SZ, %o0

	RESTORE_ALL

	/* This routine handles floating point disabled traps. */
	.align	4
	.globl	fpd_trap_handler
fpd_trap_handler:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(do_fpd_trap)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Floating Point Exceptions. */
	.align	4
	.globl	fpe_trap_handler
fpe_trap_handler:
	set	fpsave_magic, %l5
	cmp	%l1, %l5
	be	1f
	 sethi	%hi(C_LABEL(fpsave)), %l5
	or	%l5, %lo(C_LABEL(fpsave)), %l5
	cmp	%l1, %l5
	bne	2f
	 sethi	%hi(fpsave_catch2), %l5
	or	%l5, %lo(fpsave_catch2), %l5
	wr	%l0, 0x0, %psr
	WRITE_PAUSE
	jmp	%l5
	 rett	%l5 + 4
1:	
	sethi	%hi(fpsave_catch), %l5
	or	%l5, %lo(fpsave_catch), %l5
	wr	%l0, 0x0, %psr
	WRITE_PAUSE
	jmp	%l5
	 rett	%l5 + 4

2:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(do_fpe_trap)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Tag Overflow Exceptions. */
	.align	4
	.globl	do_tag_overflow
do_tag_overflow:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_tag_overflow)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Watchpoint Exceptions. */
	.align	4
	.globl	do_watchpoint
do_watchpoint:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_watchpoint)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Register Access Exceptions. */
	.align	4
	.globl	do_reg_access
do_reg_access:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_reg_access)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Co-Processor Disabled Exceptions. */
	.align	4
	.globl	do_cp_disabled
do_cp_disabled:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_cp_disabled)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Co-Processor Exceptions. */
	.align	4
	.globl	do_cp_exception
do_cp_exception:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_cp_exception)
	 mov	%l0, %o3

	RESTORE_ALL

	/* This routine handles Hardware Divide By Zero Exceptions. */
	.align	4
	.globl	do_hw_divzero
do_hw_divzero:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr		! re-enable traps
	WRITE_PAUSE

	add	%sp, STACKFRAME_SZ, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	C_LABEL(handle_hw_divzero)
	 mov	%l0, %o3

	RESTORE_ALL

	.align	4
	.globl	do_flush_windows
do_flush_windows:
	SAVE_ALL

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	andcc	%l0, PSR_PS, %g0
	bne	dfw_kernel
	 nop

	call	C_LABEL(flush_user_windows)
	 nop

	/* Advance over the trap instruction. */
	ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1
	add	%l1, 0x4, %l2
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]

	RESTORE_ALL

	.globl	flush_patch_one

	/* We get these for debugging routines using __builtin_return_address() */
dfw_kernel:
flush_patch_one:
	FLUSH_ALL_KERNEL_WINDOWS

	/* Advance over the trap instruction. */
	ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1
	add	%l1, 0x4, %l2
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]

	RESTORE_ALL

	/* The getcc software trap.  The user wants the condition codes from
	 * the %psr in register %g1.
	 */

	.align	4
	.globl	getcc_trap_handler
getcc_trap_handler:
	srl	%l0, 20, %g1	! give user
	and	%g1, 0xf, %g1	! only ICC bits in %psr
	jmp	%l2		! advance over trap instruction
	rett	%l2 + 0x4	! like this...

	/* The setcc software trap.  The user has condition codes in %g1
	 * that it would like placed in the %psr.  Be careful not to flip
	 * any unintentional bits!
	 */

	.align	4
	.globl	setcc_trap_handler
setcc_trap_handler:
	sll	%g1, 0x14, %l4
	set	PSR_ICC, %l5
	andn	%l0, %l5, %l0	! clear ICC bits in %psr
	and	%l4, %l5, %l4	! clear non-ICC bits in user value
	or	%l4, %l0, %l4	! or them in... mix mix mix

	wr	%l4, 0x0, %psr	! set new %psr
	WRITE_PAUSE		! TI scumbags...

	jmp	%l2		! advance over trap instruction
	rett	%l2 + 0x4	! like this...

	.align	4
	.globl	linux_trap_nmi_sun4c
linux_trap_nmi_sun4c:
	SAVE_ALL

	/* Ugh, we need to clear the IRQ line.  This is now
	 * a very sun4c specific trap handler...
	 */
	sethi	%hi(C_LABEL(interrupt_enable)), %l5
	ld	[%l5 + %lo(C_LABEL(interrupt_enable))], %l5
	ldub	[%l5], %l6
	andn	%l6, INTS_ENAB, %l6
	stb	%l6, [%l5]

	/* Now it is safe to re-enable traps without recursion. */
	or	%l0, PSR_PIL, %l0
	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	/* Now call the c-code with the pt_regs frame ptr and the
	 * memory error registers as arguments.  The ordering chosen
	 * here is due to unlatching semantics.
	 */
	sethi	%hi(AC_SYNC_ERR), %o0
	add	%o0, 0x4, %o0
	lda	[%o0] ASI_CONTROL, %o2	! sync vaddr
	sub	%o0, 0x4, %o0
	lda	[%o0] ASI_CONTROL, %o1	! sync error
	add	%o0, 0xc, %o0
	lda	[%o0] ASI_CONTROL, %o4	! async vaddr
	sub	%o0, 0x4, %o0
	lda	[%o0] ASI_CONTROL, %o3	! async error
	call	C_LABEL(sparc_lvl15_nmi)
	 add	%sp, STACKFRAME_SZ, %o0

	RESTORE_ALL

	.align	4
	.globl	C_LABEL(invalid_segment_patch1_ff)
	.globl	C_LABEL(invalid_segment_patch2_ff)
C_LABEL(invalid_segment_patch1_ff):	cmp	%l4, 0xff
C_LABEL(invalid_segment_patch2_ff):	mov	0xff, %l3

	.align	4
	.globl	C_LABEL(invalid_segment_patch1_1ff)
	.globl	C_LABEL(invalid_segment_patch2_1ff)
C_LABEL(invalid_segment_patch1_1ff):	cmp	%l4, 0x1ff
C_LABEL(invalid_segment_patch2_1ff):	mov	0x1ff, %l3

	.align	4
	.globl	C_LABEL(num_context_patch1_16), C_LABEL(num_context_patch2_16)
C_LABEL(num_context_patch1_16):		mov	0x10, %l7
C_LABEL(num_context_patch2_16):		mov	0x10, %l7

	.align	4
	.globl	C_LABEL(vac_linesize_patch_32)
C_LABEL(vac_linesize_patch_32):		subcc	%l7, 32, %l7

	.align	4
	.globl	C_LABEL(vac_hwflush_patch1_on), C_LABEL(vac_hwflush_patch2_on)

/*
 * Ugly, but we cant use hardware flushing on the sun4 and we'd require
 * two instructions (Anton)
 */
#ifdef CONFIG_SUN4
C_LABEL(vac_hwflush_patch1_on):		nop
#else
C_LABEL(vac_hwflush_patch1_on):		addcc	%l7, -PAGE_SIZE, %l7
#endif

C_LABEL(vac_hwflush_patch2_on):		sta	%g0, [%l3 + %l7] ASI_HWFLUSHSEG

	.globl	C_LABEL(invalid_segment_patch1), C_LABEL(invalid_segment_patch2)
	.globl	C_LABEL(num_context_patch1), C_LABEL(num_context_patch2)
	.globl	C_LABEL(vac_linesize_patch), C_LABEL(vac_hwflush_patch1)
	.globl	C_LABEL(vac_hwflush_patch2)

	.align	4
	.globl	sun4c_fault

! %l0 = %psr
! %l1 = %pc
! %l2 = %npc
! %l3 = %wim
! %l7 = 1 for textfault
! We want error in %l5, vaddr in %l6
sun4c_fault:
#ifdef CONFIG_SUN4
	sethi	%hi(C_LABEL(sun4c_memerr_reg)), %l4
	ld	[%l4+%lo(C_LABEL(sun4c_memerr_reg))], %l4  ! memerr ctrl reg addr
	ld	[%l4], %l6		! memerr ctrl reg
	ld	[%l4 + 4], %l5		! memerr vaddr reg
	andcc	%l6, 0x80, %g0		! check for error type
	st	%g0, [%l4 + 4]		! clear the error
	be	0f			! normal error
	 sethi	%hi(AC_BUS_ERROR), %l4	! bus err reg addr

	call	C_LABEL(prom_halt)	! something weird happened
					! what exactly did happen?
					! what should we do here?

0:	or	%l4, %lo(AC_BUS_ERROR), %l4	! bus err reg addr
	lduba	[%l4] ASI_CONTROL, %l6	! bus err reg

	cmp    %l7, 1			! text fault?
	be	1f			! yes
	 nop

	ld     [%l1], %l4		! load instruction that caused fault
	srl	%l4, 21, %l4
	andcc	%l4, 1, %g0		! store instruction?

	be	1f			! no
	 sethi	%hi(SUN4C_SYNC_BADWRITE), %l4 ! yep
					! %lo(SUN4C_SYNC_BADWRITE) = 0
	or	%l4, %l6, %l6		! set write bit to emulate sun4c
1:
#else
	sethi	%hi(AC_SYNC_ERR), %l4
	add	%l4, 0x4, %l6			! AC_SYNC_VA in %l6
	lda	[%l6] ASI_CONTROL, %l5		! Address
	lda	[%l4] ASI_CONTROL, %l6		! Error, retained for a bit
#endif

	andn	%l5, 0xfff, %l5			! Encode all info into l7
	srl	%l6, 14, %l4

	and	%l4, 2, %l4
	or	%l5, %l4, %l4

	or	%l4, %l7, %l7			! l7 = [addr,write,txtfault]

	andcc	%l0, PSR_PS, %g0
	be	sun4c_fault_fromuser
	 andcc	%l7, 1, %g0			! Text fault?

	be	1f
	 sethi	%hi(KERNBASE), %l4

	mov	%l1, %l5			! PC

1:
	cmp	%l5, %l4
	blu	sun4c_fault_fromuser
	 sethi	%hi(~((1 << SUN4C_REAL_PGDIR_SHIFT) - 1)), %l4

	/* If the kernel references a bum kernel pointer, or a pte which
	 * points to a non existant page in ram, we will run this code
	 * _forever_ and lock up the machine!!!!! So we must check for
	 * this condition, the AC_SYNC_ERR bits are what we must examine.
	 * Also a parity error would make this happen as well.  So we just
	 * check that we are in fact servicing a tlb miss and not some
	 * other type of fault for the kernel.
	 */
	andcc	%l6, 0x80, %g0
	be	sun4c_fault_fromuser
	 and	%l5, %l4, %l5

	/* Test for NULL pte_t * in vmalloc area. */
	sethi   %hi(VMALLOC_START), %l4
	cmp     %l5, %l4
	blu,a   C_LABEL(invalid_segment_patch1)
	 lduXa	[%l5] ASI_SEGMAP, %l4

	sethi   %hi(C_LABEL(swapper_pg_dir)), %l4
	srl     %l5, SUN4C_PGDIR_SHIFT, %l6
	or      %l4, %lo(C_LABEL(swapper_pg_dir)), %l4
	sll     %l6, 2, %l6
	ld      [%l4 + %l6], %l4
#ifdef CONFIG_SUN4
	sethi	%hi(PAGE_MASK), %l6
	andcc	%l4, %l6, %g0
#else
	andcc   %l4, PAGE_MASK, %g0
#endif
	be      sun4c_fault_fromuser
	 lduXa  [%l5] ASI_SEGMAP, %l4

C_LABEL(invalid_segment_patch1):
	cmp	%l4, 0x7f
	bne	1f
	 sethi	%hi(C_LABEL(sun4c_kfree_ring)), %l4
	or	%l4, %lo(C_LABEL(sun4c_kfree_ring)), %l4
	ld	[%l4 + 0x18], %l3
	deccc	%l3			! do we have a free entry?
	bcs,a	2f			! no, unmap one.
	 sethi	%hi(C_LABEL(sun4c_kernel_ring)), %l4

	st	%l3, [%l4 + 0x18]	! sun4c_kfree_ring.num_entries--

	ld	[%l4 + 0x00], %l6	! entry = sun4c_kfree_ring.ringhd.next
	st	%l5, [%l6 + 0x08]	! entry->vaddr = address

	ld	[%l6 + 0x00], %l3	! next = entry->next
	ld	[%l6 + 0x04], %l7	! entry->prev

	st	%l7, [%l3 + 0x04]	! next->prev = entry->prev
	st	%l3, [%l7 + 0x00]	! entry->prev->next = next

	sethi	%hi(C_LABEL(sun4c_kernel_ring)), %l4
	or	%l4, %lo(C_LABEL(sun4c_kernel_ring)), %l4
					! head = &sun4c_kernel_ring.ringhd

	ld	[%l4 + 0x00], %l7	! head->next

	st	%l4, [%l6 + 0x04]	! entry->prev = head
	st	%l7, [%l6 + 0x00]	! entry->next = head->next
	st	%l6, [%l7 + 0x04]	! head->next->prev = entry

	st	%l6, [%l4 + 0x00]	! head->next = entry

	ld	[%l4 + 0x18], %l3
	inc	%l3			! sun4c_kernel_ring.num_entries++
	st	%l3, [%l4 + 0x18]
	b	4f
	 ld	[%l6 + 0x08], %l5

2:
	or	%l4, %lo(C_LABEL(sun4c_kernel_ring)), %l4
					! head = &sun4c_kernel_ring.ringhd

	ld	[%l4 + 0x04], %l6	! entry = head->prev

	ld	[%l6 + 0x08], %l3	! tmp = entry->vaddr

	! Flush segment from the cache.
#ifdef CONFIG_SUN4
	sethi	%hi((128 * 1024)), %l7
#else
	sethi	%hi((64 * 1024)), %l7
#endif
9:
C_LABEL(vac_hwflush_patch1):
C_LABEL(vac_linesize_patch):
	subcc	%l7, 16, %l7
	bne	9b
C_LABEL(vac_hwflush_patch2):
	 sta	%g0, [%l3 + %l7] ASI_FLUSHSEG

	st	%l5, [%l6 + 0x08]	! entry->vaddr = address

	ld	[%l6 + 0x00], %l5	! next = entry->next
	ld	[%l6 + 0x04], %l7	! entry->prev

	st	%l7, [%l5 + 0x04]	! next->prev = entry->prev
	st	%l5, [%l7 + 0x00]	! entry->prev->next = next
	st	%l4, [%l6 + 0x04]	! entry->prev = head

	ld	[%l4 + 0x00], %l7	! head->next

	st	%l7, [%l6 + 0x00]	! entry->next = head->next
	st	%l6, [%l7 + 0x04]	! head->next->prev = entry
	st	%l6, [%l4 + 0x00]	! head->next = entry

	mov	%l3, %l5		! address = tmp

4:
C_LABEL(num_context_patch1):
	mov	0x08, %l7

	ld	[%l6 + 0x08], %l4
	ldub	[%l6 + 0x0c], %l3
	or	%l4, %l3, %l4		! encode new vaddr/pseg into l4

	sethi	%hi(AC_CONTEXT), %l3
	lduba	[%l3] ASI_CONTROL, %l6

	/* Invalidate old mapping, instantiate new mapping,
	 * for each context.  Registers l6/l7 are live across
	 * this loop.
	 */
3:	deccc	%l7
	sethi	%hi(AC_CONTEXT), %l3
	stba	%l7, [%l3] ASI_CONTROL
C_LABEL(invalid_segment_patch2):
	mov	0x7f, %l3
	stXa	%l3, [%l5] ASI_SEGMAP
	andn	%l4, 0x1ff, %l3
	bne	3b
	 stXa	%l4, [%l3] ASI_SEGMAP

	sethi	%hi(AC_CONTEXT), %l3
	stba	%l6, [%l3] ASI_CONTROL

	andn	%l4, 0x1ff, %l5

1:
	sethi	%hi(VMALLOC_START), %l4
	cmp	%l5, %l4

	bgeu	1f
	 mov	1 << (SUN4C_REAL_PGDIR_SHIFT - PAGE_SHIFT), %l7

	sethi	%hi(KERNBASE), %l6

	sub	%l5, %l6, %l4
	srl	%l4, PAGE_SHIFT, %l4
	sethi	%hi((SUN4C_PAGE_KERNEL & 0xf4000000)), %l3
	or	%l3, %l4, %l3

	sethi	%hi(PAGE_SIZE), %l4

2:
	sta	%l3, [%l5] ASI_PTE
	deccc	%l7
	inc	%l3
	bne	2b
	 add	%l5, %l4, %l5

	b	7f
	 sethi	%hi(C_LABEL(sun4c_kernel_faults)), %l4

1:
	srl	%l5, SUN4C_PGDIR_SHIFT, %l3
	sethi	%hi(C_LABEL(swapper_pg_dir)), %l4
	or	%l4, %lo(C_LABEL(swapper_pg_dir)), %l4
	sll	%l3, 2, %l3
	ld	[%l4 + %l3], %l4
#ifndef CONFIG_SUN4
	and	%l4, PAGE_MASK, %l4
#else
	sethi	%hi(PAGE_MASK), %l6
	and	%l4, %l6, %l4
#endif

	srl	%l5, (PAGE_SHIFT - 2), %l6
	and	%l6, ((SUN4C_PTRS_PER_PTE - 1) << 2), %l6
	add	%l6, %l4, %l6

	sethi	%hi(PAGE_SIZE), %l4

2:
	ld	[%l6], %l3
	deccc	%l7
	sta	%l3, [%l5] ASI_PTE
	add	%l6, 0x4, %l6
	bne	2b
	 add	%l5, %l4, %l5

	sethi	%hi(C_LABEL(sun4c_kernel_faults)), %l4
7:
	ld	[%l4 + %lo(C_LABEL(sun4c_kernel_faults))], %l3
	inc	%l3
	st	%l3, [%l4 + %lo(C_LABEL(sun4c_kernel_faults))]

	/* Restore condition codes */
	wr	%l0, 0x0, %psr
	WRITE_PAUSE
	jmp	%l1
	 rett	%l2

sun4c_fault_fromuser:
	SAVE_ALL
	 nop
	
	mov	%l7, %o1		! Decode the info from %l7
	mov	%l7, %o2
	and	%o1, 1, %o1		! arg2 = text_faultp
	mov	%l7, %o3
	and	%o2, 2, %o2		! arg3 = writep
	andn	%o3, 0xfff, %o3		! arg4 = faulting address

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	call	C_LABEL(do_sun4c_fault)
	 add	%sp, STACKFRAME_SZ, %o0	! arg1 = pt_regs ptr

	RESTORE_ALL

	.align	4
	.globl	C_LABEL(srmmu_fault)
C_LABEL(srmmu_fault):
	mov	0x400, %l5
	mov	0x300, %l4

	lda	[%l5] ASI_M_MMUREGS, %l6	! read sfar first
	lda	[%l4] ASI_M_MMUREGS, %l5	! read sfsr last

	andn	%l6, 0xfff, %l6
	srl	%l5, 6, %l5			! and encode all info into l7

	and	%l5, 2, %l5
	or	%l5, %l6, %l6

	or	%l6, %l7, %l7			! l7 = [addr,write,txtfault]

	SAVE_ALL

	mov	%l7, %o1
	mov	%l7, %o2
	and	%o1, 1, %o1		! arg2 = text_faultp
	mov	%l7, %o3
	and	%o2, 2, %o2		! arg3 = writep
	andn	%o3, 0xfff, %o3		! arg4 = faulting address

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	call	C_LABEL(do_sparc_fault)
	 add	%sp, STACKFRAME_SZ, %o0	! arg1 = pt_regs ptr

	RESTORE_ALL

#ifdef CONFIG_SUNOS_EMUL
	/* SunOS uses syscall zero as the 'indirect syscall' it looks
	 * like indir_syscall(scall_num, arg0, arg1, arg2...);  etc.
	 * This is complete brain damage.
	 */
	.globl	C_LABEL(sunos_indir)
C_LABEL(sunos_indir):
	mov	%o7, %l4
	cmp	%o0, NR_SYSCALLS
	blu,a	1f
	 sll	%o0, 0x2, %o0

	sethi	%hi(C_LABEL(sunos_nosys)), %l6
	b	2f
	 or	%l6, %lo(C_LABEL(sunos_nosys)), %l6

1:
	set	C_LABEL(sunos_sys_table), %l7
	ld	[%l7 + %o0], %l6

2:	
	mov	%o1, %o0
	mov	%o2, %o1
	mov	%o3, %o2
	mov	%o4, %o3
	mov	%o5, %o4
	call	%l6
	 mov	%l4, %o7
#endif

	.align	4
	.globl	C_LABEL(sys_nis_syscall)
C_LABEL(sys_nis_syscall):
	mov	%o7, %l5
	add	%sp, STACKFRAME_SZ, %o0		! pt_regs *regs arg
	call	C_LABEL(c_sys_nis_syscall)
	 mov	%l5, %o7

	.align 4
	.globl	C_LABEL(sys_ptrace)
C_LABEL(sys_ptrace):
	call	C_LABEL(do_ptrace)
	 add	%sp, STACKFRAME_SZ, %o0

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	RESTORE_ALL

	.align	4
	.globl	C_LABEL(sys_execve)
C_LABEL(sys_execve):
	mov	%o7, %l5
	add	%sp, STACKFRAME_SZ, %o0		! pt_regs *regs arg
	call	C_LABEL(sparc_execve)
	 mov	%l5, %o7

	.align	4
	.globl	C_LABEL(sys_pipe)
C_LABEL(sys_pipe):
	mov	%o7, %l5
	add	%sp, STACKFRAME_SZ, %o0		! pt_regs *regs arg
	call	C_LABEL(sparc_pipe)
	 mov	%l5, %o7

	.align	4
	.globl	C_LABEL(sys_sigaltstack)
C_LABEL(sys_sigaltstack):
	mov	%o7, %l5
	mov	%fp, %o2
	call	C_LABEL(do_sigaltstack)
	 mov	%l5, %o7

	.align	4
	.globl	C_LABEL(sys_sigstack)
C_LABEL(sys_sigstack):
	mov	%o7, %l5
	mov	%fp, %o2
	call	C_LABEL(do_sys_sigstack)
	 mov	%l5, %o7

	.align	4
	.globl	C_LABEL(sys_sigpause)
C_LABEL(sys_sigpause):
	/* Note: %o0 already has correct value... */
	call	C_LABEL(do_sigpause)
	 add	%sp, STACKFRAME_SZ, %o1

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	/* We are returning to a signal handler. */
	RESTORE_ALL

	.align	4
	.globl	C_LABEL(sys_sigsuspend)
C_LABEL(sys_sigsuspend):
	call	C_LABEL(do_sigsuspend)
	 add	%sp, STACKFRAME_SZ, %o0

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	/* We are returning to a signal handler. */
	RESTORE_ALL

	.align	4
	.globl	C_LABEL(sys_rt_sigsuspend)
C_LABEL(sys_rt_sigsuspend):
	/* Note: %o0, %o1 already have correct value... */
	call	C_LABEL(do_rt_sigsuspend)
	 add	%sp, STACKFRAME_SZ, %o2

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	/* We are returning to a signal handler. */
	RESTORE_ALL

	.align	4
	.globl	C_LABEL(sys_sigreturn)
C_LABEL(sys_sigreturn):
	call	C_LABEL(do_sigreturn)
	 add	%sp, STACKFRAME_SZ, %o0

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	/* We don't want to muck with user registers like a
	 * normal syscall, just return.
	 */
	RESTORE_ALL

	.align	4
	.globl	C_LABEL(sys_rt_sigreturn)
C_LABEL(sys_rt_sigreturn):
	call	C_LABEL(do_rt_sigreturn)
	 add	%sp, STACKFRAME_SZ, %o0

	ld	[%curptr + AOFF_task_ptrace], %l5
	andcc	%l5, 0x02, %g0
	be	1f
	 nop

	call	C_LABEL(syscall_trace)
	 nop

1:
	/* We are returning to a signal handler. */
	RESTORE_ALL

	/* Now that we have a real sys_clone, sys_fork() is
	 * implemented in terms of it.  Our _real_ implementation
	 * of SunOS vfork() will use sys_vfork().
	 *
	 * XXX These three should be consolidated into mostly shared
	 * XXX code just like on sparc64... -DaveM
	 */
	.align	4
	.globl	C_LABEL(sys_fork), flush_patch_two
C_LABEL(sys_fork):
	mov	%o7, %l5
flush_patch_two:
	FLUSH_ALL_KERNEL_WINDOWS;
	rd	%psr, %g4
	WRITE_PAUSE
	mov	SIGCHLD, %o0			! arg0:	clone flags
	rd	%wim, %g5
	WRITE_PAUSE
	mov	%fp, %o1			! arg1:	usp
	std	%g4, [%curptr + AOFF_task_thread + AOFF_thread_fork_kpsr]
	add	%sp, STACKFRAME_SZ, %o2		! arg2:	pt_regs ptr
	mov	0, %o3
	call	C_LABEL(do_fork)
	 mov	%l5, %o7
	
	/* Whee, kernel threads! */
	.globl	C_LABEL(sys_clone), flush_patch_three
C_LABEL(sys_clone):
	mov	%o7, %l5
flush_patch_three:
	FLUSH_ALL_KERNEL_WINDOWS;
	rd	%psr, %g4
	WRITE_PAUSE

	/* arg0,1: flags,usp  -- loaded already */
	cmp	%o1, 0x0			! Is new_usp NULL?
	rd	%wim, %g5
	WRITE_PAUSE
	be,a	1f
	 mov	%fp, %o1			! yes, use callers usp
	andn	%o1, 7, %o1			! no, align to 8 bytes
1:
	std	%g4, [%curptr + AOFF_task_thread + AOFF_thread_fork_kpsr]
	add	%sp, STACKFRAME_SZ, %o2		! arg2:	pt_regs ptr
	mov	0, %o3
	call	C_LABEL(do_fork)
	 mov	%l5, %o7

	/* Whee, real vfork! */
	.globl	C_LABEL(sys_vfork), flush_patch_four
C_LABEL(sys_vfork):
flush_patch_four:
	FLUSH_ALL_KERNEL_WINDOWS;
	rd	%psr, %g4
	WRITE_PAUSE
	rd	%wim, %g5
	WRITE_PAUSE
	std	%g4, [%curptr + AOFF_task_thread + AOFF_thread_fork_kpsr]
	sethi	%hi(0x4000 | 0x0100 | SIGCHLD), %o0
	mov	%fp, %o1
	or	%o0, %lo(0x4000 | 0x0100 | SIGCHLD), %o0
	sethi	%hi(C_LABEL(do_fork)), %l1
	mov	0, %o3
	jmpl	%l1 + %lo(C_LABEL(do_fork)), %g0
	 add	%sp, STACKFRAME_SZ, %o2

        .align  4
linux_sparc_ni_syscall:
	sethi   %hi(C_LABEL(sys_ni_syscall)), %l7
	b       syscall_is_too_hard
	 or     %l7, %lo(C_LABEL(sys_ni_syscall)), %l7

linux_fast_syscall:
	andn	%l7, 3, %l7
	mov	%i0, %o0
	mov	%i1, %o1
	mov 	%i2, %o2
	jmpl	%l7 + %g0, %g0
	 mov	%i3, %o3

linux_syscall_trace:
	call	C_LABEL(syscall_trace)
	 nop
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	b	2f
	 mov	%i4, %o4

	.globl	C_LABEL(ret_from_fork)
C_LABEL(ret_from_fork):
	call	schedule_tail
	 mov	%g3, %o0
	b	C_LABEL(ret_sys_call)
	 ld	[%sp + STACKFRAME_SZ + PT_I0], %o0

	/* Linux native and SunOS system calls enter here... */
	.align	4
	.globl	linux_sparc_syscall
linux_sparc_syscall:
	/* Direct access to user regs, must faster. */
	cmp	%g1, NR_SYSCALLS
	bgeu	linux_sparc_ni_syscall
	 sll	%g1, 2, %l4
	ld	[%l7 + %l4], %l7
	andcc	%l7, 1, %g0
	bne	linux_fast_syscall
	 /* Just do first insn from SAVE_ALL in the delay slot */

	.globl	syscall_is_too_hard
syscall_is_too_hard:
	SAVE_ALL_HEAD
	 rd	%wim, %l3

	wr	%l0, PSR_ET, %psr
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2

	ld	[%curptr + AOFF_task_ptrace], %l5
	mov	%i3, %o3
	andcc	%l5, 0x02, %g0
	mov	%i4, %o4
	bne	linux_syscall_trace
	 mov	%i0, %l5
2:
	call	%l7
	 mov	%i5, %o5

	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]

	.globl	C_LABEL(ret_sys_call)
C_LABEL(ret_sys_call):
	ld	[%curptr + AOFF_task_ptrace], %l6
	cmp	%o0, -ENOIOCTLCMD
	ld	[%sp + STACKFRAME_SZ + PT_PSR], %g3
	set	PSR_C, %g2
	bgeu	1f
	 andcc	%l6, 0x02, %l6	

	/* System call success, clear Carry condition code. */
	andn	%g3, %g2, %g3
	clr	%l6
	st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]	
	bne	linux_syscall_trace2
	 ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1 /* pc = npc */
	add	%l1, 0x4, %l2			/* npc = npc+4 */
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	b	ret_trap_entry
	 st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]
1:
	/* System call failure, set Carry condition code.
	 * Also, get abs(errno) to return to the process.
	 */
	sub	%g0, %o0, %o0
	or	%g3, %g2, %g3
	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]
	mov	1, %l6
	st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]
	bne	linux_syscall_trace2
	 ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1 /* pc = npc */
	add	%l1, 0x4, %l2			/* npc = npc+4 */
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	b	ret_trap_entry
	 st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]

linux_syscall_trace2:
	call	C_LABEL(syscall_trace)
	 add	%l1, 0x4, %l2			/* npc = npc+4 */
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	b	ret_trap_entry
	 st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]


	/*
	 * Solaris system calls and indirect system calls enter here.
         *
	 * I have named the solaris indirect syscalls like that because
	 * it seems like Solaris has some fast path syscalls that can
	 * be handled as indirect system calls. - mig
	 */

linux_syscall_for_solaris:
	sethi	%hi(sys_call_table), %l7
	b	linux_sparc_syscall
	 or	%l7, %lo(sys_call_table), %l7
	
	.align	4
	.globl	solaris_syscall
solaris_syscall:
	cmp	%g1,59
	be	linux_syscall_for_solaris
	 cmp	%g1,2
	be	linux_syscall_for_solaris
	 cmp    %g1,42
	be      linux_syscall_for_solaris
	 cmp	%g1,119
	be,a	linux_syscall_for_solaris
	 mov	2, %g1
1:	
	SAVE_ALL_HEAD
	 rd	%wim, %l3

	wr	%l0, PSR_ET, %psr
	nop
	nop
	mov	%i0, %l5

	call	C_LABEL(do_solaris_syscall)
	 add	%sp, STACKFRAME_SZ, %o0

	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]
	set	PSR_C, %g2
	cmp	%o0, -ENOIOCTLCMD
	bgeu	1f
	 ld	[%sp + STACKFRAME_SZ + PT_PSR], %g3

	/* System call success, clear Carry condition code. */		
	andn	%g3, %g2, %g3
	clr	%l6
	b	2f
	 st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]	

1:
	/* System call failure, set Carry condition code.
	 * Also, get abs(errno) to return to the process.
	 */
	sub	%g0, %o0, %o0
	mov	1, %l6
	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]
	or	%g3, %g2, %g3
	st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]

	/* Advance the pc and npc over the trap instruction.
	 * If the npc is unaligned (has a 1 in the lower byte), it means
	 * the kernel does not want us to play magic (ie, skipping over
	 * traps).  Mainly when the Solaris code wants to set some PC and
	 * nPC (setcontext).
	 */
2:
	ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1	/* pc  = npc   */
	andcc	%l1, 1, %g0
	bne	1f
	 add	%l1, 0x4, %l2			/* npc = npc+4 */
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	b	ret_trap_entry
	 st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]

	/* kernel knows what it is doing, fixup npc and continue */
1:
	sub	%l1, 1, %l1
 	b	ret_trap_entry	
	 st	%l1, [%sp + STACKFRAME_SZ + PT_NPC]

#ifndef CONFIG_SUNOS_EMUL
	.align	4
	.globl	sunos_syscall
sunos_syscall:
	SAVE_ALL_HEAD
	 rd	%wim, %l3
	wr	%l0, PSR_ET, %psr
	nop
	nop
	mov	%i0, %l5
	call	C_LABEL(do_sunos_syscall)
	 add	%sp, STACKFRAME_SZ, %o0
#endif

	/* {net, open}bsd system calls enter here... */
	.align	4
	.globl	bsd_syscall
bsd_syscall:
	/* Direct access to user regs, must faster. */
	cmp	%g1, NR_SYSCALLS
	blu,a	1f
	 sll	%g1, 2, %l4

	set	C_LABEL(sys_ni_syscall), %l7
	b	bsd_is_too_hard
	 nop

1:
	ld	[%l7 + %l4], %l7

	.globl	bsd_is_too_hard
bsd_is_too_hard:
	rd	%wim, %l3
	SAVE_ALL

	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

2:
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i0, %l5
	mov	%i3, %o3
	mov	%i4, %o4
	call	%l7
	 mov	%i5, %o5

	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]
	set	PSR_C, %g2
	cmp	%o0, -ENOIOCTLCMD
	bgeu	1f
	 ld	[%sp + STACKFRAME_SZ + PT_PSR], %g3

	/* System call success, clear Carry condition code. */		
	andn	%g3, %g2, %g3
	clr	%l6
	b	2f
	 st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]	

1:
	/* System call failure, set Carry condition code.
	 * Also, get abs(errno) to return to the process.
	 */
	sub	%g0, %o0, %o0
#if 0 /* XXX todo XXX */
	sethi	%hi(C_LABEL(bsd_xlatb_rorl), %o3
	or	%o3, %lo(C_LABEL(bsd_xlatb_rorl)), %o3
	sll	%o0, 2, %o0
	ld	[%o3 + %o0], %o0
#endif
	mov	1, %l6
	st	%o0, [%sp + STACKFRAME_SZ + PT_I0]
	or	%g3, %g2, %g3
	st	%g3, [%sp + STACKFRAME_SZ + PT_PSR]

	/* Advance the pc and npc over the trap instruction. */
2:
	ld	[%sp + STACKFRAME_SZ + PT_NPC], %l1	/* pc  = npc   */
	add	%l1, 0x4, %l2			/* npc = npc+4 */
	st	%l1, [%sp + STACKFRAME_SZ + PT_PC]
	b	ret_trap_entry
	 st	%l2, [%sp + STACKFRAME_SZ + PT_NPC]

/* Saving and restoring the FPU state is best done from lowlevel code.
 *
 * void fpsave(unsigned long *fpregs, unsigned long *fsr,
 *             void *fpqueue, unsigned long *fpqdepth)
 */

	.globl	C_LABEL(fpsave)
C_LABEL(fpsave):
	st	%fsr, [%o1]	! this can trap on us if fpu is in bogon state
	ld	[%o1], %g1
	set	0x2000, %g4
	andcc	%g1, %g4, %g0
	be	2f
	 mov	0, %g2

	/* We have an fpqueue to save. */
1:
	std	%fq, [%o2]
fpsave_magic:
	st	%fsr, [%o1]
	ld	[%o1], %g3
	andcc	%g3, %g4, %g0
	add	%g2, 1, %g2
	bne	1b
	 add	%o2, 8, %o2

2:
	st	%g2, [%o3]

	std	%f0, [%o0 + 0x00]
	std	%f2, [%o0 + 0x08]
	std	%f4, [%o0 + 0x10]
	std	%f6, [%o0 + 0x18]
	std	%f8, [%o0 + 0x20]
	std	%f10, [%o0 + 0x28]
	std	%f12, [%o0 + 0x30]
	std	%f14, [%o0 + 0x38]
	std	%f16, [%o0 + 0x40]
	std	%f18, [%o0 + 0x48]
	std	%f20, [%o0 + 0x50]
	std	%f22, [%o0 + 0x58]
	std	%f24, [%o0 + 0x60]
	std	%f26, [%o0 + 0x68]
	std	%f28, [%o0 + 0x70]
	retl
	 std	%f30, [%o0 + 0x78]

	/* Thanks for Theo Deraadt and the authors of the Sprite/netbsd/openbsd
	 * code for pointing out this possible deadlock, while we save state
	 * above we could trap on the fsr store so our low level fpu trap
	 * code has to know how to deal with this.
	 */
fpsave_catch:
	b	fpsave_magic + 4
	 st	%fsr, [%o1]

fpsave_catch2:
	b	C_LABEL(fpsave) + 4
	 st	%fsr, [%o1]

	/* void fpload(unsigned long *fpregs, unsigned long *fsr); */

	.globl	C_LABEL(fpload)
C_LABEL(fpload):
	ldd	[%o0 + 0x00], %f0
	ldd	[%o0 + 0x08], %f2
	ldd	[%o0 + 0x10], %f4
	ldd	[%o0 + 0x18], %f6
	ldd	[%o0 + 0x20], %f8
	ldd	[%o0 + 0x28], %f10
	ldd	[%o0 + 0x30], %f12
	ldd	[%o0 + 0x38], %f14
	ldd	[%o0 + 0x40], %f16
	ldd	[%o0 + 0x48], %f18
	ldd	[%o0 + 0x50], %f20
	ldd	[%o0 + 0x58], %f22
	ldd	[%o0 + 0x60], %f24
	ldd	[%o0 + 0x68], %f26
	ldd	[%o0 + 0x70], %f28
	ldd	[%o0 + 0x78], %f30
	ld	[%o1], %fsr
	retl
	 nop

	.globl	C_LABEL(ndelay)
C_LABEL(ndelay):
	save	%sp, -STACKFRAME_SZ, %sp
	mov	%i0, %o0
	call	.umul
	 mov	5, %o1
	ba	delay_continue
	 nop

	.globl	C_LABEL(udelay)
C_LABEL(udelay):
	save	%sp, -STACKFRAME_SZ, %sp
	mov	%i0, %o0
	sethi	%hi(0x10c6), %o1
	call	.umul
	 or	%o1, %lo(0x10c6), %o1
delay_continue:
#ifndef CONFIG_SMP
	sethi	%hi(C_LABEL(loops_per_jiffy)), %o3
	call	.umul
	 ld	[%o3 + %lo(C_LABEL(loops_per_jiffy))], %o1
#else
	GET_PROCESSOR_OFFSET(o4, o2)
	set	C_LABEL(cpu_data), %o3
	call	.umul
	 ld	[%o3 + %o4], %o1
#endif
	call	.umul
	 mov	100, %o0

	cmp	%o0, 0x0
1:
	bne	1b
	 subcc	%o0, 1, %o0
	
	ret
	restore

	/* Handle a software breakpoint */
	/* We have to inform parent that child has stopped */
	.align 4
	.globl breakpoint_trap
breakpoint_trap:
	rd	%wim,%l3
	SAVE_ALL
	wr 	%l0, PSR_ET, %psr
	WRITE_PAUSE

	st	%i0, [%sp + STACKFRAME_SZ + PT_G0] ! for restarting syscalls
	call	C_LABEL(sparc_breakpoint)
	 add	%sp, STACKFRAME_SZ, %o0

	RESTORE_ALL

	.align	4
	.globl	C_LABEL(__handle_exception), flush_patch_exception
C_LABEL(__handle_exception):
flush_patch_exception:
	FLUSH_ALL_KERNEL_WINDOWS;
	ldd	[%o0], %o6
	jmpl	%o7 + 0xc, %g0			! see asm-sparc/processor.h
	 mov	1, %g1				! signal EFAULT condition

	.align	4
	.globl	C_LABEL(kill_user_windows), kuw_patch1_7win
	.globl	kuw_patch1
kuw_patch1_7win:	sll	%o3, 6, %o3

	/* No matter how much overhead this routine has in the worst
	 * case scenerio, it is several times better than taking the
	 * traps with the old method of just doing flush_user_windows().
	 */
C_LABEL(kill_user_windows):
	ld	[%g6 + AOFF_task_thread + AOFF_thread_uwinmask], %o0	! get current umask
	orcc	%g0, %o0, %g0			! if no bits set, we are done
	be	3f				! nothing to do
	 rd	%psr, %o5			! must clear interrupts
	or	%o5, PSR_PIL, %o4		! or else that could change
	wr	%o4, 0x0, %psr			! the uwinmask state
	WRITE_PAUSE				! burn them cycles
1:
	ld	[%g6 + AOFF_task_thread + AOFF_thread_uwinmask], %o0	! get consistant state
	orcc	%g0, %o0, %g0			! did an interrupt come in?
	be	4f				! yep, we are done
	 rd	%wim, %o3			! get current wim
	srl	%o3, 1, %o4			! simulate a save
kuw_patch1:
	sll	%o3, 7, %o3			! compute next wim
	or	%o4, %o3, %o3			! result
	andncc	%o0, %o3, %o0			! clean this bit in umask
	bne	kuw_patch1			! not done yet
	 srl	%o3, 1, %o4			! begin another save simulation
	wr	%o3, 0x0, %wim			! set the new wim
	st	%g0, [%g6 + AOFF_task_thread + AOFF_thread_uwinmask]	! clear uwinmask
4:
	wr	%o5, 0x0, %psr			! re-enable interrupts
	WRITE_PAUSE				! burn baby burn
3:
	retl					! return
	 st	%g0, [%g6 + AOFF_task_thread + AOFF_thread_w_saved]	! no windows saved

	.align	4
	.globl	C_LABEL(restore_current)
C_LABEL(restore_current):
	LOAD_CURRENT(g6, o0)
	retl
	 nop

#ifdef CONFIG_PCI
#include <asm/pcic.h>

	.align	4
	.globl	linux_trap_ipi15_pcic
linux_trap_ipi15_pcic:
	rd	%wim, %l3
	SAVE_ALL

	/*
	 * First deactivate NMI
	 * or we cannot drop ET, cannot get window spill traps.
	 * The busy loop is necessary because the PIO error
	 * sometimes does not go away quickly and we trap again.
	 */
	sethi	%hi(C_LABEL(pcic_regs)), %o1
	ld	[%o1 + %lo(C_LABEL(pcic_regs))], %o2

	! Get pending status for printouts later.
	ld	[%o2 + PCI_SYS_INT_PENDING], %o0

	mov	PCI_SYS_INT_PENDING_CLEAR_ALL, %o1
	stb	%o1, [%o2 + PCI_SYS_INT_PENDING_CLEAR]
1:
	ld	[%o2 + PCI_SYS_INT_PENDING], %o1
	andcc	%o1, ((PCI_SYS_INT_PENDING_PIO|PCI_SYS_INT_PENDING_PCI)>>24), %g0
	bne	1b
	 nop

	or	%l0, PSR_PIL, %l4
	wr	%l4, 0x0, %psr
	WRITE_PAUSE
	wr	%l4, PSR_ET, %psr
	WRITE_PAUSE

	call	C_LABEL(pcic_nmi)
	 add	%sp, STACKFRAME_SZ, %o1	! struct pt_regs *regs
	RESTORE_ALL

	.globl	C_LABEL(pcic_nmi_trap_patch)
C_LABEL(pcic_nmi_trap_patch):
	sethi	%hi(linux_trap_ipi15_pcic), %l3
	jmpl	%l3 + %lo(linux_trap_ipi15_pcic), %g0
	 rd	%psr, %l0
	.word	0

#endif /* CONFIG_PCI */

/* End of entry.S */
