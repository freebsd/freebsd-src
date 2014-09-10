/*	$NetBSD: locore.S,v 1.14 2003/04/20 16:21:40 thorpej Exp $	*/

/*-
 * Copyright 2011 Semihalf
 * Copyright (C) 1994-1997 Mark Brinicombe
 * Copyright (C) 1994 Brini
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of Brini may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "assym.s"
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/cpuconf.h>
#include <machine/pte.h>

__FBSDID("$FreeBSD$");

/* What size should this really be ? It is only used by initarm() */
#define INIT_ARM_STACK_SIZE	(2048 * 4)

#define	CPWAIT_BRANCH							 \
	sub	pc, pc, #4

#define	CPWAIT(tmp)							 \
	mrc	p15, 0, tmp, c2, c0, 0	/* arbitrary read of CP15 */	;\
	mov	tmp, tmp		/* wait for it to complete */	;\
	CPWAIT_BRANCH			/* branch to next insn */

/*
 * This is for kvm_mkdb, and should be the address of the beginning
 * of the kernel text segment (not necessarily the same as kernbase).
 */
	.text
	.align	0
.globl kernbase
.set kernbase,KERNBASE
.globl physaddr
.set physaddr,PHYSADDR

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

	mov	r9, r0		/* 0 or boot mode from boot2 */
	mov	r8, r1		/* Save Machine type */
	mov	ip, r2		/* Save meta data */
	mov	fp, r3		/* Future expansion */

	/* Make sure interrupts are disabled. */
	mrs	r7, cpsr
	orr	r7, r7, #(PSR_I | PSR_F)
	msr	cpsr_c, r7

#if defined (FLASHADDR) && defined(LOADERRAMADDR)
	/* Check if we're running from flash. */
	ldr	r7, =FLASHADDR
	/*
	 * If we're running with MMU disabled, test against the
	 * physical address instead.
	 */
	mrc     p15, 0, r2, c1, c0, 0
	ands	r2, r2, #CPU_CONTROL_MMU_ENABLE
	ldreq	r6, =PHYSADDR
	ldrne	r6, =LOADERRAMADDR
	cmp	r7, r6
	bls 	flash_lower
	cmp	r7, pc
	bhi	from_ram
	b	do_copy
	
flash_lower:
	cmp	r6, pc
	bls	from_ram
do_copy:
	ldr	r7, =KERNBASE
	adr	r1, _start
	ldr	r0, Lreal_start
	ldr	r2, Lend
	sub	r2, r2, r0
	sub	r0, r0, r7
	add	r0, r0, r6
	mov	r4, r0
	bl	memcpy
	ldr	r0, Lram_offset
	add	pc, r4, r0
Lram_offset:	.word from_ram-_C_LABEL(_start)
from_ram:
	nop
#endif

disable_mmu:
	/* Disable MMU for a while */
	mrc     p15, 0, r2, c1, c0, 0
	bic	r2, r2, #(CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_DC_ENABLE |\
	    CPU_CONTROL_WBUF_ENABLE)
	bic	r2, r2, #(CPU_CONTROL_IC_ENABLE)
	bic	r2, r2, #(CPU_CONTROL_BPRD_ENABLE)
	mcr     p15, 0, r2, c1, c0, 0

	nop
	nop
	nop
	CPWAIT(r0)

Lunmapped:
	/*
	 * Build page table from scratch.
	 */

	/* Find the delta between VA and PA */
	adr	r0, Lpagetable
	bl	translate_va_to_pa

#ifndef _ARM_ARCH_6
	/*
	 * Some of the older ports (the various XScale, mostly) assume
	 * that the memory before the kernel is mapped, and use it for
	 * the various stacks, page tables, etc. For those CPUs, map the 
	 * 64 first MB of RAM, as it used to be. 
	 */
	/*
	 * Map PA == VA
	 */    
	ldr     r5, =PHYSADDR
	mov     r1, r5
	mov     r2, r5
	/* Map 64MiB, preserved over calls to build_pagetables */
	mov     r3, #64
	bl      build_pagetables
	
	/* Create the kernel map to jump to */
	mov     r1, r5
	ldr     r2, =(KERNBASE)
	bl      build_pagetables
	ldr	r5, =(KERNPHYSADDR)
#else
	/*
	 * Map PA == VA
	 */    
	/* Find the start kernels load address */
	adr	r5, _start
	ldr	r2, =(L1_S_OFFSET)
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
#endif
	
#if defined(SOCDEV_PA) && defined(SOCDEV_VA)
	/* Create the custom map */
	ldr	r1, =SOCDEV_PA
	ldr	r2, =SOCDEV_VA
	bl	build_pagetables
#endif

#if defined(SMP)
	orr 	r0, r0, #2		/* Set TTB shared memory flag */
