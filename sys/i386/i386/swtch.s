/*-
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
 *	$Id: swtch.s,v 1.6 1994/04/20 07:06:18 davidg Exp $
 */

#include "npx.h"	/* for NNPX */
#include "assym.s"	/* for preprocessor defines */
#include "errno.h"	/* for error codes */

#include "machine/asmacros.h"	/* for miscellaneous assembly macros */
#define	LOCORE			/* XXX inhibit C declarations */
#include "machine/spl.h"	/* for SWI_AST_MASK ... */


/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrq puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_pri, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */
	.data
	.globl	_curpcb, _whichqs
_curpcb:	.long	0			/* pointer to curproc's PCB area */
_whichqs:	.long	0			/* which run queues have data */

	.globl	_qs,_cnt,_panic
	.comm	_noproc,4
	.comm	_runrun,4

	.globl	_want_resched
_want_resched:	.long	0			/* we need to re-run the scheduler */

	.text
/*
 * Setrq(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrq)
	movl	4(%esp),%eax
	cmpl	$0,P_RLINK(%eax)		/* should not be on q already */
	je	set1
	pushl	$set2
	call	_panic
set1:
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btsl	%edx,_whichqs			/* set q full bit */
	shll	$3,%edx
	addl	$_qs,%edx			/* locate q hdr */
	movl	%edx,P_LINK(%eax)		/* link process on tail of q */
	movl	P_RLINK(%edx),%ecx
	movl	%ecx,P_RLINK(%eax)
	movl	%eax,P_RLINK(%edx)
	movl	%eax,P_LINK(%ecx)
	ret

set2:	.asciz	"setrq"

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrq)
	movl	4(%esp),%eax
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btrl	%edx,_whichqs			/* clear full bit, panic if clear already */
	jb	rem1
	pushl	$rem3
	call	_panic
rem1:
	pushl	%edx
	movl	P_LINK(%eax),%ecx		/* unlink process */
	movl	P_RLINK(%eax),%edx
	movl	%edx,P_RLINK(%ecx)
	movl	P_RLINK(%eax),%ecx
	movl	P_LINK(%eax),%edx
	movl	%edx,P_LINK(%ecx)
	popl	%edx
	movl	$_qs,%ecx
	shll	$3,%edx
	addl	%edx,%ecx
	cmpl	P_LINK(%ecx),%ecx		/* q still has something? */
	je	rem2
	shrl	$3,%edx				/* yes, set bit as still full */
	btsl	%edx,_whichqs
rem2:
	movl	$0,P_RLINK(%eax)		/* zap reverse link to indicate off list */
	ret

rem3:	.asciz	"remrq"
sw0:	.asciz	"swtch"

/*
 * When no processes are on the runq, swtch() branches to _idle
 * to wait for something to come ready.
 */
	ALIGN_TEXT
_idle:
	MCOUNT
	movl	_IdlePTD,%ecx
	movl	%ecx,%cr3
	movl	$tmpstk-4,%esp
	sti

	/*
	 * XXX callers of swtch() do a bogus splclock().  Locking should
	 * be left to swtch().
	 */
	movl	$SWI_AST_MASK,_cpl
	testl	$~SWI_AST_MASK,_ipending
	je	idle_loop
	call	_splz

	ALIGN_TEXT
idle_loop:
	cli
	cmpl	$0,_whichqs
	jne	sw1a
	sti
	hlt					/* wait for interrupt */
	jmp	idle_loop

badsw:
	pushl	$sw0
	call	_panic
	/*NOTREACHED*/

/*
 * Swtch()
 */
ENTRY(swtch)
	incl	_cnt+V_SWTCH

	/* switch to new process. first, save context as needed */

	movl	_curproc,%ecx

	/* if no process to save, don't bother */
	testl	%ecx,%ecx
	je	sw1

	movl	P_ADDR(%ecx),%ecx

	movl	(%esp),%eax			/* Hardware registers */
	movl	%eax,PCB_EIP(%ecx)
	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)

