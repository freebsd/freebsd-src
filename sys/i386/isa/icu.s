/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 *	@(#)icu.s	7.2 (Berkeley) 5/21/91
 *
 *	$Id: icu.s,v 1.8 1994/04/02 07:00:41 davidg Exp $
 */

/*
 * AT/386
 * Vector interrupt control section
 */

/*
 * XXX this file should be named ipl.s.  All spls are now soft and the
 * only thing related to the hardware icu is that the h/w interrupt
 * numbers are used without translation in the masks.
 */

#include "../net/netisr.h"

	.data
	.globl	_cpl
_cpl:	.long	HWI_MASK | SWI_MASK	/* current priority (all off) */
	.globl	_imen
_imen:	.long	HWI_MASK		/* interrupt mask enable (all h/w off) */
_high_imask:	.long	HWI_MASK | SWI_MASK
	.globl	_tty_imask
_tty_imask:	.long	0
	.globl	_bio_imask
_bio_imask:	.long	0
	.globl	_net_imask
_net_imask:	.long	0
	.globl	_ipending
_ipending:	.long	0
	.globl	_astpending
_astpending:	.long	0		/* tells us an AST needs to be taken */
	.globl	_netisr
_netisr:	.long	0		/* set with bits for which queue to service */
vec:
	.long	vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7
	.long	vec8, vec9, vec10, vec11, vec12, vec13, vec14, vec15

	.text

/*
 * Handle return from interrupts, traps and syscalls.
 */
	SUPERALIGN_TEXT
_doreti:
	FAKE_MCOUNT(_bintr)		/* init "from" _bintr -> _doreti */
	addl	$4,%esp			/* discard unit number */
	popl	%eax			/* cpl to restore */
doreti_next:
	/*
	 * Check for pending HWIs and SWIs atomically with restoring cpl
	 * and exiting.  The check has to be atomic with exiting to stop
	 * (ipending & ~cpl) changing from zero to nonzero while we're
	 * looking at it (this wouldn't be fatal but it would increase
	 * interrupt latency).  Restoring cpl has to be atomic with exiting
	 * so that the stack cannot pile up (the nesting level of interrupt
	 * handlers is limited by the number of bits in cpl).
	 */
	movl	%eax,%ecx
	notl	%ecx
	cli
	andl	_ipending,%ecx
	jne	doreti_unpend
doreti_exit:
	movl	%eax,_cpl
	MEXITCOUNT
	popl	%es
	popl	%ds
	popal
	addl	$8,%esp
	iret

	ALIGN_TEXT
doreti_unpend:
	/*
	 * Enabling interrupts is safe because we haven't restored cpl yet.
	 * The locking from the "btrl" test is probably no longer necessary.
	 * We won't miss any new pending interrupts because we will check
	 * for them again.
	 */
	sti
	bsfl	%ecx,%ecx		/* slow, but not worth optimizing */
	btrl	%ecx,_ipending
	jnc	doreti_next		/* some intr cleared memory copy */
	movl	ihandlers(,%ecx,4),%edx
	testl	%edx,%edx
	je	doreti_next		/* "can't happen" */
	cmpl	$NHWI,%ecx
	jae	doreti_swi
	cli
	movl	%eax,_cpl
	MEXITCOUNT
	jmp	%edx

	ALIGN_TEXT
doreti_swi:
	pushl	%eax
	/*
	 * The SWI_AST handler has to run at cpl = SWI_AST_MASK and the
	 * SWI_CLOCK handler at cpl = SWI_CLOCK_MASK, so we have to restore
	 * all the h/w bits in cpl now and have to worry about stack growth.
	 * The worst case is currently (30 Jan 1994) 2 SWI handlers nested
	 * in dying interrupt frames and about 12 HWIs nested in active
	 * interrupt frames.  There are only 4 different SWIs and the HWI
	 * and SWI masks limit the nesting further.
	 */
	orl	imasks(,%ecx,4),%eax
	movl	%eax,_cpl
	call	%edx
	popl	%eax
	jmp	doreti_next

	ALIGN_TEXT
swi_ast:
	addl	$8,%esp			/* discard raddr & cpl to get trap frame */
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp)
	je	swi_ast_phantom
	movl	$T_ASTFLT,(2+8+0)*4(%esp)
	call	_trap
	subl	%eax,%eax		/* recover cpl */
	jmp	doreti_next

	ALIGN_TEXT
swi_ast_phantom:
	/*
	 * These happen when there is an interrupt in a trap handler before
	 * ASTs can be masked or in an lcall handler before they can be
	 * masked or after they are unmasked.  They could be avoided for
	 * trap entries by using interrupt gates, and for lcall exits by
	 * using by using cli, but they are unavoidable for lcall entries.
	 */
	cli
	orl	$SWI_AST_PENDING,_ipending
	jmp	doreti_exit	/* SWI_AST is highest so we must be done */

