/*      $NetBSD: rtld_start.S,v 1.4 2001/09/26 04:06:43 mycroft Exp $   */

/*-
 * Copyright (C) 1998   Tsubai Masanari
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asm.h>
#include <machine/spr.h>	/* For SPR_SPEFSCR if needed. */

.extern _GLOBAL_OFFSET_TABLE_
.extern _DYNAMIC

_ENTRY(.rtld_start)
	stwu    %r1,-48(%r1)	/* 16-byte aligned stack for reg saves +
				exit_proc & obj _rtld args +
				backchain & lrsave stack frame */
	stw     %r3,16(%r1)	/*  argc */
	stw     %r4,20(%r1)	/*  argv */
	stw     %r5,24(%r1)	/*  envp */
/*	stw     %r6,28(%r1)   *//*  obj (always 0) */
/*	stw     %r7,32(%r1)   *//*  cleanup (always 0) */
	stw     %r8,36(%r1)	/*  ps_strings */

	/*
	 * Perform initial relocation of ld-elf.so. Not as easy as it
	 * sounds.
	 *  - perform small forward branch to put PC into link reg
	 *  - use link-time constants to determine offset to the
	 *    _DYNAMIC section and the GOT. Add these to the PC to
	 *    convert to absolute addresses.
	 *  - read GOT[0], which is the SVR4 ABI-specified link-time
	 *    value of _DYNAMIC. Subtract this value from the absolute
	 *    value to determine the load address
	 *  - call reloc_non_plt_self() to fix up ld-elf.so's relocations
	 */
	bcl	20,31,1f
1:	mflr	%r30
	mr	%r3,%r30		# save for _DYNAMIC
	addis	%r30,%r30,_GLOBAL_OFFSET_TABLE_-1b@ha
	addi	%r30,%r30,_GLOBAL_OFFSET_TABLE_-1b@l
	addis	%r3,%r3,_DYNAMIC-1b@ha	# get _DYNAMIC actual address
	addi	%r3,%r3,_DYNAMIC-1b@l
	lwz	%r28,0(%r30)		# get base-relative &_DYNAMIC
	sub	%r28,%r3,%r28		# r28 = relocbase
	mr	%r4,%r28		# r4 = relocbase
	bl	reloc_non_plt_self /* reloc_non_plt_self(&_DYNAMIC,base) */

	/*
	 * The _rtld() function likes to see a stack layout containing
	 * { argc, argv[0], argv[1] ... argv[N], 0, env[0], ... , env[N] }
	 * Since the PowerPC stack was 16-byte aligned at exec time, the
	 * original stack layout has to be found by moving back a word
	 * from the argv pointer.
	 */
        lwz     %r4,20(%r1)	/* restore argv */
        addi    %r3,%r4,-4	/* locate argc ptr, &argv[-1] */

	addi	%r4,%r1,8	/* &exit_proc on stack */
	addi	%r5,%r1,12	/* &obj_main on stack */

	bl      _rtld		/* &_start = _rtld(sp, &exit_proc, &obj_main)*/
	mtlr    %r3

	/*
	 * Restore args, with new obj/exit proc
	 */
	lwz     %r3,16(%r1)     /* argc */
	lwz     %r4,20(%r1)	/* argv */
	lwz     %r5,24(%r1)	/* envp */
	lwz     %r6,12(%r1)	/* obj */
	lwz     %r7,8(%r1)	/* exit proc */
	lwz     %r8,36(%r1)	/* ps_strings */
        addi    %r1,%r1,48	/* restore original stackptr */

	blrl	/* _start(argc, argv, envp, obj, cleanup, ps_strings) */

	li      %r0,1		/* _exit() */
	sc
_END(.rtld_start)

#ifdef __SPE__
/* stack space for 30 GPRs + SPEFSCR/ACC/lr/cr */
#define	NREGS		31
#define	GPRWIDTH	8
#define	FUDGE		4	/* Fudge factor for alignment */
#else
/* stack space for 30 GPRs + lr/cr */
#define	NREGS		30
#define	GPRWIDTH	4
#define	FUDGE		4
#endif
/* Stack frame needs the 12-byte ABI frame plus fudge factor. */
#define	STACK_SIZE	(NREGS * GPRWIDTH + 4 * 2 + 12 + FUDGE)

