/*-
 * Copyright 2004-2014 Olivier Houchard <cognet@FreeBSD.org>
 * Copyright 2012-2014 Ian Lepore <ian@FreeBSD.org>
 * Copyright 2013-2014 Andrew Turner <andrew@FreeBSD.org>
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "assym.s"
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/asmacros.h>
#include <machine/armreg.h>
#include <machine/sysreg.h>
#include <machine/cpuconf.h>
#include <machine/pte.h>

__FBSDID("$FreeBSD$");

#ifndef ARM_NEW_PMAP
#define	PTE1_OFFSET	L1_S_OFFSET
#define	PTE1_SHIFT	L1_S_SHIFT
#define	PTE1_SIZE	L1_S_SIZE
#endif

#if __ARM_ARCH >= 7
#if defined(__ARM_ARCH_7VE__) || defined(__clang__)
/*
 * HYP support is in bintuils >= 2.21 and gcc >= 4.9 defines __ARM_ARCH_7VE__
 * when enabled. llvm >= 3.6 supports it too.
 */
.arch_extension virt
#define	MSR_ELR_HYP(regnum)	msr	elr_hyp, lr
#define	ERET	eret
#else
#define MSR_ELR_HYP(regnum) .word (0xe12ef300 | regnum)
#define ERET .word 0xe160006e
#endif
#endif /* __ARM_ARCH >= 7 */

/* A small statically-allocated stack used only during initarm() and AP startup. */
#define	INIT_ARM_STACK_SIZE	2048

	.text
	.align	2

#if __ARM_ARCH >= 7
#define	LEAVE_HYP							\
	/* Leave HYP mode */						;\
	mrs	r0, cpsr						;\
	and	r0, r0, #(PSR_MODE)   /* Mode is in the low 5 bits of CPSR */ ;\
	teq	r0, #(PSR_HYP32_MODE) /* Hyp Mode? */			;\
	bne	1f							;\
	/* Ensure that IRQ, FIQ and Aborts will be disabled after eret */ ;\
	mrs	r0, spsr						;\
	orr	r0, r0, #(PSR_I | PSR_F | PSR_A)			;\
	msr	spsr, r0						;\
	/* Exit hypervisor mode */					;\
	adr	lr, 1f							;\
	MSR_ELR_HYP(14)							;\
	ERET								;\
1:
#else
#define	LEAVE_HYP
#endif /* __ARM_ARCH >= 7 */

/*
 * On entry for FreeBSD boot ABI:
 *	r0 - metadata pointer or 0 (boothowto on AT91's boot2)
 *	r1 - if (r0 == 0) then metadata pointer
 * On entry for Linux boot ABI:
 *	r0 - 0
 *	r1 - machine type (passed as arg2 to initarm)
 *	r2 - Pointer to a tagged list or dtb image (phys addr) (passed as arg1 initarm)
 *
 * For both types of boot we gather up the args, put them in a struct arm_boot_params
 * structure and pass that to initarm.
 */
	.globl	btext
btext:
ASENTRY_NP(_start)
	STOP_UNWINDING		/* Can't unwind into the bootloader! */

	/* Make sure interrupts are disabled. */
	cpsid	ifa

	mov	r8, r0		/* 0 or boot mode from boot2 */
	mov	r9, r1		/* Save Machine type */
	mov	r10, r2		/* Save meta data */
	mov	r11, r3		/* Future expansion */

	LEAVE_HYP

	/*
	 * Check whether data cache is enabled.  If it is, then we know
	 * current tags are valid (not power-on garbage values) and there
	 * might be dirty lines that need cleaning.  Disable cache to prevent
	 * new lines being allocated, then call wbinv_poc_all to clean it.
	 */
	mrc	CP15_SCTLR(r7)
	tst	r7, #CPU_CONTROL_DC_ENABLE
	blne	dcache_wbinv_poc_all

	/* ! Do not write to memory between wbinv and disabling cache ! */

	/*
	 * Now there are no dirty lines, but there may still be lines marked
	 * valid.  Disable all caches and the MMU, and invalidate everything
	 * before setting up new page tables and re-enabling the mmu.
	 */
