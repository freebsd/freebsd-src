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
 *	$Id: swtch.s,v 1.30 1996/03/12 05:44:23 nate Exp $
 */

#include "npx.h"	/* for NNPX */
#include "opt_user_ldt.h" /* for USER_LDT */
#include "assym.s"	/* for preprocessor defines */
#include "apm.h"	/* for NAPM */
#include <sys/errno.h>	/* for error codes */

#include <machine/asmacros.h>	/* for miscellaneous assembly macros */
#include <machine/spl.h>	/* for SWI_AST_MASK ... */
#include <sys/rtprio.h>


/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  setrunqueue puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_priority, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */
	.data
	.globl	_curpcb
_curpcb:	.long	0			/* pointer to curproc's PCB area */
_whichqs:	.long	0			/* which run queues have data */
_whichrtqs:	.long	0			/* which realtime run queues have data */
_whichidqs:	.long	0			/* which idletime run queues have data */

	.globl	_qs,_cnt,_panic

	.globl	_want_resched
_want_resched:	.long	0			/* we need to re-run the scheduler */

	.text
/*
 * setrunqueue(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrunqueue)
	movl	4(%esp),%eax
	cmpl	$0,P_BACK(%eax)			/* should not be on q already */
	je	set1
	pushl	$set2
	call	_panic
set1:
	cmpw	$RTP_PRIO_NORMAL,P_RTPRIO_TYPE(%eax) /* normal priority process? */
	je	set_nort

	movzwl	P_RTPRIO_PRIO(%eax),%edx

	cmpw	$RTP_PRIO_REALTIME,P_RTPRIO_TYPE(%eax) /* realtime priority? */
	jne	set_id				/* must be idle priority */
	
set_rt:
	btsl	%edx,_whichrtqs			/* set q full bit */
	shll	$3,%edx
	addl	$_rtqs,%edx			/* locate q hdr */
	movl	%edx,P_FORW(%eax)		/* link process on tail of q */
	movl	P_BACK(%edx),%ecx
	movl	%ecx,P_BACK(%eax)
	movl	%eax,P_BACK(%edx)
	movl	%eax,P_FORW(%ecx)
	ret

set_id:	
	btsl	%edx,_whichidqs			/* set q full bit */
	shll	$3,%edx
	addl	$_idqs,%edx			/* locate q hdr */
	movl	%edx,P_FORW(%eax)		/* link process on tail of q */
	movl	P_BACK(%edx),%ecx
	movl	%ecx,P_BACK(%eax)
	movl	%eax,P_BACK(%edx)
	movl	%eax,P_FORW(%ecx)
	ret

set_nort:                    			/*  Normal (RTOFF) code */
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btsl	%edx,_whichqs			/* set q full bit */
	shll	$3,%edx
	addl	$_qs,%edx			/* locate q hdr */
	movl	%edx,P_FORW(%eax)		/* link process on tail of q */
	movl	P_BACK(%edx),%ecx
	movl	%ecx,P_BACK(%eax)
	movl	%eax,P_BACK(%edx)
	movl	%eax,P_FORW(%ecx)
	ret

set2:	.asciz	"setrunqueue"

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrq)
	movl	4(%esp),%eax
	cmpw	$RTP_PRIO_NORMAL,P_RTPRIO_TYPE(%eax) /* normal priority process? */
	je	rem_nort

	movzwl	P_RTPRIO_PRIO(%eax),%edx

	cmpw	$RTP_PRIO_REALTIME,P_RTPRIO_TYPE(%eax) /* normal priority process? */
	jne	rem_id
		
	btrl	%edx,_whichrtqs			/* clear full bit, panic if clear already */
	jb	rem1rt
	pushl	$rem3rt
	call	_panic
rem1rt:
	pushl	%edx
	movl	P_FORW(%eax),%ecx		/* unlink process */
	movl	P_BACK(%eax),%edx
	movl	%edx,P_BACK(%ecx)
	movl	P_BACK(%eax),%ecx
	movl	P_FORW(%eax),%edx
	movl	%edx,P_FORW(%ecx)
	popl	%edx
	movl	$_rtqs,%ecx
	shll	$3,%edx
	addl	%edx,%ecx
	cmpl	P_FORW(%ecx),%ecx		/* q still has something? */
	je	rem2rt
	shrl	$3,%edx				/* yes, set bit as still full */
	btsl	%edx,_whichrtqs