#endif
	mcr	p15, 0, r0, c2, c0, 0	/* Set TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* Flush TLB */

#if defined(CPU_ARM1136) || defined(CPU_ARM1176) || defined(CPU_CORTEXA) || defined(CPU_MV_PJ4B) || defined(CPU_KRAIT)
	mov	r0, #0
	mcr	p15, 0, r0, c13, c0, 1	/* Set ASID to 0 */
#endif

	/* Set the Domain Access register.  Very important! */
	mov     r0, #((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT)
	mcr	p15, 0, r0, c3, c0, 0
	/* 
	 * Enable MMU.
	 * On armv6 enable extended page tables, and set alignment checking
	 * to modulo-4 (CPU_CONTROL_UNAL_ENABLE) for the ldrd/strd
	 * instructions emitted by clang.
	 */
	mrc	p15, 0, r0, c1, c0, 0
#ifdef _ARM_ARCH_6
	orr	r0, r0, #(CPU_CONTROL_V6_EXTPAGE | CPU_CONTROL_UNAL_ENABLE)
	orr	r0, r0, #(CPU_CONTROL_AFLT_ENABLE)
	orr	r0, r0, #(CPU_CONTROL_AF_ENABLE)
#endif
	orr	r0, r0, #(CPU_CONTROL_MMU_ENABLE)
	mcr	p15, 0, r0, c1, c0, 0
	nop
	nop
	nop
	CPWAIT(r0)

mmu_done:
	nop
	adr	r1, .Lstart
	ldmia	r1, {r1, r2, sp}	/* Set initial stack and */
	sub	r2, r2, r1		/* get zero init data */
	mov	r3, #0
.L1:
	str	r3, [r1], #0x0004	/* get zero init data */
	subs	r2, r2, #4
	bgt	.L1
	ldr	pc, .Lvirt_done

