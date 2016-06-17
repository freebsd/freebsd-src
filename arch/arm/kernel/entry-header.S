#include <linux/config.h> /* for CONFIG_ARCH_xxxx */
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/errno.h>
#include <asm/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/proc-fns.h>

#ifndef MODE_SVC
#define MODE_SVC 0x13
#endif

		.macro	zero_fp
#ifdef CONFIG_FRAME_POINTER
		mov	fp, #0
#endif
		.endm

		.text

@ Bad Abort numbers
@ -----------------
@
#define BAD_PREFETCH	0
#define BAD_DATA	1
#define BAD_ADDREXCPTN	2
#define BAD_IRQ		3
#define BAD_UNDEFINSTR	4

#define PT_TRACESYS	0x00000002

@ OS version number used in SWIs
@  RISC OS is 0
@  RISC iX is 8
@
#define OS_NUMBER	9
#define ARMSWI_OFFSET	0x000f0000

@
@ Stack format (ensured by USER_* and SVC_*)
@
#ifdef CONFIG_CPU_32
#define S_FRAME_SIZE	72
#define S_OLD_R0	68
#define S_PSR		64
#else
#define S_FRAME_SIZE	68
#define S_OLD_R0	64
#define S_PSR		60
#endif

#define S_PC		60
#define S_LR		56
#define S_SP		52
#define S_IP		48
#define S_FP		44
#define S_R10		40
#define S_R9		36
#define S_R8		32
#define S_R7		28
#define S_R6		24
#define S_R5		20
#define S_R4		16
#define S_R3		12
#define S_R2		8
#define S_R1		4
#define S_R0		0
#define S_OFF		8

#ifdef CONFIG_CPU_32
	.macro	set_cpsr_c, reg, mode
#if 1
	/* broken binutils */
	mov	\reg, \mode
	msr	cpsr_c, \reg
#else
	msr	cpsr_c, \mode
#endif
	.endm

	.macro	disable_irq, temp
	set_cpsr_c \temp, #I_BIT | MODE_SVC
	.endm

	.macro	enable_irq, temp
	set_cpsr_c \temp, #MODE_SVC
	.endm

		.macro	save_user_regs
		sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ Calling r0 - r12
		add	r8, sp, #S_PC
		stmdb	r8, {sp, lr}^			@ Calling sp, lr
		mrs	r8, spsr			@ called from non-FIQ mode, so ok.
		str	lr, [sp, #S_PC]			@ Save calling PC
		str	r8, [sp, #S_PSR]		@ Save CPSR
		str	r0, [sp, #S_OLD_R0]		@ Save OLD_R0
		.endm

/*
 * Must be called with IRQs already disabled.
 */
		.macro	restore_user_regs
		ldr	r1, [sp, #S_PSR]		@ Get calling cpsr
		ldr	lr, [sp, #S_PC]!		@ Get PC
		msr	spsr, r1			@ save in spsr_svc
		ldmdb	sp, {r0 - lr}^			@ Get calling r0 - lr
		mov	r0, r0
		add	sp, sp, #S_FRAME_SIZE - S_PC
		movs	pc, lr				@ return & move spsr_svc into cpsr
		.endm

/*
 * Must be called with IRQs already disabled.
 */
		.macro	fast_restore_user_regs
		ldr	r1, [sp, #S_OFF + S_PSR]	@ get calling cpsr
		ldr	lr, [sp, #S_OFF + S_PC]!	@ get pc
 		msr	spsr, r1			@ save in spsr_svc
		ldmdb	sp, {r1 - lr}^			@ get calling r1 - lr
		mov	r0, r0
		add	sp, sp, #S_FRAME_SIZE - S_PC
		movs	pc, lr				@ return & move spsr_svc into cpsr
		.endm

		.macro	mask_pc, rd, rm
		.endm

		.macro	get_current_task, rd
		mov	\rd, sp, lsr #13
		mov	\rd, \rd, lsl #13
		.endm

		/*
		 * Like adr, but force SVC mode (if required)
		 */
		.macro	adrsvc, cond, reg, label
		adr\cond	\reg, \label
		.endm

		.macro	alignment_trap, rbase, rtemp, sym
#ifdef CONFIG_ALIGNMENT_TRAP
#define OFF_CR_ALIGNMENT(x)	cr_alignment - x

		ldr	\rtemp, [\rbase, #OFF_CR_ALIGNMENT(\sym)]
		mcr	p15, 0, \rtemp, c1, c0
#endif
		.endm

#else
		.macro	save_user_regs
		sub	sp, sp, #S_FRAME_SIZE
		str	r0, [sp, #S_OLD_R0]
		str	lr, [sp, #S_PC]
		stmia	sp, {r0 - lr}^
		mov	r0, r0
		.endm

		.macro	restore_user_regs
		ldmia	sp, {r0 - lr}^
		mov	r0, r0
		ldr	lr, [sp, #S_PC]
		add	sp, sp, #S_FRAME_SIZE
		movs	pc, lr
		.endm

		.macro	fast_restore_user_regs
		add	sp, sp, #S_OFF + S_PC
		ldmdb	sp, {r1 - lr}^
		mov	r0, r0
		ldr	lr, [sp], #S_FRAME_SIZE - S_PC
		movs	pc, lr
		.endm

		.macro	mask_pc, rd, rm
		bic	\rd, \rm, #PCMASK
		.endm

		.macro	disable_irq, temp
		teqp	pc, #0x08000003
		.endm

		.macro	enable_irq, temp
		teqp	pc, #0x00000003
		.endm

		.macro	initialise_traps_extra
		.endm

		.macro	get_current_task, rd
		mov	\rd, sp, lsr #13
		mov	\rd, \rd, lsl #13
		.endm

		/*
		 * Like adr, but force SVC mode (if required)
		 */
		.macro	adrsvc, cond, reg, label
		adr\cond	\reg, \label
		orr\cond	\reg, \reg, #0x08000003
		.endm

#endif


/*
 * These are the registers used in the syscall handler, and allow us to
 * have in theory up to 7 arguments to a function - r0 to r6.
 *
 * r7 is reserved for the system call number for thumb mode.
 *
 * Note that tbl == why is intentional.
 *
 * We must set at least "tsk" and "why" when calling ret_with_reschedule.
 */
scno	.req	r7				@ syscall number
tbl	.req	r8				@ syscall table pointer
why	.req	r8				@ Linux syscall (!= 0)
tsk	.req	r9				@ current task

/*
 * Get the system call number.
 */
	.macro	get_scno
#ifdef CONFIG_ARM_THUMB
	tst	r8, #T_BIT		@ this is SPSR from save_user_regs
	addne	scno, r7, #OS_NUMBER << 20 @ put OS number in
	ldreq	scno, [lr, #-4]

#else
	mask_pc	lr, lr
	ldr	scno, [lr, #-4]		@ get SWI instruction
#endif
	.endm

