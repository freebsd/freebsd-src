/*
 * Copyright (c) 1995, Jack F. Vogel
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
 *	This product includes software developed by Jack F. Vogel
 * 4. The name of the developer may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * mpboot.s:	FreeBSD machine support for the Intel MP Spec
 *		multiprocessor systems.
 *
 * $FreeBSD$
 */

#include <machine/asmacros.h>		/* miscellaneous asm macros */
#include <machine/apic.h>
#include <machine/specialreg.h>

#include "assym.s"

#define	R(x)	((x)-KERNBASE)

/*
 * this code MUST be enabled here and in mp_machdep.c
 * it follows the very early stages of AP boot by placing values in CMOS ram.
 * it NORMALLY will never be needed and thus the primitive method for enabling.
 *
#define CHECK_POINTS
 */

#if defined(CHECK_POINTS) && !defined(PC98)

#define CMOS_REG	(0x70)
#define CMOS_DATA	(0x71)

#define CHECKPOINT(A,D)		\
	movb	$(A),%al ;	\
	outb	%al,$CMOS_REG ;	\
	movb	$(D),%al ;	\
	outb	%al,$CMOS_DATA

#else

#define CHECKPOINT(A,D)

#endif /* CHECK_POINTS */


/*
 * the APs enter here from their trampoline code (bootMP, below)
 */
	.p2align 4

NON_GPROF_ENTRY(MPentry)
	CHECKPOINT(0x36, 3)
	/* Now enable paging mode */
#ifdef PAE
	movl	%cr4,%eax
	orl	$CR4_PAE,%eax
	movl	%eax,%cr4
	movl	$R(_IdlePDPT),%eax
#else
	movl	R(_IdlePTD),%eax
#endif
	movl	%eax,%cr3	
	movl	%cr0,%eax
	orl	$CR0_PE|CR0_PG,%eax		/* enable paging */
	movl	%eax,%cr0			/* let the games begin! */
	movl	_bootSTK,%esp			/* boot stack end loc. */

	pushl	$mp_begin			/* jump to high mem */
	ret

	/*
	 * Wait for the booting CPU to signal startup
	 */
mp_begin:	/* now running relocated at KERNBASE */
	CHECKPOINT(0x37, 4)
	call	_init_secondary			/* load i386 tables */
	CHECKPOINT(0x38, 5)

	/*
	 * If the [BSP] CPU has support for VME, turn it on.
	 */
	testl	$CPUID_VME, _cpu_feature	/* XXX WRONG! BSP! */
	jz	1f
	movl	%cr4, %eax
	orl	$CR4_VME, %eax
	movl	%eax, %cr4
1:

	/* disable the APIC, just to be SURE */
	movl	lapic_svr, %eax			/* get spurious vector reg. */
	andl	$~APIC_SVR_SWEN, %eax		/* clear software enable bit */
	movl	%eax, lapic_svr

	/* signal our startup to the BSP */
	movl	lapic_ver, %eax			/* our version reg contents */
	movl	%eax, _cpu_apic_versions	/* into [ 0 ] */
	incl	_mp_ncpus			/* signal BSP */

	CHECKPOINT(0x39, 6)

	/* wait till we can get into the kernel */
	call	_boot_get_mplock

	/* Now, let's prepare for some REAL WORK :-) */
	call	_ap_init

	call	_rel_mplock
	wbinvd				/* Avoid livelock */
2:	
	cmpl	$0, CNAME(smp_started)	/* Wait for last AP to be ready */
	jz	2b
	call _get_mplock
	
	/* let her rip! (loads new stack) */
	jmp 	_cpu_switch

NON_GPROF_ENTRY(wait_ap)
	pushl	%ebp
	movl	%esp, %ebp
	call	_rel_mplock
	wbinvd				/* Avoid livelock */
	movl	%eax, 8(%ebp)
1:		
	cmpl	$0, CNAME(smp_started)
	jnz	2f
	decl	%eax
	cmpl	$0, %eax
	jge	1b
2:
	call	_get_mplock
	movl	%ebp, %esp
	popl	%ebp
	ret
	