1:
	bic	r7, #CPU_CONTROL_DC_ENABLE
	bic	r7, #CPU_CONTROL_MMU_ENABLE
	bic	r7, #CPU_CONTROL_IC_ENABLE
	bic	r7, #CPU_CONTROL_UNAL_ENABLE
	bic	r7, #CPU_CONTROL_BPRD_ENABLE
	bic	r7, #CPU_CONTROL_SW_ENABLE
	orr	r7, #CPU_CONTROL_AFLT_ENABLE
	orr	r7, #CPU_CONTROL_VECRELOC
	mcr	CP15_SCTLR(r7)
	ISB
	bl	dcache_inv_poc_all
	mcr	CP15_ICIALLU
	ISB

	/*
	 * Build page table from scratch.
	 */

	/* Calculate the physical address of the startup pagetable. */
	adr	r0, Lpagetable
	bl	translate_va_to_pa

	/*
	 * Map PA == VA
	 */
	/* Find the start kernels load address */
	adr	r5, _start
	ldr	r2, =(PTE1_OFFSET)
	bic	r5, r2
	mov	r1, r5
	mov	r2, r5
	/* Map 64MiB, preserved over calls to build_pagetables */
	mov	r3, #64
	bl	build_pagetables

	/* Create the kernel map to jump to */
	mov	r1, r5
	ldr	r2, =(KERNVIRTADDR)
	bl	build_pagetables

#if defined(SOCDEV_PA) && defined(SOCDEV_VA)
	/* Create the custom map used for early_printf(). */
	ldr	r1, =SOCDEV_PA
	ldr	r2, =SOCDEV_VA
	bl	build_pagetables
#endif
	bl	init_mmu

	/* Switch to virtual addresses. */
	ldr	pc, =1f
1:

	/* Setup stack, clear BSS */
	ldr	r1, =.Lstart
	ldmia	r1, {r1, r2, sp}	/* Set initial stack and */
	add	sp, sp,	#INIT_ARM_STACK_SIZE
	sub	r2, r2, r1		/* get zero init data */
	mov	r3, #0
