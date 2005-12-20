/*	$NetBSD: locore.S,v 1.14 2003/04/20 16:21:40 thorpej Exp $	*/

/*-
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
#include <machine/pte.h>
__FBSDID("$FreeBSD$");

/* What size should this really be ? It is only used by init_arm() */
#define INIT_ARM_STACK_SIZE	2048

/*
 * This is for kvm_mkdb, and should be the address of the beginning
 * of the kernel text segment (not necessarily the same as kernbase).
 */


#define	CPWAIT_BRANCH							 \
	sub	pc, pc, #4

#define	CPWAIT(tmp)							 \
	mrc	p15, 0, tmp, c2, c0, 0	/* arbitrary read of CP15 */	;\
	mov	tmp, tmp		/* wait for it to complete */	;\
	CPWAIT_BRANCH			/* branch to next insn */

	.text
	.align	0
.globl kernbase
.set kernbase,KERNBASE
.globl physaddr
.set physaddr,PHYSADDR

ENTRY_NP(btext)

ASENTRY_NP(_start)


	/* Check if we are running on RAM, if not move ourself to RAM */
#if 0
	cmp	pc, #PHYSADDR
	bhi	start_inram /* XXX: This is wrong */
#endif
	b	start_inram /*
			     * XXX: this is even more wrong, but RedBoot
			     * use 0x00000000-0x100000000 as virtual
			     * addresses for the RAM.
			     */

	/* move me to RAM
	 * XXX: we can use memcpy if it is PIC
	 */
	ldr r1, Lcopy_size
	adr r0, _C_LABEL(_start)
	add r1, r1, #3
	mov r1, r1, LSR #2
	mov r2, #PHYSADDR
	add r2, r2, #0x00200000
	mov r4, r2

5:	ldr r3,[r0],#4
	str r3,[r2],#4
	subs r1,r1,#1
	bhi 5b

	/* Jump to RAM */
	ldr r0, Lstart_off
	add pc, r4, r0

Lcopy_size:	.word _edata-_C_LABEL(_start)
Lstart_off:	.word start_inram-_C_LABEL(_start)
start_inram:
	adr	r7, Lunmapped

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
	mov	pc, r7
Lunmapped:

#ifdef STARTUP_PAGETABLE_ADDR
	/* build page table from scratch */
	ldr	r0, Lstartup_pagetable
	adr	r4, mmu_init_table
	bic	r4, r4, #0xff000000
	orr	r4, r4, #PHYSADDR
	b	3f

2:
	str	r3, [r0, r2]
	add	r2, r2, #4
	add	r3, r3, #(L1_S_SIZE)
	adds	r1, r1, #-1
	bhi	2b
3:
	ldmia	r4!, {r1,r2,r3}   /* # of sections, PA|attr, VA */
	cmp	r1, #0
	adrne	r5, 2b
	bicne	r5, r5, #0xff000000
	orrne	r5, r5, #PHYSADDR
	movne	pc, r5

	mcr	p15, 0, r0, c2, c0, 0	/* Set TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* Flush TLB */

	/* Set the Domain Access register.  Very important! */
	mov     r0, #((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT)
	mcr	p15, 0, r0, c3, c0, 0
	/* Enable MMU */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #CPU_CONTROL_MMU_ENABLE
	mcr	p15, 0, r0, c1, c0, 0
	nop
	nop
	nop
	CPWAIT(r0)

#endif
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
	mov	fp, #0		/* trace back starts here */
	bl	_C_LABEL(initarm)	/* Off we go */

	/* init arm will return the new stack pointer. */
	mov	sp, r0

	bl	_C_LABEL(mi_startup)		/* call mi_startup()! */

	adr	r0, .Lmainreturned
	b	_C_LABEL(panic)
	/* NOTEACHED */
#ifdef STARTUP_PAGETABLE_ADDR
#define MMU_INIT(va,pa,n_sec,attr) \
	.word	n_sec					    ; \
	.word	4*((va)>>L1_S_SHIFT)			    ; \
	.word	(pa)|(attr)				    ;

Lvirtaddr:
	.word	KERNVIRTADDR
Lphysaddr:
	.word	KERNPHYSADDR
Lstartup_pagetable:
	.word	STARTUP_PAGETABLE_ADDR
mmu_init_table:
	/* fill all table VA==PA */
	/* map SDRAM VA==PA, WT cacheable */
	MMU_INIT(PHYSADDR, PHYSADDR , 64, L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))
	/* map VA 0xc0000000..0xc3ffffff to PA */
	MMU_INIT(KERNBASE, PHYSADDR, 64, L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))

	.word 0	/* end of table */
#endif
.Lstart:
	.word	_edata
	.word	_end
	.word	svcstk + INIT_ARM_STACK_SIZE

.Lvirt_done:
	.word	virt_done
.Lmainreturned:
	.asciz	"main() returned"
	.align	0

	.bss
svcstk:
	.space	INIT_ARM_STACK_SIZE

	.text
	.align	0

#ifndef OFW
	/* OFW based systems will used OF_boot() */

.Lcpufuncs:
	.word	_C_LABEL(cpufuncs)

ENTRY_NP(cpu_halt)
	mrs     r2, cpsr
	bic	r2, r2, #(PSR_MODE)
	orr     r2, r2, #(PSR_SVC32_MODE)
	orr	r2, r2, #(I32_bit | F32_bit)
	msr     cpsr_all, r2

	ldr	r4, .Lcpu_reset_address
	ldr	r4, [r4]

	ldr	r0, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r0, #CF_IDCACHE_WBINV_ALL]

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

#endif	/* OFW */

#ifdef IPKDB
/*
 * Execute(inst, psr, args, sp)
 *
 * Execute INSTruction with PSR and ARGS[0] - ARGS[3] making
 * available stack at SP for next undefined instruction trap.
 *
 * Move the instruction onto the stack and jump to it.
 */
ENTRY_NP(Execute)
	mov	ip, sp
	stmfd	sp!, {r2, r4-r7, fp, ip, lr, pc}
	sub	fp, ip, #4
	mov	ip, r3
	ldr	r7, .Lreturn
	stmfd	sp!, {r0, r7}
	adr	r7, #.LExec
	mov	r5, r1
	mrs	r4, cpsr
	ldmia	r2, {r0-r3}
	mov	r6, sp
	mov	sp, ip
	msr	cpsr_all, r5
	mov	pc, r6
.LExec:
	mrs	r5, cpsr
/* XXX Cannot switch thus easily back from user mode */
	msr	cpsr_all, r4
	add	sp, r6, #8
	ldmfd	sp!, {r6}
	stmia	r6, {r0-r3}
	mov	r0, r5
	ldmdb	fp, {r4-r7, fp, sp, pc}
.Lreturn:
	mov	pc, r7
#endif

/*
 * setjump + longjmp
 */
ENTRY(setjmp)
	stmia	r0, {r4-r14}
	mov	r0, #0x00000000
	RET

ENTRY(longjmp)
	ldmia	r0, {r4-r14}
	mov	r0, #0x00000001
	RET

	.data
	.global _C_LABEL(esym)
_C_LABEL(esym):	.word	_C_LABEL(end)

ENTRY_NP(abort)
	b	_C_LABEL(abort)

ENTRY_NP(sigcode)
	mov	r0, sp
	swi	SYS_sigreturn

	/* Well if that failed we better exit quick ! */

	swi	SYS_exit
	b	. - 8

	.align	0
	.global _C_LABEL(esigcode)
		_C_LABEL(esigcode):

	.data
	.global szsigcode
szsigcode:
	.long esigcode-sigcode
/* End of locore.S */