virt_done:
	mov	r1, #28			/* loader info size is 28 bytes also second arg */
	subs	sp, sp, r1		/* allocate arm_boot_params struct on stack */
	mov	r0, sp			/* loader info pointer is first arg */
	bic	sp, sp, #7		/* align stack to 8 bytes */
	str	r1, [r0]		/* Store length of loader info */
	str	r9, [r0, #4]		/* Store r0 from boot loader */
	str	r8, [r0, #8]		/* Store r1 from boot loader */
	str	ip, [r0, #12]		/* store r2 from boot loader */
	str	fp, [r0, #16]		/* store r3 from boot loader */
	str	r5, [r0, #20]		/* store the physical address */
	adr	r4, Lpagetable		/* load the pagetable address */
	ldr	r5, [r4, #4]
	str	r5, [r0, #24]		/* store the pagetable address */
	mov	fp, #0			/* trace back starts here */
	bl	_C_LABEL(initarm)	/* Off we go */

	/* init arm will return the new stack pointer. */
	mov	sp, r0

	bl	_C_LABEL(mi_startup)		/* call mi_startup()! */

	adr	r0, .Lmainreturned
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
	RET

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
	ldr	r4, =(L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))
#if defined(SMP)
	orr	r4, #(L1_SHARED)
#endif
	orr	r1, r4

	/* Move the virtual address to the correct bit location */
	lsr	r2, #(L1_S_SHIFT - 2)

	mov	r4, r3
1:
	str	r1, [r0, r2]
	add	r2, r2, #4
	add	r1, r1, #(L1_S_SIZE)
	adds	r4, r4, #-1
	bhi	1b

	RET

VA_TO_PA_POINTER(Lpagetable, pagetable)

Lreal_start:
	.word	_start
Lend:
	.word	_edata

.Lstart:
	.word	_edata
	.word	_ebss
	.word	svcstk + INIT_ARM_STACK_SIZE

.Lvirt_done:
	.word	virt_done

.Lmainreturned:
	.asciz	"main() returned"
	.align	0

	.bss
svcstk:
	.space	INIT_ARM_STACK_SIZE

/*
 * Memory for the initial pagetable. We are unable to place this in
 * the bss as this will be cleared after the table is loaded.
 */
	.section ".init_pagetable"
	.align	14 /* 16KiB aligned */
pagetable:
	.space	L1_TABLE_SIZE

	.text
	.align	0

.Lcpufuncs:
	.word	_C_LABEL(cpufuncs)

#if defined(SMP)

.Lmpvirt_done:
	.word	mpvirt_done
VA_TO_PA_POINTER(Lstartup_pagetable_secondary, temp_pagetable)

ASENTRY_NP(mpentry)

	/* Make sure interrupts are disabled. */
	mrs	r7, cpsr
	orr	r7, r7, #(PSR_I | PSR_F)
	msr	cpsr_c, r7

	/* Disable MMU.  It should be disabled already, but make sure. */
	mrc	p15, 0, r2, c1, c0, 0
	bic	r2, r2, #(CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_DC_ENABLE |\
	    CPU_CONTROL_WBUF_ENABLE)
	bic	r2, r2, #(CPU_CONTROL_IC_ENABLE)
	bic	r2, r2, #(CPU_CONTROL_BPRD_ENABLE)
	mcr	p15, 0, r2, c1, c0, 0
	nop
	nop
	nop
	CPWAIT(r0)

#if ARM_MMU_V6
	bl	armv6_idcache_inv_all	/* Modifies r0 only */
#elif ARM_MMU_V7
	bl	armv7_idcache_inv_all	/* Modifies r0-r3, ip */
#endif

	/* Load the page table physical address */
	adr	r0, Lstartup_pagetable_secondary
	bl	translate_va_to_pa
	/* Load the address the secondary page table */
	ldr	r0, [r0]

	orr 	r0, r0, #2		/* Set TTB shared memory flag */
	mcr	p15, 0, r0, c2, c0, 0	/* Set TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* Flush TLB */

	mov	r0, #0
	mcr	p15, 0, r0, c13, c0, 1	/* Set ASID to 0 */

	/* Set the Domain Access register.  Very important! */
	mov	r0, #((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT)
	mcr	p15, 0, r0, c3, c0, 0
	/* Enable MMU */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #CPU_CONTROL_V6_EXTPAGE
	orr	r0, r0, #CPU_CONTROL_AF_ENABLE
	orr	r0, r0, #(CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_DC_ENABLE |\
	    CPU_CONTROL_WBUF_ENABLE)
	orr	r0, r0, #(CPU_CONTROL_IC_ENABLE)
	orr	r0, r0, #(CPU_CONTROL_BPRD_ENABLE)
	mcr	p15, 0, r0, c1, c0, 0
	nop
	nop
	nop
	CPWAIT(r0)

	adr	r1, .Lstart
	ldmia	r1, {r1, r2, sp}	/* Set initial stack and */
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #15
	mov	r1, #2048
	mul	r2, r1, r0
	sub	sp, sp, r2
	str	r1, [sp]
	ldr	pc, .Lmpvirt_done

mpvirt_done:

	mov	fp, #0			/* trace back starts here */
	bl	_C_LABEL(init_secondary)	/* Off we go */

	adr	r0, .Lmpreturned
	b	_C_LABEL(panic)
	/* NOTREACHED */

.Lmpreturned:
	.asciz	"init_secondary() returned"
	.align	0
END(mpentry)
#endif

ENTRY_NP(cpu_halt)
	mrs     r2, cpsr
	bic	r2, r2, #(PSR_MODE)
	orr     r2, r2, #(PSR_SVC32_MODE)
	orr	r2, r2, #(PSR_I | PSR_F)
	msr     cpsr_fsxc, r2

	ldr	r4, .Lcpu_reset_address
	ldr	r4, [r4]

	ldr	r0, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r0, #CF_IDCACHE_WBINV_ALL]
	mov	lr, pc
	ldr	pc, [r0, #CF_L2CACHE_WBINV_ALL]

	/*
	 * Load the cpu_reset_needs_v4_MMU_disable flag to determine if it's
	 * necessary.
	 */

	ldr	r1, .Lcpu_reset_needs_v4_MMU_disable
	ldr	r1, [r1]
	cmp	r1, #0
	mov	r2, #0

	/*
	 * MMU & IDC off, 32 bit program & data space
	 * Hurl ourselves into the ROM
	 */
	mov	r0, #(CPU_CONTROL_32BP_ENABLE | CPU_CONTROL_32BD_ENABLE)
	mcr     15, 0, r0, c1, c0, 0
	mcrne   15, 0, r2, c8, c7, 0 	/* nail I+D TLB on ARMv4 and greater */
	mov     pc, r4

	/*
	 * _cpu_reset_address contains the address to branch to, to complete
	 * the cpu reset after turning the MMU off
	 * This variable is provided by the hardware specific code
	 */
.Lcpu_reset_address:
	.word	_C_LABEL(cpu_reset_address)

	/*
	 * cpu_reset_needs_v4_MMU_disable contains a flag that signals if the
	 * v4 MMU disable instruction needs executing... it is an illegal instruction
	 * on f.e. ARM6/7 that locks up the computer in an endless illegal
	 * instruction / data-abort / reset loop.
	 */
.Lcpu_reset_needs_v4_MMU_disable:
	.word	_C_LABEL(cpu_reset_needs_v4_MMU_disable)
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
	.global _C_LABEL(esym)
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

	.align	0
	.global _C_LABEL(esigcode)
		_C_LABEL(esigcode):

	.data
	.global szsigcode
szsigcode:
	.long esigcode-sigcode

/* End of locore.S */