2:
	str	r3, [r1], #0x0004	/* get zero init data */
	subs	r2, r2, #4
	bgt	2b

	mov	r1, #28			/* loader info size is 28 bytes also second arg */
	subs	sp, sp, r1		/* allocate arm_boot_params struct on stack */
	mov	r0, sp			/* loader info pointer is first arg */
	bic	sp, sp, #7		/* align stack to 8 bytes */
	str	r1, [r0]		/* Store length of loader info */
	str	r8, [r0, #4]		/* Store r0 from boot loader */
	str	r9, [r0, #8]		/* Store r1 from boot loader */
	str	r10, [r0, #12]		/* store r2 from boot loader */
	str	r11, [r0, #16]		/* store r3 from boot loader */
	str	r5, [r0, #20]		/* store the physical address */
	adr	r4, Lpagetable		/* load the pagetable address */
	ldr	r5, [r4, #4]
	str	r5, [r0, #24]		/* store the pagetable address */
	mov	fp, #0			/* trace back starts here */
	bl	_C_LABEL(initarm)	/* Off we go */

	/* init arm will return the new stack pointer. */
	mov	sp, r0

	bl	_C_LABEL(mi_startup)	/* call mi_startup()! */

	ldr	r0, =.Lmainreturned
	b	_C_LABEL(panic)
	/* NOTREACHED */
END(_start)

#define VA_TO_PA_POINTER(name, table)	 \
name:					;\
	.word	.			;\
	.word	table

/*
 * Returns the physical address of a magic va to pa pointer.
 * r0     - The pagetable data pointer. This must be built using the
 *          VA_TO_PA_POINTER macro.
 *          e.g.
 *            VA_TO_PA_POINTER(Lpagetable, pagetable)
 *            ...
 *            adr  r0, Lpagetable
 *            bl   translate_va_to_pa
 *            r0 will now contain the physical address of pagetable
 * r1, r2 - Trashed
 */
translate_va_to_pa:
	ldr	r1, [r0]
	sub	r2, r1, r0
	/* At this point: r2 = VA - PA */

	/*
	 * Find the physical address of the table. After these two
	 * instructions:
	 * r1 = va(pagetable)
	 *
	 * r0 = va(pagetable) - (VA - PA)
	 *    = va(pagetable) - VA + PA
	 *    = pa(pagetable)
	 */
	ldr	r1, [r0, #4]
	sub	r0, r1, r2
	mov	pc, lr

/*
 * Init MMU
 * r0 - the table base address
 */

ASENTRY_NP(init_mmu)

	/* Setup TLB and MMU registers */
	mcr	CP15_TTBR0(r0)		/* Set TTB */
	mov	r0, #0
	mcr	CP15_CONTEXTIDR(r0)	/* Set ASID to 0 */

	/* Set the Domain Access register */
	mov	r0, #((DOMAIN_CLIENT <<	(PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT)
	mcr	CP15_DACR(r0)

#ifdef ARM_NEW_PMAP
	/*
	 * Set TEX remap registers
	 *  - All is set to uncacheable memory
	 */
	ldr	r0, =0xAAAAA
	mcr	CP15_PRRR(r0)
	mov	r0, #0
	mcr	CP15_NMRR(r0)
#endif
	mcr	CP15_TLBIALL		/* Flush TLB */
	DSB
	ISB

	/* Enable MMU */
	mrc	CP15_SCTLR(r0)
	orr	r0, r0,	#CPU_CONTROL_MMU_ENABLE
	orr	r0, r0,	#CPU_CONTROL_V6_EXTPAGE
#ifdef ARM_NEW_PMAP
	orr	r0, r0,	#CPU_CONTROL_TR_ENABLE
#endif
	orr	r0, r0,	#CPU_CONTROL_AF_ENABLE
	mcr	CP15_SCTLR(r0)
	DSB
	ISB
	mcr	CP15_TLBIALL		/* Flush TLB */
	mcr	CP15_BPIALL		/* Flush Branch predictor */
	ISB
	mov	pc, lr
END(init_mmu)


/*
 * Init SMP coherent mode, enable caching and switch to final MMU table.
 * Called with disabled caches
 * r0 - The table base address
 * r1 - clear bits for aux register
 * r2 - set bits for aux register
 */
ASENTRY_NP(reinit_mmu)
	push	{r4-r11, lr}
	mov	r4, r0
	mov	r5, r1
	mov	r6, r2

	/* !! Be very paranoid here !! */
	/* !! We cannot write single bit here !! */

#if 0	/* XXX writeback shouldn't be necessary */
	/* Write back and invalidate all integrated caches */
	bl 	dcache_wbinv_poc_all
#else
	bl	dcache_inv_pou_all
#endif
	mcr	CP15_ICIALLU
	ISB

	/* Set auxiliary register */
	mrc	CP15_ACTLR(r7)
	bic	r8, r7, r5		/* Mask bits */
	eor 	r8, r8, r6		/* Set bits */
	teq 	r7, r8
	mcrne 	CP15_ACTLR(r8)
	ISB

	/* Enable caches. */
	mrc	CP15_SCTLR(r7)
	orr	r7, #CPU_CONTROL_DC_ENABLE
	orr	r7, #CPU_CONTROL_IC_ENABLE
	orr	r7, #CPU_CONTROL_BPRD_ENABLE
	mcr	CP15_SCTLR(r7)
	DSB

	mcr	CP15_TTBR0(r4)		/* Set new TTB */
	DSB
	ISB

	/* Flush all TLBs */
	mcr	CP15_TLBIALL
	DSB
	ISB

#if 0 /* XXX writeback shouldn't be necessary */
	/* Write back and invalidate all integrated caches */
	bl 	dcache_wbinv_poc_all
#else
	bl	dcache_inv_pou_all
#endif
	mcr	CP15_ICIALLU
	ISB

	pop	{r4-r11, pc}
END(reinit_mmu)


/*
 * Builds the page table
 * r0 - The table base address
 * r1 - The physical address (trashed)
 * r2 - The virtual address (trashed)
 * r3 - The number of 1MiB sections
 * r4 - Trashed
 *
 * Addresses must be 1MiB aligned
 */
build_pagetables:
	/* Set the required page attributed */
#if defined(ARM_NEW_PMAP)
	ldr	r4, =PTE1_V|PTE1_A|PTE1_AP_KRW|TEX1_CLASS_0
#elif defined(SMP)
	ldr	r4, =(L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW)|L1_SHARED)
#else
	ldr	r4, =(L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))
#endif
	orr	r1, r4

	/* Move the virtual address to the correct bit location */
	lsr	r2, #(PTE1_SHIFT - 2)

	mov	r4, r3
1:
	str	r1, [r0, r2]
	add	r2, r2, #4
	add	r1, r1, #(PTE1_SIZE)
	adds	r4, r4, #-1
	bhi	1b

	mov	pc, lr

VA_TO_PA_POINTER(Lpagetable, boot_pt1)


.Lstart:
	.word	_edata			/* Note that these three items are */
	.word	_ebss			/* loaded with a single ldmia and */
	.word	svcstk			/* must remain in order together. */

.Lmainreturned:
	.asciz	"main() returned"
	.align	2

	.bss
svcstk:
	.space	INIT_ARM_STACK_SIZE * MAXCPU

/*
 * Memory for the initial pagetable. We are unable to place this in
 * the bss as this will be cleared after the table is loaded.
 */
	.section ".init_pagetable"
	.align	14 /* 16KiB aligned */
	.globl	boot_pt1
boot_pt1:
	.space	L1_TABLE_SIZE

	.text
	.align	2

.Lcpufuncs:
	.word	_C_LABEL(cpufuncs)

#if defined(SMP)

ASENTRY_NP(mpentry)
	/* Make sure interrupts are disabled. */
	cpsid	ifa

	LEAVE_HYP

	/* Setup core, disable all caches. */
	mrc	CP15_SCTLR(r0)
	bic	r0, #CPU_CONTROL_MMU_ENABLE
	bic	r0, #CPU_CONTROL_DC_ENABLE
	bic	r0, #CPU_CONTROL_IC_ENABLE
	bic	r0, #CPU_CONTROL_UNAL_ENABLE
	bic	r0, #CPU_CONTROL_BPRD_ENABLE
	bic	r0, #CPU_CONTROL_SW_ENABLE
	orr	r0, #CPU_CONTROL_AFLT_ENABLE
	orr	r0, #CPU_CONTROL_VECRELOC
	mcr	CP15_SCTLR(r0)
	ISB

	/* Invalidate L1 cache I+D cache */
	bl	dcache_inv_pou_all
	mcr	CP15_ICIALLU
	ISB

	/* Find the delta between VA and PA */
	adr	r0, Lpagetable
	bl	translate_va_to_pa

	bl	init_mmu

	adr	r1, .Lstart+8		/* Get initstack pointer from */
	ldr	sp, [r1]		/* startup data. */
	mrc	CP15_MPIDR(r0)		/* Get processor id number. */
	and	r0, r0,	#0x0f
	mov	r1, #INIT_ARM_STACK_SIZE
	mul	r2, r1,	r0		/* Point sp to initstack */
	add	sp, sp,	r2		/* area for this processor. */

	/* Switch to virtual addresses. */
	ldr	pc, =1f
1:
	mov	fp, #0			/* trace back starts here */
	bl	_C_LABEL(init_secondary)/* Off we go, cpu id in r0. */

	adr	r0, .Lmpreturned
	b	_C_LABEL(panic)
	/* NOTREACHED */
END(mpentry)

.Lmpreturned:
	.asciz	"init_secondary() returned"
	.align	2
#endif

ENTRY_NP(cpu_halt)

	/* XXX re-implement !!! */
	cpsid	ifa
	bl	dcache_wbinv_poc_all

	ldr	r4, .Lcpu_reset_address
	ldr	r4, [r4]
	teq	r4, #0
	movne	pc, r4
1:
	WFI
	b	1b

	/*
	 * _cpu_reset_address contains the address to branch to, to complete
	 * the cpu reset after turning the MMU off
	 * This variable is provided by the hardware specific code
	 */
.Lcpu_reset_address:
	.word	_C_LABEL(cpu_reset_address)
END(cpu_halt)


/*
 * setjump + longjmp
 */
ENTRY(setjmp)
	stmia	r0, {r4-r14}
	mov	r0, #0x00000000
	RET
END(setjmp)

ENTRY(longjmp)
	ldmia	r0, {r4-r14}
	mov	r0, #0x00000001
	RET
END(longjmp)

	.data
	.global	_C_LABEL(esym)
_C_LABEL(esym):	.word	_C_LABEL(end)

ENTRY_NP(abort)
	b	_C_LABEL(abort)
END(abort)

ENTRY_NP(sigcode)
	mov	r0, sp
	add	r0, r0, #SIGF_UC

	/*
	 * Call the sigreturn system call.
	 *
	 * We have to load r7 manually rather than using
	 * "ldr r7, =SYS_sigreturn" to ensure the value of szsigcode is
	 * correct. Using the alternative places esigcode at the address
	 * of the data rather than the address one past the data.
	 */

	ldr	r7, [pc, #12]	/* Load SYS_sigreturn */
	swi	SYS_sigreturn

	/* Well if that failed we better exit quick ! */

	ldr	r7, [pc, #8]	/* Load SYS_exit */
	swi	SYS_exit

	/* Branch back to retry SYS_sigreturn */
	b	. - 16
END(sigcode)
	.word	SYS_sigreturn
	.word	SYS_exit

	.align	2
	.global _C_LABEL(esigcode)
		_C_LABEL(esigcode):

	.data
	.global szsigcode
szsigcode:
	.long esigcode-sigcode

/* End of locore.S */
