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
 *	@(#)ipl.s
 *
 *	$Id: ipl.s,v 1.8 1997/08/10 21:17:55 fsmp Exp $
 */


/*
 * AT/386
 * Vector interrupt control section
 */

	.data
	ALIGN_DATA

/* current priority (all off) */
	.globl	_cpl
_cpl:	.long	HWI_MASK | SWI_MASK

#ifndef APIC_IO
/* interrupt mask enable (all h/w off) */
	.globl	_imen
_imen:	.long	HWI_MASK
#endif /* APIC_IO */

	.globl	_tty_imask
_tty_imask:	.long	0
	.globl	_bio_imask
_bio_imask:	.long	0
	.globl	_net_imask
_net_imask:	.long	0
	.globl	_soft_imask
_soft_imask:	.long	SWI_MASK
	.globl	_softnet_imask
_softnet_imask:	.long	SWI_NET_MASK
	.globl	_softtty_imask
_softtty_imask:	.long	SWI_TTY_MASK

	.globl	_ipending

/* pending interrupts blocked by splxxx() */
_ipending:	.long	0

/* set with bits for which queue to service */
	.globl	_netisr
_netisr:	.long	0

	.globl _netisrs
_netisrs:
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr
	.long	dummynetisr, dummynetisr, dummynetisr, dummynetisr

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
	decb	_intr_nesting_level
	MEXITCOUNT
#ifdef VM86
	/*
	 * XXX
	 * Sometimes when attempting to return to vm86 mode, cpl is not
	 * being reset to 0, so here we force it to 0 before returning to
	 * vm86 mode.  doreti_stop is a convenient place to set a breakpoint.
	 * When the cpl problem is solved, this code can disappear.
	 */
	cmpl	$0,_cpl
	je	1f
	testl	$PSL_VM,TF_EFLAGS(%esp)
	je	1f
doreti_stop:
	movl	$0,_cpl
	nop
1:
#endif /* VM86 */

#ifdef SMP
	/* release the kernel lock */
	pushl	$_mp_lock		/* GIANT_LOCK */
	call	_MPrellock
	add	$4, %esp
#endif /* SMP */

	.globl	doreti_popl_es
doreti_popl_es:
	popl	%es
	.globl	doreti_popl_ds
doreti_popl_ds:
	popl	%ds
	popal
	addl	$8,%esp
	.globl	doreti_iret
doreti_iret:
	iret

	ALIGN_TEXT
	.globl	doreti_iret_fault
doreti_iret_fault:
	subl	$8,%esp
	pushal
	pushl	%ds
	.globl	doreti_popl_ds_fault
doreti_popl_ds_fault:
	pushl	%es
	.globl	doreti_popl_es_fault
doreti_popl_es_fault:
	movl	$0,4+4+32+4(%esp)	/* XXX should be the error code */
	movl	$T_PROTFLT,4+4+32+0(%esp)
	jmp	alltraps_with_regs_pushed

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
	MPLOCKED btrl %ecx, _ipending
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
swi_ast_user:
	movl	$T_ASTFLT,(2+8+0)*4(%esp)
	movb	$0,_intr_nesting_level	/* finish becoming a trap handler */
	call	_trap
	subl	%eax,%eax		/* recover cpl */
	movb	$1,_intr_nesting_level	/* for doreti_next to decrement */
	jmp	doreti_next

	ALIGN_TEXT
swi_ast_phantom:
#ifdef VM86
	/*
	 * check for ast from vm86 mode.  Placed down here so the jumps do
	 * not get taken for mainline code.
	 */
	testl	$PSL_VM,TF_EFLAGS(%esp)
	jne	swi_ast_user
#endif /* VM86 */
	/*
	 * These happen when there is an interrupt in a trap handler before
	 * ASTs can be masked or in an lcall handler before they can be
	 * masked or after they are unmasked.  They could be avoided for
	 * trap entries by using interrupt gates, and for lcall exits by
	 * using by using cli, but they are unavoidable for lcall entries.
	 */
	cli
	MPLOCKED orl $SWI_AST_PENDING, _ipending
	subl	%eax,%eax
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
	MPLOCKED btrl %ecx, _ipending
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
	jmp	*_vec(,%ecx,4)

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


	ALIGN_TEXT
swi_net:
	MCOUNT
	bsfl	_netisr,%eax
	je	swi_net_done
swi_net_more:
	btrl	%eax,_netisr
	jnc	swi_net_next
	call	*_netisrs(,%eax,4)
swi_net_next:
	bsfl	_netisr,%eax
	jne	swi_net_more
swi_net_done:
	ret

	ALIGN_TEXT
dummynetisr:
	MCOUNT
	ret	

/*
 * XXX there should be a registration function to put the handler for the
 * attached driver directly in ihandlers.  Then this function will go away.
 */
	ALIGN_TEXT
swi_tty:
	MCOUNT
#include "cy.h"
#if NCY > 0
	call	_cypoll
#endif
#include "rc.h"
#if NRC > 0
	call	_rcpoll
#endif
#include "sio.h"
#if NSIO > 0
	jmp	_siopoll
#else
	ret
#endif

#ifdef APIC_IO
#include "i386/isa/apic_ipl.s"
#else
#include "i386/isa/icu_ipl.s"
#endif /* APIC_IO */
