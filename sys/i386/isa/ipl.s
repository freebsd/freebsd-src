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
 * $FreeBSD: src/sys/i386/isa/ipl.s,v 1.32.2.2 2000/07/07 00:38:48 obrien Exp $
 */


/*
 * AT/386
 * Vector interrupt control section
 *
 *  cpl		- Current interrupt disable mask
 *  *_imask	- Interrupt masks for various spl*() functions
 *  ipending	- Pending interrupts (set when a masked interrupt occurs)
 */

	.data
	ALIGN_DATA

/* current priority (all off) */
	.globl	_cpl
_cpl:	.long	HWI_MASK | SWI_MASK

	.globl	_tty_imask
_tty_imask:	.long	SWI_TTY_MASK
	.globl	_bio_imask
_bio_imask:	.long	SWI_CLOCK_MASK | SWI_CAMBIO_MASK
	.globl	_net_imask
_net_imask:	.long	SWI_NET_MASK | SWI_CAMNET_MASK
	.globl	_cam_imask
_cam_imask:	.long	SWI_CAMBIO_MASK | SWI_CAMNET_MASK
	.globl	_soft_imask
_soft_imask:	.long	SWI_MASK
	.globl	_softnet_imask
_softnet_imask:	.long	SWI_NET_MASK
	.globl	_softtty_imask
_softtty_imask:	.long	SWI_TTY_MASK

/* pending interrupts blocked by splxxx() */
	.globl	_ipending
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
	.type	_doreti,@function
_doreti:
	FAKE_MCOUNT(_bintr)		/* init "from" _bintr -> _doreti */
	addl	$4,%esp			/* discard unit number */
	popl	%eax			/* cpl or cml to restore */
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
#ifdef SMP
	cli				/* early to prevent INT deadlock */
doreti_next2:
#endif
	movl	%eax,%ecx
	notl	%ecx			/* set bit = unmasked level */
#ifndef SMP
	cli
#endif
	andl	_ipending,%ecx		/* set bit = unmasked pending INT */
	jne	doreti_unpend
	movl	%eax,_cpl
	decb	_intr_nesting_level

	/* Check for ASTs that can be handled now. */
	testl	$AST_PENDING,_astpending
	je	doreti_exit
	testb	$SEL_RPL_MASK,TF_CS(%esp)
	jne	doreti_ast
	testl	$PSL_VM,TF_EFLAGS(%esp)
	je	doreti_exit
	cmpl	$1,_in_vm86call
	jne	doreti_ast

	/*
	 * doreti_exit -	release MP lock, pop registers, iret.
	 *
	 *	Note that the syscall trap shotcuts to doreti_syscall_ret.
	 *	The segment register pop is a special case, since it may
	 *	fault if (for example) a sigreturn specifies bad segment
	 *	registers.  The fault is handled in trap.c
	 */

doreti_exit:
	MEXITCOUNT

#ifdef SMP
	/* release the kernel lock */
	movl	$_mp_lock, %edx		/* GIANT_LOCK */
	call	_MPrellock_edx
#endif /* SMP */

	.globl	doreti_popl_fs
	.globl	doreti_syscall_ret
doreti_syscall_ret:
doreti_popl_fs:
	popl	%fs
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
	pushl	%fs
	.globl	doreti_popl_fs_fault
doreti_popl_fs_fault:
	movl	$0,TF_ERR(%esp)	/* XXX should be the error code */
	movl	$T_PROTFLT,TF_TRAPNO(%esp)
	jmp	alltraps_with_regs_pushed

	ALIGN_TEXT
doreti_unpend:
	/*
	 * Enabling interrupts is safe because we haven't restored cpl yet.
	 * %ecx contains the next probable ready interrupt (~cpl & ipending)
	 */
#ifdef SMP
	bsfl	%ecx, %ecx		/* locate the next dispatchable int */
	lock
	btrl	%ecx, _ipending		/* is it really still pending? */
	jnc	doreti_next2		/* some intr cleared memory copy */
	sti				/* late to prevent INT deadlock */
#else
	sti
	bsfl	%ecx,%ecx		/* slow, but not worth optimizing */
	btrl	%ecx,_ipending
	jnc	doreti_next		/* some intr cleared memory copy */
#endif /* SMP */
	/*
	 * Execute handleable interrupt
	 *
	 * Set up JUMP to _ihandlers[%ecx] for HWIs.
	 * Set up CALL of _ihandlers[%ecx] for SWIs.
	 * This is a bit early for the SMP case - we have to push %ecx and
	 * %edx, but could push only %ecx and load %edx later.
	 */
	movl	_ihandlers(,%ecx,4),%edx
	cmpl	$NHWI,%ecx
	jae	doreti_swi		/* software interrupt handling */
	cli				/* else hardware int handling */
#ifdef SMP
	movl	%eax,_cpl		/* same as non-smp case right now */
#else
	movl	%eax,_cpl
#endif
	MEXITCOUNT
#ifdef APIC_INTR_DIAGNOSTIC
	lock
	incl	CNAME(apic_itrace_doreti)(,%ecx,4)
#ifdef APIC_INTR_DIAGNOSTIC_IRQ	
	cmpl	$APIC_INTR_DIAGNOSTIC_IRQ,%ecx
	jne	9f
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	pushl	$APIC_ITRACE_DORETI
	call	log_intr_event
	addl	$4,%esp
	popl	%edx
	popl	%ecx
	popl	%eax
9:	
#endif
#endif
	jmp	*%edx

	ALIGN_TEXT
doreti_swi:
	pushl	%eax
	/*
	 * At least the SWI_CLOCK handler has to run at a possibly strictly
	 * lower cpl, so we have to restore
	 * all the h/w bits in cpl now and have to worry about stack growth.
	 * The worst case is currently (30 Jan 1994) 2 SWI handlers nested
	 * in dying interrupt frames and about 12 HWIs nested in active
	 * interrupt frames.  There are only 4 different SWIs and the HWI
	 * and SWI masks limit the nesting further.
	 *
	 * The SMP case is currently the same as the non-SMP case.
	 */
#ifdef SMP
	orl	imasks(,%ecx,4), %eax	/* or in imasks */
	movl	%eax,_cpl		/* set cpl for call */
#else
	orl	imasks(,%ecx,4),%eax
	movl	%eax,_cpl
#endif
	call	*%edx
	popl	%eax			/* cpl to restore */
	jmp	doreti_next

	ALIGN_TEXT
doreti_ast:
	andl	$~AST_PENDING,_astpending
	sti
	movl	$T_ASTFLT,TF_TRAPNO(%esp)
	call	_trap
	subl	%eax,%eax		/* recover cpl|cml */
	movb	$1,_intr_nesting_level	/* for doreti_next to decrement */
	jmp	doreti_next

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
 * The arg is in a nonstandard place, so swi_dispatcher() can't be called
 * directly and swi_generic() can't use ENTRY() or MCOUNT.
 */
	ALIGN_TEXT
	.globl	_swi_generic
	.type	_swi_generic,@function
_swi_generic:
	pushl	%ecx
	FAKE_MCOUNT(4(%esp))
	call	_swi_dispatcher
	popl	%ecx
	ret

ENTRY(swi_null)
	ret

#ifdef APIC_IO
#include "i386/isa/apic_ipl.s"
#else
#include "i386/isa/icu_ipl.s"
#endif /* APIC_IO */