/*
 * _rtld_bind_secureplt_start()
 *
 * Call into the MI binder (Secure-PLT stub).
 * secure-plt expects %r11 to be the offset to the rela entry.
 * bss-plt expects %r11 to be index of the rela entry.
 * So for bss-plt, we multiply the index by 12 to get the offset.
 */
_ENTRY(_rtld_bind_secureplt_start)
	stwu    %r1,-STACK_SIZE(%r1)
#ifdef __SPE__
	evstdd	%r0,24(%r1)
#else
	stw     %r0,20(%r1)		# save r0
#endif

	/*
	 * Instead of division which is costly we will use multiplicative
	 * inverse.  a / n = ((a * inv(n)) >> 32)
	 * where inv(n) = (0x100000000 + n - 1) / n
	 */
	mr	%r0,%r11
	lis	%r11,0x15555556@h	# load multiplicative inverse of 12
	ori	%r11,%r11,0x15555556@l
	mulhwu	%r11,%r11,%r0		# get high half of multiplication
	b	1f
_END(_rtld_bind_secureplt_start)

/*
 * _rtld_bind_start()
 *
 * Call into the MI binder. This routine is reached via the PLT call cell,
 * and then _rtld_powerpc_pltresolve().
 * On entry, %r11 contains the index of the PLT cell, and %r12 contains
 * a pointer to the ELF object for the file.
 *  Save all registers, call into the binder to resolve and fixup the external
 * routine, and then transfer to the external routine on return.
 */
	.globl  _rtld_bind

_ENTRY(_rtld_bind_start)
	stwu    %r1,-STACK_SIZE(%r1)
#ifdef __SPE__
	evstdd	%r0,24(%r1)
#else
	stw     %r0,20(%r1)		# save r0
#endif
1:
	mflr    %r0
	stw     %r0,16(%r1)		# save lr
	mfcr    %r0
	stw     %r0,12(%r1)		# save cr
#ifdef __SPE__
	evstdd	%r3, 32(%r1)
	evstdd	%r4, 40(%r1)
	evstdd	%r5, 48(%r1)
	evstdd	%r6, 56(%r1)
	evstdd	%r7, 64(%r1)
	evstdd	%r8, 72(%r1)
	evstdd	%r9, 80(%r1)
	evstdd	%r10, 88(%r1)
	evstdd	%r11, 96(%r1)
	evstdd	%r12, 104(%r1)
	evstdd	%r13, 112(%r1)
	evstdd	%r14, 120(%r1)
	evstdd	%r15, 128(%r1)
	evstdd	%r16, 136(%r1)
	evstdd	%r17, 144(%r1)
	evstdd	%r18, 152(%r1)
	evstdd	%r19, 160(%r1)
	evstdd	%r20, 168(%r1)
	evstdd	%r21, 176(%r1)
	evstdd	%r22, 184(%r1)
	evstdd	%r23, 192(%r1)
	evstdd	%r24, 200(%r1)
	evstdd	%r25, 208(%r1)
	evstdd	%r26, 216(%r1)
	evstdd	%r27, 224(%r1)
	evstdd	%r28, 232(%r1)
	evstdd	%r29, 240(%r1)
	evstdd	%r30, 248(%r1)
	li	%r3, 256
	evstddx	%r31, %r1, %r3
	evxor	%r0, %r0, %r0
	li	%r3, 264
	evmwumiaa	%r0, %r0, %r0
	evstddx	%r0, %r1, %r3
	mfspr	%r3, SPR_SPEFSCR
	stw	%r3, 20(%r1)
#else
	stmw    %r3,24(%r1)		# save r3-r31
#endif

	mr      %r3,%r12		# obj
	mulli   %r4,%r11,12		# rela index * sizeof(Elf_Rela)
	bl      _rtld_bind		# target addr = _rtld_bind(obj, reloff)
	mtctr   %r3			# move absolute target addr into ctr