rem2rt:
	movl	$0,P_BACK(%eax)			/* zap reverse link to indicate off list */
	ret
rem_id:
	btrl	%edx,_whichidqs			/* clear full bit, panic if clear already */
	jb	rem1id
	pushl	$rem3id
	call	_panic
rem1id:
	pushl	%edx
	movl	P_FORW(%eax),%ecx		/* unlink process */
	movl	P_BACK(%eax),%edx
	movl	%edx,P_BACK(%ecx)
	movl	P_BACK(%eax),%ecx
	movl	P_FORW(%eax),%edx
	movl	%edx,P_FORW(%ecx)
	popl	%edx
	movl	$_idqs,%ecx
	shll	$3,%edx
	addl	%edx,%ecx
	cmpl	P_FORW(%ecx),%ecx		/* q still has something? */
	je	rem2id
	shrl	$3,%edx				/* yes, set bit as still full */
	btsl	%edx,_whichidqs
rem2id:
	movl	$0,P_BACK(%eax)			/* zap reverse link to indicate off list */
	ret

rem_nort:     
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btrl	%edx,_whichqs			/* clear full bit, panic if clear already */
	jb	rem1
	pushl	$rem3
	call	_panic
rem1:
	pushl	%edx
	movl	P_FORW(%eax),%ecx		/* unlink process */
	movl	P_BACK(%eax),%edx
	movl	%edx,P_BACK(%ecx)
	movl	P_BACK(%eax),%ecx
	movl	P_FORW(%eax),%edx
	movl	%edx,P_FORW(%ecx)
	popl	%edx
	movl	$_qs,%ecx
	shll	$3,%edx
	addl	%edx,%ecx
	cmpl	P_FORW(%ecx),%ecx		/* q still has something? */
	je	rem2
	shrl	$3,%edx				/* yes, set bit as still full */
	btsl	%edx,_whichqs
rem2:
	movl	$0,P_BACK(%eax)			/* zap reverse link to indicate off list */
	ret

rem3:	.asciz	"remrq"
rem3rt:	.asciz	"remrq.rt"
rem3id:	.asciz	"remrq.id"
sw0:	.asciz	"cpu_switch"

/*
 * When no processes are on the runq, cpu_switch() branches to _idle
 * to wait for something to come ready.
 */
	ALIGN_TEXT
_idle:
	MCOUNT
	xorl	%ebp,%ebp
	movl	$tmpstk,%esp
	movl	_IdlePTD,%ecx
	movl	%ecx,%cr3
	sti

	/*
	 * XXX callers of cpu_switch() do a bogus splclock().  Locking should
	 * be left to cpu_switch().
	 */
	movl	$SWI_AST_MASK,_cpl
	testl	$~SWI_AST_MASK,_ipending
	je	idle_loop
	call	_splz

	ALIGN_TEXT
idle_loop:
	cli
	movb	$1,_intr_nesting_level		/* charge Intr if we leave */
	cmpl	$0,_whichrtqs			/* real-time queue */
	jne	sw1a
	cmpl	$0,_whichqs			/* normal queue */
	jne	nortqr
	cmpl	$0,_whichidqs			/* 'idle' queue */
	jne	idqr
	movb	$0,_intr_nesting_level		/* charge Idle for this loop */
	call	_vm_page_zero_idle
	testl	%eax, %eax
	jnz	idle_loop
	sti
#if NAPM > 0
	call    _apm_cpu_idle
	call    _apm_cpu_busy
#else
	hlt					/* wait for interrupt */
#endif
	jmp	idle_loop

badsw:
	pushl	$sw0
	call	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 */
ENTRY(cpu_switch)
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

	movb	_intr_nesting_level,%al
	movb	%al,PCB_INL(%ecx)

#if NNPX > 0
	/* have we used fp, and need a save? */
	mov	_curproc,%eax
	cmp	%eax,_npxproc
	jne	1f
	addl	$PCB_SAVEFPU,%ecx		/* h/w bugs make saving complicated */
	pushl	%ecx
	call	_npxsave			/* do it in a big C function */
	popl	%eax
1:
#endif	/* NNPX > 0 */

	movb	$1,_intr_nesting_level		/* charge Intr, not Sys/Idle */

	movl	$0,_curproc			/* out of process */

	/* save is done, now choose a new process or idle */
sw1:
	cli