#if NNPX > 0
	/* have we used fp, and need a save? */
	mov	_curproc,%eax
	cmp	%eax,_npxproc
	jne	1f
	pushl	%ecx				/* h/w bugs make saving complicated */
	leal	PCB_SAVEFPU(%ecx),%eax
	pushl	%eax
	call	_npxsave			/* do it in a big C function */
	popl	%eax
	popl	%ecx
1:
#endif	/* NNPX > 0 */

	movl	_CMAP2,%eax			/* save temporary map PTE */
	movl	%eax,PCB_CMAP2(%ecx)		/* in our context */
	movl	$0,_curproc			/*  out of process */

#	movw	_cpl,%ax
#	movw	%ax,PCB_IML(%ecx)		/* save ipl */

	/* save is done, now choose a new process or idle */
sw1:
	cli
sw1a:
	movl	_whichqs,%edi
2:
	/* XXX - bsf is sloow */
	bsfl	%edi,%eax			/* find a full q */
	je	_idle				/* if none, idle */

	/* XX update whichqs? */
	btrl	%eax,%edi			/* clear q full status */
	jnb	2b				/* if it was clear, look for another */
	movl	%eax,%ebx			/* save which one we are using */

	shll	$3,%eax
	addl	$_qs,%eax			/* select q */
	movl	%eax,%esi

#ifdef	DIAGNOSTIC
	cmpl	P_LINK(%eax),%eax 		/* linked to self? (e.g. not on list) */
	je	badsw				/* not possible */
#endif

	movl	P_LINK(%eax),%ecx		/* unlink from front of process q */
	movl	P_LINK(%ecx),%edx
	movl	%edx,P_LINK(%eax)
	movl	P_RLINK(%ecx),%eax
	movl	%eax,P_RLINK(%edx)

	cmpl	P_LINK(%ecx),%esi		/* q empty */
	je	3f
	btsl	%ebx,%edi			/* nope, set to indicate full */
3:
	movl	%edi,_whichqs			/* update q status */

	movl	$0,%eax
	movl	%eax,_want_resched

#ifdef	DIAGNOSTIC
	cmpl	%eax,P_WCHAN(%ecx)
	jne	badsw
	cmpb	$SRUN,P_STAT(%ecx)
	jne	badsw
#endif

	movl	%eax,P_RLINK(%ecx) 		/* isolate process to run */
	movl	P_ADDR(%ecx),%edx
	movl	PCB_CR3(%edx),%ebx

	/* switch address space */
	movl	%ebx,%cr3

	/* restore context */
	movl	PCB_EBX(%edx),%ebx
	movl	PCB_ESP(%edx),%esp
	movl	PCB_EBP(%edx),%ebp
	movl	PCB_ESI(%edx),%esi
	movl	PCB_EDI(%edx),%edi
	movl	PCB_EIP(%edx),%eax
	movl	%eax,(%esp)

	movl	PCB_CMAP2(%edx),%eax		/* get temporary map */
	movl	%eax,_CMAP2			/* reload temporary map PTE */

	movl	%ecx,_curproc			/* into next process */
	movl	%edx,_curpcb

#ifdef	USER_LDT
	cmpl	$0, PCB_USERLDT(%edx)
	jnz	1f
	movl	__default_ldt,%eax
	cmpl	_currentldt,%eax
	je	2f
	lldt	__default_ldt
	movl	%eax,_currentldt
	jmp	2f
1:	pushl	%edx
	call	_set_user_ldt
	popl	%edx
2:
#endif

	pushl	%edx				/* save p to return */
/*
 * XXX - 0.0 forgot to save it - is that why this was commented out in 0.1?
 * I think restoring the cpl is unnecessary, but we must turn off the cli
 * now that spl*() don't do it as a side affect.
 */
	pushl	PCB_IML(%edx)
	sti
#if 0
	call	_splx
#endif
	addl	$4,%esp
/*
 * XXX - 0.0 gets here via swtch_to_inactive().  I think 0.1 gets here in the
 * same way.  Better return a value.
 */
	popl	%eax				/* return(p); */
	ret

ENTRY(mvesp)
	movl	%esp,%eax
	ret