#ifdef __SPE__
	lwz	%r3, 20(%r1)
	mtspr	SPR_SPEFSCR, %r3
	li	%r3, 264
	evlddx	%r0, %r3, %r1
	evmra	%r0, %r0
	evldd	%r3, 32(%r1)
	evldd	%r4, 40(%r1)
	evldd	%r5, 48(%r1)
	evldd	%r6, 56(%r1)
	evldd	%r7, 64(%r1)
	evldd	%r8, 72(%r1)
	evldd	%r9, 80(%r1)
	evldd	%r10, 88(%r1)
	evldd	%r11, 96(%r1)
	evldd	%r12, 104(%r1)
	evldd	%r13, 112(%r1)
	evldd	%r14, 120(%r1)
	evldd	%r15, 128(%r1)
	evldd	%r16, 136(%r1)
	evldd	%r17, 144(%r1)
	evldd	%r18, 152(%r1)
	evldd	%r19, 160(%r1)
	evldd	%r20, 168(%r1)
	evldd	%r21, 176(%r1)
	evldd	%r22, 184(%r1)
	evldd	%r23, 192(%r1)
	evldd	%r24, 200(%r1)
	evldd	%r25, 208(%r1)
	evldd	%r26, 216(%r1)
	evldd	%r27, 224(%r1)
	evldd	%r28, 232(%r1)
	evldd	%r29, 240(%r1)
	evldd	%r30, 248(%r1)
	li	%r0, 256
	evlddx	%r31, %r1, %r0
#else
        lmw     %r3,24(%r1)		# restore r3-r31
#endif
        lwz     %r0,12(%r1)		# restore cr
        mtcr    %r0
        lwz     %r0,16(%r1)		# restore lr
        mtlr    %r0
#ifdef __SPE__
	evldd	%r0,24(%r1)
#else
        lwz     %r0,20(%r1)		# restore r0
#endif

        addi    %r1,%r1,STACK_SIZE	# restore stack
        bctr				# jump to target
_END(_rtld_bind_start)


/*
 * _rtld_powerpc_pltresolve()
 *
 *  This routine is copied into the latter part of the 72-byte reserved
 * area at the start of the PLT. The absolute address of the _rtld_bind_start
 * routine, and the ELF object for the loaded file, are inserted into
 * the code by the reloc.c:init_pltgot() routine.
 *  The first time an external routine is called, the PLT slot will
 * set up %r11 to the offset of the slot, and will jump to this routine.
 * The ELF object is shifted into %r11, and _rtld_bind_start is called
 * to complete the binding.
 */
_ENTRY(_rtld_powerpc_pltlongresolve)
	lis	%r12,0			# lis	12,jmptab@ha
	addi    %r12,%r12,0		# addi  12,12,jmptab@l
	subf	%r11,%r12,%r11		# reloff
	li	%r12,2
	srw	%r11,%r11,%r12		# index = reloff/sizeof(Elf_Addr)
_END(_rtld_powerpc_pltlongresolve)
_ENTRY(_rtld_powerpc_pltresolve)
        lis     %r12,0			# lis   12,_rtld_bind_start@ha
        addi    %r12,%r12,0		# addi  12,12,_rtld_bind_start@l
        mtctr   %r12
        lis     %r12,0			# lis   12,obj@ha
        addi    %r12,%r12,0		# addi  12,12,obj@l
        bctr
_END(_rtld_powerpc_pltresolve)

/*
 * _rtld_powerpc_pltcall()
 *
 *  This routine is copied into the 72-byte reserved area at the
 * start of the PLT. The reloc.c:init_pltgot() routine inserts
 * the absolute address of the jumptable.
 *  Control is transferred to this routine when the binder has
 * located the external routine, but determined that it is > 32Mb
 * from the PLT slot. Code is inserted into the PLT slot to set up
 * %r11 with the jumptable index, and jump to here, where the
 * absolute address of the external routine is loaded from the
 * jumptable and transferred to
 */
_ENTRY(_rtld_powerpc_pltcall)
        slwi    %r11,%r11,2		# jmptab offset = index * 4
        addis   %r11,%r11,0		# addis 11,11,jmptab@ha
        lwz     %r11,0(%r11)		# lwz   11,jmptab@l(11)
        mtctr   %r11
        bctr				# (*jmptab[index])()
_END(_rtld_powerpc_pltcall)

	.section .note.GNU-stack,"",%progbits