/*
 * Interrupt priority mechanism
 *	-- soft splXX masks with group mechanism (cpl)
 *	-- h/w masks for currently active or unused interrupts (imen)
 *	-- ipending = active interrupts currently masked by cpl
 */

ENTRY(splz)
	/*
	 * The caller has restored cpl and checked that (ipending & ~cpl)
	 * is nonzero.  We have to repeat the check since if there is an
	 * interrupt while we're looking, _doreti processing for the
	 * interrupt will handle all the unmasked pending interrupts
	 * because we restored early.  We're repeating the calculation
	 * of (ipending & ~cpl) anyway so that the caller doesn't have
	 * to pass it, so this only costs one "jne".  "bsfl %ecx,%ecx"
	 * is undefined when %ecx is 0 so we can't rely on the secondary
	 * btrl tests.
	 */
	movl	_cpl,%eax
splz_next:
	/*
	 * We don't need any locking here.  (ipending & ~cpl) cannot grow 
	 * while we're looking at it - any interrupt will shrink it to 0.
	 */
	movl	%eax,%ecx
	notl	%ecx
	andl	_ipending,%ecx
	jne	splz_unpend
	ret

	ALIGN_TEXT
splz_unpend:
	bsfl	%ecx,%ecx
	btrl	%ecx,_ipending
	jnc	splz_next
	movl	ihandlers(,%ecx,4),%edx
	testl	%edx,%edx
	je	splz_next		/* "can't happen" */
	cmpl	$NHWI,%ecx
	jae	splz_swi
	/*
	 * We would prefer to call the intr handler directly here but that
	 * doesn't work for badly behaved handlers that want the interrupt
	 * frame.  Also, there's a problem determining the unit number.
	 * We should change the interface so that the unit number is not
	 * determined at config time.
	 */
	jmp	*vec(,%ecx,4)

	ALIGN_TEXT
splz_swi:
	cmpl	$SWI_AST,%ecx
	je	splz_next		/* "can't happen" */
	pushl	%eax
	orl	imasks(,%ecx,4),%eax
	movl	%eax,_cpl
	call	%edx
	popl	%eax
	movl	%eax,_cpl
	jmp	splz_next

/*
 * Fake clock IRQ so that it appears to come from our caller and not from
 * vec0, so that kernel profiling works.
 * XXX do this more generally (for all vectors; look up the C entry point).
 * XXX frame bogusness stops us from just jumping to the C entry point.
 */
	ALIGN_TEXT
vec0:
	popl	%eax			/* return address */
	pushfl
#define	KCSEL	8
	pushl	$KCSEL
	pushl	%eax
	cli
	MEXITCOUNT
	jmp	_Vclk

#define BUILD_VEC(irq_num) \
	ALIGN_TEXT ; \
vec/**/irq_num: ; \
	int	$ICU_OFFSET + (irq_num) ; \
	ret

	BUILD_VEC(1)
	BUILD_VEC(2)
	BUILD_VEC(3)
	BUILD_VEC(4)
	BUILD_VEC(5)
	BUILD_VEC(6)
	BUILD_VEC(7)
	BUILD_VEC(8)
	BUILD_VEC(9)
	BUILD_VEC(10)
	BUILD_VEC(11)
	BUILD_VEC(12)
	BUILD_VEC(13)
	BUILD_VEC(14)
	BUILD_VEC(15)

	ALIGN_TEXT
swi_clock:
	MCOUNT
	subl	%eax,%eax
	cmpl	$_splz,(%esp)		/* XXX call from splz()? */
	jae	1f			/* yes, usermode = 0 */
	movl	4+4+TRAPF_CS_OFF(%esp),%eax	/* no, check trap frame */
	andl	$SEL_RPL_MASK,%eax
1:
	pushl	%eax
	call	_softclock
	addl	$4,%esp
	ret

#define DONET(s, c, event) ; \
	.globl	c ; \
	btrl	$s,_netisr ; \
	jnc	9f ; \
	call	c ; \
9:

	ALIGN_TEXT
swi_net:
	MCOUNT
#if 0	
	DONET(NETISR_RAW, _rawintr,netisr_raw)
#endif
#ifdef INET
	DONET(NETISR_IP, _ipintr,netisr_ip)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr,netisr_imp)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr,netisr_ns)
#endif
#ifdef ISO
	DONET(NETISR_ISO, _clnlintr,netisr_iso)
#endif
#ifdef	CCITT
	DONET(NETISR_X25, _pkintr, 29)
	DONET(NETISR_HDLC, _hdintr, 30)
#endif
	ret

	ALIGN_TEXT
swi_tty:
	MCOUNT
#include "sio.h"
#if NSIO > 0
	jmp	_siopoll
#else
	ret
#endif