/*
 * This is the embedded trampoline or bootstrap that is
 * copied into 'real-mode' low memory, it is where the
 * secondary processor "wakes up". When it is executed
 * the processor will eventually jump into the routine
 * MPentry, which resides in normal kernel text above
 * 1Meg.		-jackv
 */

	.data
	ALIGN_DATA				/* just to be sure */

BOOTMP1:

NON_GPROF_ENTRY(bootMP)
	.code16		
	cli
	CHECKPOINT(0x34, 1)
	/* First guarantee a 'clean slate' */
	xorl	%eax, %eax
	movl	%eax, %ebx
	movl	%eax, %ecx
 	movl	%eax, %edx
	movl	%eax, %esi
	movl	%eax, %edi

	/* set up data segments */
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	mov	$(boot_stk-_bootMP), %esp

	/* Now load the global descriptor table */
	lgdt	MP_GDTptr-_bootMP

	/* Enable protected mode */
	movl	%cr0, %eax
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	pushl	$0x18
	pushl	$(protmode-_bootMP)
	lretl

       .code32		
protmode:
	CHECKPOINT(0x35, 2)

	/*
	 * we are NOW running for the first time with %eip
	 * having the full physical address, BUT we still
	 * are using a segment descriptor with the origin
	 * not matching the booting kernel.
	 *
 	 * SO NOW... for the BIG Jump into kernel's segment
	 * and physical text above 1 Meg.
	 */
	mov	$0x10, %ebx
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs
	movw	%bx, %ss

	.globl	_bigJump
_bigJump:
	/* this will be modified by mpInstallTramp() */
	ljmp	$0x08, $0			/* far jmp to MPentry() */
	
dead:	hlt /* We should never get here */
	jmp	dead

/*
 * MP boot strap Global Descriptor Table
 */
	.p2align 4
	.globl	_MP_GDT
	.globl	_bootCodeSeg
	.globl	_bootDataSeg
_MP_GDT:

nulldesc:		/* offset = 0x0 */

	.word	0x0	
	.word	0x0	
	.byte	0x0	
	.byte	0x0	
	.byte	0x0	
	.byte	0x0	

kernelcode:		/* offset = 0x08 */

	.word	0xffff	/* segment limit 0..15 */
	.word	0x0000	/* segment base 0..15 */
	.byte	0x0	/* segment base 16..23; set for 0K */
	.byte	0x9f	/* flags; Type	*/
	.byte	0xcf	/* flags; Limit	*/
	.byte	0x0	/* segment base 24..32 */

kerneldata:		/* offset = 0x10 */

	.word	0xffff	/* segment limit 0..15 */
	.word	0x0000	/* segment base 0..15 */
	.byte	0x0	/* segment base 16..23; set for 0k */
	.byte	0x93	/* flags; Type  */
	.byte	0xcf	/* flags; Limit */
	.byte	0x0	/* segment base 24..32 */

bootcode:		/* offset = 0x18 */

	.word	0xffff	/* segment limit 0..15 */
_bootCodeSeg:		/* this will be modified by mpInstallTramp() */
	.word	0x0000	/* segment base 0..15 */
	.byte	0x00	/* segment base 16...23; set for 0x000xx000 */
	.byte	0x9e	/* flags; Type  */
	.byte	0xcf	/* flags; Limit */
	.byte	0x0	/*segment base 24..32 */

bootdata:		/* offset = 0x20 */

	.word	0xffff	
_bootDataSeg:		/* this will be modified by mpInstallTramp() */
	.word	0x0000	/* segment base 0..15 */
	.byte	0x00	/* segment base 16...23; set for 0x000xx000 */
	.byte	0x92	
	.byte	0xcf	
	.byte	0x0		

/*
 * GDT pointer for the lgdt call
 */
	.globl	_mp_gdtbase

MP_GDTptr:	
_mp_gdtlimit:
	.word	0x0028		
_mp_gdtbase:		/* this will be modified by mpInstallTramp() */
	.long	0

	.space	0x100	/* space for boot_stk - 1st temporary stack */
boot_stk:

BOOTMP2:
	.globl	_bootMP_size
_bootMP_size:
	.long	BOOTMP2 - BOOTMP1
