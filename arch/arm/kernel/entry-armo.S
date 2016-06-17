/*
 *  linux/arch/arm/kernel/entry-armo.S
 *
 *  Copyright (C) 1995,1996,1997,1998 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Low-level vector interface routines
 *
 *  Design issues:
 *   - We have several modes that each vector can be called from,
 *     each with its own set of registers.  On entry to any vector,
 *     we *must* save the registers used in *that* mode.
 *
 *   - This code must be as fast as possible.
 *
 *  There are a few restrictions on the vectors:
 *   - the SWI vector cannot be called from *any* non-user mode
 *
 *   - the FP emulator is *never* called from *any* non-user mode undefined
 *     instruction.
 *
 *  Ok, so this file may be a mess, but its as efficient as possible while
 *  adhering to the above criteria.
 */
#include <linux/config.h>
#include "entry-header.S"

		.text

#ifdef IOC_BASE
/* IOC / IOMD based hardware */
		.equ	ioc_base_high, IOC_BASE & 0xff000000
		.equ	ioc_base_low, IOC_BASE & 0x00ff0000
		.macro	disable_fiq
		mov	r12, #ioc_base_high
		.if	ioc_base_low
		orr	r12, r12, #ioc_base_low
		.endif
		strb	r12, [r12, #0x38]	@ Disable FIQ register
		.endm

		.macro	get_irqnr_and_base, irqnr, base
		mov	r4, #ioc_base_high		@ point at IOC
		.if	ioc_base_low
		orr	r4, r4, #ioc_base_low
		.endif
		ldrb	\irqnr, [r4, #0x24]		@ get high priority first
		adr	\base, irq_prio_h
		teq	\irqnr, #0
		ldreqb	\irqnr, [r4, #0x14]		@ get low priority
		adreq	\base, irq_prio_l
		.endm

/*
 * Interrupt table (incorporates priority)
 */
		.macro	irq_prio_table
irq_prio_l:	.byte	 0, 0, 1, 0, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 4, 0, 1, 0, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
irq_prio_h:	.byte	 0, 8, 9, 8,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	12, 8, 9, 8,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	14,14,14,14,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	14,14,14,14,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.endm
#else
#error Unknown architecture
#endif

/*=============================================================================
 * For entry-common.S
 */

#if 0
/*
 * Uncomment these if you wish to get more debugging into about data aborts.
 */
#define FAULT_CODE_LDRSTRPOST	0x80
#define FAULT_CODE_LDRSTRPRE	0x40
#define FAULT_CODE_LDRSTRREG	0x20
#define FAULT_CODE_LDMSTM	0x10
#define FAULT_CODE_LDCSTC	0x08
#endif
#define FAULT_CODE_PREFETCH	0x04
#define FAULT_CODE_WRITE	0x02
#define FAULT_CODE_FORCECOW	0x01

#define SVC_SAVE_ALL				\
		str	sp, [sp, #-16]!		;\
		str	lr, [sp, #8]		;\
		str	lr, [sp, #4]		;\
		stmfd	sp!, {r0 - r12}		;\
		mov	r0, #-1			;\
		str	r0, [sp, #S_OLD_R0]	;\
		zero_fp

#define SVC_IRQ_SAVE_ALL			\
		str	sp, [sp, #-16]!		;\
		str	lr, [sp, #4]		;\
		ldr	lr, .LCirq		;\
		ldr	lr, [lr]		;\
		str	lr, [sp, #8]		;\
		stmfd	sp!, {r0 - r12}		;\
		mov	r0, #-1			;\
		str	r0, [sp, #S_OLD_R0]	;\
		zero_fp

#define SVC_RESTORE_ALL				\
		ldmfd	sp, {r0 - pc}^
		
/*=============================================================================
 * Undefined FIQs
 *-----------------------------------------------------------------------------
 */
_unexp_fiq:	ldr	sp, .LCfiq
		mov	r12, #IOC_BASE
		strb	r12, [r12, #0x38]	@ Disable FIQ register
		teqp	pc, #0x0c000003
		mov	r0, r0
		stmfd	sp!, {r0 - r3, ip, lr}
		adr	r0, Lfiqmsg
		bl	SYMBOL_NAME(printk)
		ldmfd	sp!, {r0 - r3, ip, lr}
		teqp	pc, #0x0c000001
		mov	r0, r0
		movs	pc, lr

Lfiqmsg:	.ascii	"*** Unexpected FIQ\n\0"
		.align

.LCfiq:		.word	__temp_fiq
.LCirq:		.word	__temp_irq

/*=============================================================================
 * Undefined instruction handler
 *-----------------------------------------------------------------------------
 * Handles floating point instructions
 */
vector_undefinstr:
		tst	lr,#3
		bne	__und_svc
		save_user_regs
		zero_fp
		teqp	pc, #I_BIT | MODE_SVC
.Lbug_undef:
		ldr	r4, .LC2
		ldr	pc, [r4]			@ Call FP module USR entry point

		.globl	SYMBOL_NAME(fpundefinstr)
SYMBOL_NAME(fpundefinstr):				@ Called by FP module on undefined instr
		mov	r0, lr
		mov	r1, sp
		teqp	pc, #MODE_SVC
		bl	SYMBOL_NAME(do_undefinstr)
		b	ret_from_exception		@ Normal FP exit

__und_svc:	SVC_SAVE_ALL				@ Non-user mode
		mask_pc	r0, lr
		and	r2, lr, #3
		sub	r0, r0, #4
		mov	r1, sp
		bl	SYMBOL_NAME(do_undefinstr)
		SVC_RESTORE_ALL

#if defined CONFIG_FPE_NWFPE || defined CONFIG_FPE_FASTFPE
		/* The FPE is always present */
		.equ	fpe_not_present, 0
#else
/* We get here if an undefined instruction happens and the floating
 * point emulator is not present.  If the offending instruction was
 * a WFS, we just perform a normal return as if we had emulated the
 * operation.  This is a hack to allow some basic userland binaries
 * to run so that the emulator module proper can be loaded. --philb
 */
fpe_not_present:
		adr	r10, wfs_mask_data
		ldmia	r10, {r4, r5, r6, r7, r8}
		ldr	r10, [sp, #S_PC]		@ Load PC
		sub	r10, r10, #4
		mask_pc	r10, r10
		ldrt	r10, [r10]			@ get instruction
		and	r5, r10, r5
		teq	r5, r4				@ Is it WFS?
		beq	ret_from_exception
		and	r5, r10, r8
		teq	r5, r6				@ Is it LDF/STF on sp or fp?
		teqne	r5, r7
		bne	fpundefinstr
		tst	r10, #0x00200000		@ Does it have WB
		beq	ret_from_exception
		and	r4, r10, #255			@ get offset
		and	r6, r10, #0x000f0000
		tst	r10, #0x00800000		@ +/-
		ldr	r5, [sp, r6, lsr #14]		@ Load reg
		rsbeq	r4, r4, #0
		add	r5, r5, r4, lsl #2
		str	r5, [sp, r6, lsr #14]		@ Save reg
		b	ret_from_exception

wfs_mask_data:	.word	0x0e200110			@ WFS/RFS
		.word	0x0fef0fff
		.word	0x0d0d0100			@ LDF [sp]/STF [sp]
		.word	0x0d0b0100			@ LDF [fp]/STF [fp]
		.word	0x0f0f0f00
#endif

.LC2:		.word	SYMBOL_NAME(fp_enter)

/*=============================================================================
 * Prefetch abort handler
 *-----------------------------------------------------------------------------
 */

vector_prefetch:
		sub	lr, lr, #4
		tst	lr, #3
		bne	__pabt_invalid
		save_user_regs
		teqp	pc, #0x00000003		@ NOT a problem - doesnt change mode
		mask_pc	r0, lr			@ Address of abort
		mov	r1, sp			@ Tasks registers
		bl	SYMBOL_NAME(do_PrefetchAbort)
		teq	r0, #0			@ If non-zero, we believe this abort..
		bne	ret_from_exception
#ifdef DEBUG_UNDEF
		adr	r0, t
		bl	SYMBOL_NAME(printk)
#endif
		ldr	lr, [sp,#S_PC]		@ program to test this on.  I think its
		b	.Lbug_undef		@ broken at the moment though!)

__pabt_invalid:	SVC_SAVE_ALL
		mov	r0, sp				@ Prefetch aborts are definitely *not*
		mov	r1, #BAD_PREFETCH		@ allowed in non-user modes.  We cant
		and	r2, lr, #3			@ recover from this problem.
		b	SYMBOL_NAME(bad_mode)

#ifdef DEBUG_UNDEF
t:		.ascii "*** undef ***\r\n\0"
		.align
#endif

/*=============================================================================
 * Address exception handler
 *-----------------------------------------------------------------------------
 * These aren't too critical.
 * (they're not supposed to happen).
 * In order to debug the reason for address exceptions in non-user modes,
 * we have to obtain all the registers so that we can see what's going on.
 */

vector_addrexcptn:
		sub	lr, lr, #8
		tst	lr, #3
		bne	Laddrexcptn_not_user
		save_user_regs
		teq	pc, #0x00000003
		mask_pc	r0, lr			@ Point to instruction
		mov	r1, sp			@ Point to registers
		mov	r2, #0x400
		mov	lr, pc
		bl	SYMBOL_NAME(do_excpt)
		b	ret_from_exception

Laddrexcptn_not_user:
		SVC_SAVE_ALL
		and	r2, lr, #3
		teq	r2, #3
		bne	Laddrexcptn_illegal_mode
		teqp	pc, #0x00000003		@ NOT a problem - doesnt change mode
		mask_pc	r0, lr
		mov	r1, sp
		orr	r2, r2, #0x400
		bl	SYMBOL_NAME(do_excpt)
		ldmia	sp, {r0 - lr}		@ I cant remember the reason I changed this...
		add	sp, sp, #15*4
		movs	pc, lr

Laddrexcptn_illegal_mode:
		mov	r0, sp
		str	lr, [sp, #-4]!
		orr	r1, r2, #0x0c000000
		teqp	r1, #0			@ change into mode (wont be user mode)
		mov	r0, r0
		mov	r1, r8			@ Any register from r8 - r14 can be banked
		mov	r2, r9
		mov	r3, r10
		mov	r4, r11
		mov	r5, r12
		mov	r6, r13
		mov	r7, r14
		teqp	pc, #0x04000003		@ back to svc
		mov	r0, r0
		stmfd	sp!, {r1-r7}
		ldmia	r0, {r0-r7}
		stmfd	sp!, {r0-r7}
		mov	r0, sp
		mov	r1, #BAD_ADDREXCPTN
		b	SYMBOL_NAME(bad_mode)

/*=============================================================================
 * Interrupt (IRQ) handler
 *-----------------------------------------------------------------------------
 * Note: if in user mode, then *no* kernel routine is running, so do not have
 *       to save svc lr
 * (r13 points to irq temp save area)
 */

vector_IRQ:	ldr	r13, .LCirq			@ I will leave this one in just in case...
		sub	lr, lr, #4
		str	lr, [r13]
		tst	lr, #3
		bne	__irq_svc
		teqp	pc, #0x08000003
		mov	r0, r0
		ldr	lr, .LCirq
		ldr	lr, [lr]
		save_user_regs

1:		get_irqnr_and_base r6, r5
		teq	r6, #0
		ldrneb	r0, [r5, r6]			@ get IRQ number
		movne	r1, sp
		@
		@ routine called with r0 = irq number, r1 = struct pt_regs *
		@
		adr	lr, 1b
		orr	lr, lr, #0x08000003		@ Force SVC
		bne	do_IRQ

		mov	why, #0
		get_current_task r5
		b	ret_to_user

		irq_prio_table

__irq_svc:	teqp	pc, #0x08000003
		mov	r0, r0
		SVC_IRQ_SAVE_ALL
                and	r2, lr, #3
		teq	r2, #3
		bne	__irq_invalid
1:		get_irqnr_and_base r6, r5
		teq	r6, #0
		ldrneb	r0, [r5, r6]			@ get IRQ number
		movne	r1, sp
		@
		@ routine called with r0 = irq number, r1 = struct pt_regs *
		@
		adr	lr, 1b
		orr	lr, lr, #0x08000003		@ Force SVC
		bne	do_IRQ				@ Returns to 1b
		SVC_RESTORE_ALL

__irq_invalid:	mov	r0, sp
		mov	r1, #BAD_IRQ
		b	SYMBOL_NAME(bad_mode)

/*=============================================================================
 * Data abort handler code
 *-----------------------------------------------------------------------------
 *
 * This handles both exceptions from user and SVC modes, computes the address
 *  range of the problem, and does any correction that is required.  It then
 *  calls the kernel data abort routine.
 *
 * This is where I wish that the ARM would tell you which address aborted.
 */

vector_data:	sub	lr, lr, #8		@ Correct lr
		tst	lr, #3
		bne	Ldata_not_user
		save_user_regs
		teqp	pc, #0x00000003		@ NOT a problem - doesnt change mode
		mask_pc	r0, lr
		bl	Ldata_do
		b	ret_from_exception

Ldata_not_user:
		SVC_SAVE_ALL
		and	r2, lr, #3
		teq	r2, #3
		bne	Ldata_illegal_mode
		tst	lr, #0x08000000
		teqeqp	pc, #0x00000003		@ NOT a problem - doesnt change mode
		mask_pc	r0, lr
		bl	Ldata_do
		SVC_RESTORE_ALL

Ldata_illegal_mode:
		mov	r0, sp
		mov	r1, #BAD_DATA
		b	SYMBOL_NAME(bad_mode)

Ldata_do:	mov	r3, sp
		ldr	r4, [r0]		@ Get instruction
		mov	r2, #0
		tst	r4, #1 << 20		@ Check to see if it is a write instruction
		orreq	r2, r2, #FAULT_CODE_WRITE @ Indicate write instruction
		mov	r1, r4, lsr #22		@ Now branch to the relevent processing routine
		and	r1, r1, #15 << 2
		add	pc, pc, r1
		movs	pc, lr
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_ldrstr_post	@ ldr	rd, [rn], #m
		b	Ldata_ldrstr_numindex	@ ldr	rd, [rn, #m]	@ RegVal
		b	Ldata_ldrstr_post	@ ldr	rd, [rn], rm
		b	Ldata_ldrstr_regindex	@ ldr	rd, [rn, rm]
		b	Ldata_ldmstm		@ ldm*a	rn, <rlist>
		b	Ldata_ldmstm		@ ldm*b	rn, <rlist>
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_ldrstr_post	@ ldc	rd, [rn], #m	@ Same as ldr	rd, [rn], #m
		b	Ldata_ldcstc_pre	@ ldc	rd, [rn, #m]
		b	Ldata_unknown
Ldata_unknown:	@ Part of jumptable
		mov	r0, r1
		mov	r1, r4
		mov	r2, r3
		b	baddataabort

Ldata_ldrstr_post:
		mov	r0, r4, lsr #14		@ Get Rn
		and	r0, r0, #15 << 2	@ Mask out reg.
		teq	r0, #15 << 2
		ldr	r0, [r3, r0]		@ Get register
		biceq	r0, r0, #PCMASK
		mov	r1, r0
#ifdef FAULT_CODE_LDRSTRPOST
		orr	r2, r2, #FAULT_CODE_LDRSTRPOST
#endif
		b	SYMBOL_NAME(do_DataAbort)

Ldata_ldrstr_numindex:
		mov	r0, r4, lsr #14		@ Get Rn
		and	r0, r0, #15 << 2	@ Mask out reg.
		teq	r0, #15 << 2
		ldr	r0, [r3, r0]		@ Get register
		mov	r1, r4, lsl #20
		biceq	r0, r0, #PCMASK
		tst	r4, #1 << 23
		addne	r0, r0, r1, lsr #20
		subeq	r0, r0, r1, lsr #20
		mov	r1, r0
#ifdef FAULT_CODE_LDRSTRPRE
		orr	r2, r2, #FAULT_CODE_LDRSTRPRE
#endif
		b	SYMBOL_NAME(do_DataAbort)

Ldata_ldrstr_regindex:
		mov	r0, r4, lsr #14		@ Get Rn
		and	r0, r0, #15 << 2	@ Mask out reg.
		teq	r0, #15 << 2
		ldr	r0, [r3, r0]		@ Get register
		and	r7, r4, #15
		biceq	r0, r0, #PCMASK
		teq	r7, #15			@ Check for PC
		ldr	r7, [r3, r7, lsl #2]	@ Get Rm
		and	r8, r4, #0x60		@ Get shift types
		biceq	r7, r7, #PCMASK
		mov	r9, r4, lsr #7		@ Get shift amount
		and	r9, r9, #31
		teq	r8, #0
		moveq	r7, r7, lsl r9
		teq	r8, #0x20		@ LSR shift
		moveq	r7, r7, lsr r9
		teq	r8, #0x40		@ ASR shift
		moveq	r7, r7, asr r9
		teq	r8, #0x60		@ ROR shift
		moveq	r7, r7, ror r9
		tst	r4, #1 << 23
		addne	r0, r0, r7
		subeq	r0, r0, r7		@ Apply correction
		mov	r1, r0
#ifdef FAULT_CODE_LDRSTRREG
		orr	r2, r2, #FAULT_CODE_LDRSTRREG
#endif
		b	SYMBOL_NAME(do_DataAbort)

Ldata_ldmstm:
		mov	r7, #0x11
		orr	r7, r7, r7, lsl #8
		and	r0, r4, r7
		and	r1, r4, r7, lsl #1
		add	r0, r0, r1, lsr #1
		and	r1, r4, r7, lsl #2
		add	r0, r0, r1, lsr #2
		and	r1, r4, r7, lsl #3
		add	r0, r0, r1, lsr #3
		add	r0, r0, r0, lsr #8
		add	r0, r0, r0, lsr #4
		and	r7, r0, #15		@ r7 = no. of registers to transfer.
		mov	r5, r4, lsr #14		@ Get Rn
		and	r5, r5, #15 << 2
		ldr	r0, [r3, r5]		@ Get reg
		eor	r6, r4, r4, lsl #2
		tst	r6, #1 << 23		@ Check inc/dec ^ writeback
		rsbeq	r7, r7, #0
		add	r7, r0, r7, lsl #2	@ Do correction (signed)
		subne	r1, r7, #1
		subeq	r1, r0, #1
		moveq	r0, r7
		tst	r4, #1 << 21		@ Check writeback
		strne	r7, [r3, r5]
		eor	r6, r4, r4, lsl #1
		tst	r6, #1 << 24		@ Check Pre/Post ^ inc/dec
		addeq	r0, r0, #4
		addeq	r1, r1, #4
		teq	r5, #15*4		@ CHECK FOR PC
		biceq	r1, r1, #PCMASK
		biceq	r0, r0, #PCMASK
#ifdef FAULT_CODE_LDMSTM
		orr	r2, r2, #FAULT_CODE_LDMSTM
#endif
		b	SYMBOL_NAME(do_DataAbort)

Ldata_ldcstc_pre:
		mov	r0, r4, lsr #14		@ Get Rn
		and	r0, r0, #15 << 2	@ Mask out reg.
		teq	r0, #15 << 2
		ldr	r0, [r3, r0]		@ Get register
		mov	r1, r4, lsl #24		@ Get offset
		biceq	r0, r0, #PCMASK
		tst	r4, #1 << 23
		addne	r0, r0, r1, lsr #24
		subeq	r0, r0, r1, lsr #24
		mov	r1, r0
#ifdef FAULT_CODE_LDCSTC
		orr	r2, r2, #FAULT_CODE_LDCSTC
#endif
		b	SYMBOL_NAME(do_DataAbort)


/*
 * This is the return code to user mode for abort handlers
 */
ENTRY(ret_from_exception)
		get_current_task tsk
		mov	why, #0
		b	ret_to_user

		.data
ENTRY(fp_enter)
		.word	fpe_not_present
		.text
/*
 * Register switch for older 26-bit only ARMs
 */
ENTRY(__switch_to)
		stmfd	sp!, {r4 - sl, fp, lr}		@ Store most regs on stack
		str	sp, [r0, #TSS_SAVE]		@ Save sp_SVC
		ldr	sp, [r1, #TSS_SAVE]		@ Get saved sp_SVC
		ldmfd	sp!, {r4 - sl, fp, pc}^		@ Load all regs saved previously

/*
 *=============================================================================
 *		Low-level interface code
 *-----------------------------------------------------------------------------
 *		Trap initialisation
 *-----------------------------------------------------------------------------
 *
 * Note - FIQ code has changed.  The default is a couple of words in 0x1c, 0x20
 * that call _unexp_fiq.  Nowever, we now copy the FIQ routine to 0x1c (removes
 * some excess cycles).
 *
 * What we need to put into 0-0x1c are branches to branch to the kernel.
 */

		.section ".text.init",#alloc,#execinstr

.Ljump_addresses:
		swi	SYS_ERROR0
		.word	vector_undefinstr	- 12
		.word	vector_swi		- 16
		.word	vector_prefetch		- 20
		.word	vector_data		- 24
		.word	vector_addrexcptn	- 28
		.word	vector_IRQ		- 32
		.word	_unexp_fiq		- 36
		b	. + 8
/*
 * initialise the trap system
 */
ENTRY(__trap_init)
		stmfd	sp!, {r4 - r7, lr}
		adr	r1, .Ljump_addresses
		ldmia	r1, {r1 - r7, ip, lr}
		orr	r2, lr, r2, lsr #2
		orr	r3, lr, r3, lsr #2
		orr	r4, lr, r4, lsr #2
		orr	r5, lr, r5, lsr #2
		orr	r6, lr, r6, lsr #2
		orr	r7, lr, r7, lsr #2
		orr	ip, lr, ip, lsr #2
		mov	r0, #0
		stmia	r0, {r1 - r7, ip}
		ldmfd	sp!, {r4 - r7, pc}^

		.bss
__temp_irq:	.space	4				@ saved lr_irq
__temp_fiq:	.space	128