sw1a:
	movl    _whichrtqs,%edi			/* pick next p. from rtqs */
	testl	%edi,%edi
	jz	nortqr				/* no realtime procs */

	/* XXX - bsf is sloow */
	bsfl	%edi,%ebx			/* find a full q */
	jz	nortqr				/* no proc on rt q - try normal ... */

	/* XX update whichqs? */
	btrl	%ebx,%edi			/* clear q full status */
	leal	_rtqs(,%ebx,8),%eax		/* select q */
	movl	%eax,%esi

#ifdef        DIAGNOSTIC
	cmpl	P_FORW(%eax),%eax		/* linked to self? (e.g. not on list) */
	je	badsw				/* not possible */
#endif

	movl	P_FORW(%eax),%ecx		/* unlink from front of process q */
	movl	P_FORW(%ecx),%edx
	movl	%edx,P_FORW(%eax)
	movl	P_BACK(%ecx),%eax
	movl	%eax,P_BACK(%edx)

	cmpl	P_FORW(%ecx),%esi		/* q empty */
	je	rt3
	btsl	%ebx,%edi			/* nope, set to indicate not empty */
rt3:
	movl	%edi,_whichrtqs			/* update q status */
	jmp	swtch_com

	/* old sw1a */
/* Normal process priority's */
nortqr:
	movl	_whichqs,%edi
2:
	/* XXX - bsf is sloow */
	bsfl	%edi,%ebx			/* find a full q */
	jz	idqr				/* if none, idle */

	/* XX update whichqs? */
	btrl	%ebx,%edi			/* clear q full status */
	leal	_qs(,%ebx,8),%eax		/* select q */
	movl	%eax,%esi

#ifdef	DIAGNOSTIC
	cmpl	P_FORW(%eax),%eax 		/* linked to self? (e.g. not on list) */
	je	badsw				/* not possible */
#endif

	movl	P_FORW(%eax),%ecx		/* unlink from front of process q */
	movl	P_FORW(%ecx),%edx
	movl	%edx,P_FORW(%eax)
	movl	P_BACK(%ecx),%eax
	movl	%eax,P_BACK(%edx)

	cmpl	P_FORW(%ecx),%esi		/* q empty */
	je	3f
	btsl	%ebx,%edi			/* nope, set to indicate not empty */
3:
	movl	%edi,_whichqs			/* update q status */
	jmp	swtch_com

idqr: /* was sw1a */
	movl    _whichidqs,%edi			/* pick next p. from idqs */

	/* XXX - bsf is sloow */
	bsfl	%edi,%ebx			/* find a full q */
	jz	_idle				/* no proc, idle */

	/* XX update whichqs? */
	btrl	%ebx,%edi			/* clear q full status */
	leal	_idqs(,%ebx,8),%eax		/* select q */
	movl	%eax,%esi

#ifdef        DIAGNOSTIC
	cmpl	P_FORW(%eax),%eax		/* linked to self? (e.g. not on list) */
	je	badsw				/* not possible */
#endif

	movl	P_FORW(%eax),%ecx		/* unlink from front of process q */
	movl	P_FORW(%ecx),%edx
	movl	%edx,P_FORW(%eax)
	movl	P_BACK(%ecx),%eax
	movl	%eax,P_BACK(%edx)

	cmpl	P_FORW(%ecx),%esi		/* q empty */
	je	id3
	btsl	%ebx,%edi			/* nope, set to indicate not empty */
id3:
	movl	%edi,_whichidqs			/* update q status */

swtch_com:
	movl	$0,%eax
	movl	%eax,_want_resched

#ifdef	DIAGNOSTIC
	cmpl	%eax,P_WCHAN(%ecx)
	jne	badsw
	cmpb	$SRUN,P_STAT(%ecx)
	jne	badsw
#endif

	movl	%eax,P_BACK(%ecx) 		/* isolate process to run */
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

	movl	%edx,_curpcb
	movl	%ecx,_curproc			/* into next process */

	movb	PCB_INL(%edx),%al
	movb	%al,_intr_nesting_level

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

	sti
	ret

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* fetch PCB */
	movl	4(%esp),%ecx

	/* caller's return address - child won't execute this routine */
	movl	(%esp),%eax
	movl	%eax,PCB_EIP(%ecx)

	movl	$1,PCB_EAX(%ecx)		/* return 1 in child */
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
#endif	/* NNPX > 0 */

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
