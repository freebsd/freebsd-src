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
 * $FreeBSD$
 */


/*
 * AT/386
 * Vector interrupt control section
 *
 *  *_imask	- Interrupt masks for various spl*() functions
 *  ipending	- Pending interrupts (set when a masked interrupt occurs)
 */

	.data
	ALIGN_DATA

/* current priority (all off) */

	.globl	_tty_imask
_tty_imask:	.long	0
	.globl	_bio_imask
_bio_imask:	.long	0
	.globl	_net_imask
_net_imask:	.long	0
	.globl	_cam_imask
_cam_imask:	.long	0
	.globl	_soft_imask
_soft_imask:	.long	0
	.globl	_softnet_imask
_softnet_imask:	.long	0
	.globl	_softtty_imask
_softtty_imask:	.long	0

/* pending software interrupts */
	.globl	_spending
_spending:	.long	0

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
 * void doreti(struct trapframe)
 *
 * Handle return from interrupts, traps and syscalls.
 */
	SUPERALIGN_TEXT
	.type	_doreti,@function
_doreti:
	FAKE_MCOUNT(_bintr)		/* init "from" _bintr -> _doreti */
doreti_next:
	decb	_intr_nesting_level

	/* Check for ASTs that can be handled now. */
	testl	$AST_PENDING,_astpending
	je	doreti_exit		/* no AST, exit */
	testb	$SEL_RPL_MASK,TF_CS(%esp)  /* are we in user mode? */
	jne	doreti_ast		/* yes, do it now. */
	testl	$PSL_VM,TF_EFLAGS(%esp) /* kernel mode */
	je	doreti_exit		/* and not VM86 mode, defer */
	cmpl	$1,_in_vm86call		/* are we in a VM86 call? */
	jne	doreti_ast		/* yes, we can do it */

	/*
	 * doreti_exit:	release MP lock, pop registers, iret.
	 *
	 *	Note that the syscall trap shortcuts to doreti_syscall_ret.
	 *	The segment register pop is a special case, since it may
	 *	fault if (for example) a sigreturn specifies bad segment
	 *	registers.  The fault is handled in trap.c.
	 */
doreti_exit:
	MEXITCOUNT

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

  	/*
	 * doreti_iret_fault and friends.  Alternative return code for
	 * the case where we get a fault in the doreti_exit code
	 * above.  trap() (i386/i386/trap.c) catches this specific
	 * case, sends the process a signal and continues in the
	 * corresponding place in the code below.
	 */
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
doreti_ast:
	andl	$~AST_PENDING,_astpending
	sti
	movl	$T_ASTFLT,TF_TRAPNO(%esp)
	call	_ast
	movb	$1,_intr_nesting_level	/* for doreti_next to decrement */
	jmp	doreti_next

	ALIGN_TEXT
dummynetisr:
	MCOUNT
	ret	



#ifdef APIC_IO
#include "i386/isa/apic_ipl.s"
#else
#include "i386/isa/icu_ipl.s"
#endif /* APIC_IO */