/*
 * struct proc *swtch_to_inactive(struct proc *p);
 *
 * At exit of a process, move off the address space of the
 * process and onto a "safe" one. Then, on a temporary stack
 * return and run code that disposes of the old state.
 * Since this code requires a parameter from the "old" stack,
 * pass it back as a return value.
 */
ENTRY(swtch_to_inactive)
	popl	%edx				/* old pc */
	popl	%eax				/* arg, our return value */
	movl	_IdlePTD,%ecx
	movl	%ecx,%cr3			/* good bye address space */
 #write buffer?
	movl	$tmpstk-4,%esp			/* temporary stack, compensated for call */
	MEXITCOUNT
	jmp	%edx				/* return, execute remainder of cleanup */

/*
 * savectx(pcb, altreturn)
 * Update pcb, saving current processor state and arranging
 * for alternate return ala longjmp in swtch if altreturn is true.
 */
ENTRY(savectx)
	movl	4(%esp),%ecx
	movw	_cpl,%ax
	movw	%ax,PCB_IML(%ecx)
	movl	(%esp),%eax
	movl	%eax,PCB_EIP(%ecx)
	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)

#if NNPX > 0
	/*
	 * If npxproc == NULL, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If npxproc != NULL, then we have to save the npx h/w state to
	 * npxproc's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's npx state for forks by forgetting to reload.
	 */
	mov	_npxproc,%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	P_ADDR(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	_npxsave
	popl	%eax
	popl	%eax
	popl	%ecx

	pushl	%ecx
	pushl	$108+8*2			/* XXX h/w state size + padding */
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	_bcopy
	addl	$12,%esp
	popl	%ecx
1:
#endif	/* NNPX > 0 */

	movl	_CMAP2,%edx			/* save temporary map PTE */
	movl	%edx,PCB_CMAP2(%ecx)		/* in our context */

	cmpl	$0,8(%esp)
	je	1f
	movl	%esp,%edx			/* relocate current sp relative to pcb */
	subl	$_kstack,%edx			/*   (sp is relative to kstack): */
	addl	%edx,%ecx			/*   pcb += sp - kstack; */
	movl	%eax,(%ecx)			/* write return pc at (relocated) sp@ */

/* this mess deals with replicating register state gcc hides */
	movl	12(%esp),%eax
	movl	%eax,12(%ecx)
	movl	16(%esp),%eax
	movl	%eax,16(%ecx)
	movl	20(%esp),%eax
	movl	%eax,20(%ecx)
	movl	24(%esp),%eax
	movl	%eax,24(%ecx)
1:
	xorl	%eax,%eax			/* return 0 */
	ret

/*
 * addupc(int pc, struct uprof *up, int ticks):
 * update profiling information for the user process.
 */
ENTRY(addupc)
	pushl %ebp
	movl %esp,%ebp
	movl 12(%ebp),%edx			/* up */
	movl 8(%ebp),%eax			/* pc */

	subl PR_OFF(%edx),%eax			/* pc -= up->pr_off */
	jb L1					/* if (pc was < off) return */

	shrl $1,%eax				/* praddr = pc >> 1 */
	imull PR_SCALE(%edx),%eax		/* praddr *= up->pr_scale */
	shrl $15,%eax				/* praddr = praddr << 15 */
	andl $-2,%eax				/* praddr &= ~1 */

	cmpl PR_SIZE(%edx),%eax			/* if (praddr > up->pr_size) return */
	ja L1

/*	addl %eax,%eax				/* praddr -> word offset */
	addl PR_BASE(%edx),%eax			/* praddr += up-> pr_base */
	movl 16(%ebp),%ecx			/* ticks */

	movl _curpcb,%edx
	movl $proffault,PCB_ONFAULT(%edx)
	addl %ecx,(%eax)			/* storage location += ticks */
	movl $0,PCB_ONFAULT(%edx)
L1:
	leave
	ret

	ALIGN_TEXT
proffault:
	/* if we get a fault, then kill profiling all together */
	movl $0,PCB_ONFAULT(%edx)		/* squish the fault handler */
	movl 12(%ebp),%ecx
	movl $0,PR_SCALE(%ecx)			/* up->pr_scale = 0 */
	leave
	ret
