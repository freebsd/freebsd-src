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
 *	$Id: swtch.s,v 1.52 1997/06/07 04:36:10 bde Exp $
 */

#include "npx.h"
#include "opt_user_ldt.h"

#include <sys/rtprio.h>

#include <machine/asmacros.h>
#include <machine/ipl.h>
#include <machine/smptests.h>		/** TEST_LOPRIO */

#if defined(SMP)
#include <machine/pmap.h>
#include <machine/apic.h>
#endif

#include "assym.s"


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
#ifndef SMP
	.globl	_curpcb
_curpcb:	.long	0			/* pointer to curproc's PCB area */
#endif
	.globl	_whichqs, _whichrtqs, _whichidqs

_whichqs:	.long	0			/* which run queues have data */
_whichrtqs:	.long	0			/* which realtime run queues have data */
_whichidqs:	.long	0			/* which idletime run queues have data */
	.globl	_hlt_vector
_hlt_vector:	.long	_default_halt		/* pointer to halt routine */


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
#ifdef DIAGNOSTIC
	cmpb	$SRUN,P_STAT(%eax)
	je	set1
	pushl	$set2
	call	_panic
set1:
#endif
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
	ret

rem3:	.asciz	"remrq"
rem3rt:	.asciz	"remrq.rt"
rem3id:	.asciz	"remrq.id"

/*
 * When no processes are on the runq, cpu_switch() branches to _idle
 * to wait for something to come ready.
 *
 * NOTE: on an SMP system this routine is a startup-only code path.
 * once initialization is over, meaning the idle procs have been
 * created, we should NEVER branch here.
 */
	ALIGN_TEXT
_idle:
#ifdef SMP
	movl	_smp_active, %eax
	cmpl	$0, %eax
	jnz	badsw3
#endif /* SMP */
	xorl	%ebp,%ebp
	movl	$HIDENAME(tmpstk),%esp
	movl	_IdlePTD,%ecx
	movl	%ecx,%cr3

	/* update common_tss.tss_esp0 pointer */
	movl	$_common_tss, %eax
	movl	%esp, TSS_ESP0(%eax)

#ifdef TSS_IS_CACHED				/* example only */
	/* Reload task register to force reload of selector */
	movl	_tssptr, %ebx
	andb	$~0x02, 5(%ebx)			/* Flip 386BSY -> 386TSS */
	movl	_gsel_tss, %ebx
	ltr	%bx
#endif

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
	CROSSJUMP(jne, sw1a, je)
	cmpl	$0,_whichqs			/* normal queue */
	CROSSJUMP(jne, nortqr, je)
	cmpl	$0,_whichidqs			/* 'idle' queue */
	CROSSJUMP(jne, idqr, je)
	movb	$0,_intr_nesting_level		/* charge Idle for this loop */
	call	_vm_page_zero_idle
	testl	%eax, %eax
	jnz	idle_loop
	sti
	call	*_hlt_vector			/* wait for interrupt */
	jmp	idle_loop

CROSSJUMPTARGET(_idle)

ENTRY(default_halt)
	hlt
	ret

/*
 * cpu_switch()
 */
ENTRY(cpu_switch)
	
	/* switch to new process. first, save context as needed */
	movl	_curproc,%ecx

	/* if no process to save, don't bother */
	testl	%ecx,%ecx
	je	sw1

#ifdef SMP
	movb	P_ONCPU(%ecx), %al		/* save "last" cpu */
	movb	%al, P_LASTCPU(%ecx)
	movb	$0xff, P_ONCPU(%ecx)		/* "leave" the cpu */
#endif

	movl	P_ADDR(%ecx),%ecx

	movl	(%esp),%eax			/* Hardware registers */
	movl	%eax,PCB_EIP(%ecx)
	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)
	movl	%fs,PCB_FS(%ecx)
	movl	%gs,PCB_GS(%ecx)

#ifdef SMP
	movl	_mp_lock, %eax
	cmpl	$0xffffffff, %eax		/* is it free? */
	je	badsw4				/* yes, bad medicine! */
	andl	$0x00ffffff, %eax		/* clear CPU portion */
	movl	%eax,PCB_MPNEST(%ecx)		/* store it */
#endif /* SMP */

#if NNPX > 0
	/* have we used fp, and need a save? */
	movl	_curproc,%eax
	cmpl	%eax,_npxproc
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
	CROSSJUMP(je, _idle, jne)		/* if no proc, idle */

	/* XX update whichqs? */
	btrl	%ebx,%edi			/* clear q full status */
	leal	_idqs(,%ebx,8),%eax		/* select q */
	movl	%eax,%esi

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
	jne	badsw1
	cmpb	$SRUN,P_STAT(%ecx)
	jne	badsw2
#endif

	movl	%eax,P_BACK(%ecx) 		/* isolate process to run */
	movl	P_ADDR(%ecx),%edx
	movl	PCB_CR3(%edx),%ebx

#if defined(SMP)
	/* Grab the private PT pointer from the outgoing process's PTD */
	movl	$_PTD,%esi
	movl	4*MPPTDI(%esi), %eax		/* fetch cpu's prv pt */
#endif

	/* switch address space */
	movl	%ebx,%cr3

#if defined(SMP)
	/* Copy the private PT to the new process's PTD */
	/* XXX yuck, the _PTD changes when we switch, so we have to
	 * reload %cr3 after changing the address space.
	 * We need to fix this by storing a pointer to the virtual
	 * location of the per-process PTD in the PCB or something quick.
	 * Dereferencing proc->vm_map->pmap->p_pdir[] is painful in asm.
	 */
	movl	%eax, 4*MPPTDI(%esi)		/* restore cpu's prv page */

	/* XXX: we have just changed the page tables.. reload.. */
	movl	%ebx,%cr3
#endif

#ifdef HOW_TO_SWITCH_TSS			/* example only */
	/* Fix up tss pointer to floating pcb/stack structure */
	/* XXX probably lots faster to store the 64 bits of tss entry
	 * in the pcb somewhere and copy them on activation.
	 */
	movl	_tssptr, %ebx
	movl	%edx, %eax			/* edx = pcb/tss */
	movw	%ax, 2(%ebx)			/* store bits 0->15 */
	roll	$16, %eax			/* swap upper and lower */
	movb	%al, 4(%ebx)			/* store bits 16->23 */
	movb	%ah, 7(%ebx)			/* store bits 24->31 */
	andb	$~0x02, 5(%ebx)			/* Flip 386BSY -> 386TSS */
#endif

	/* update common_tss.tss_esp0 pointer */
	movl	$_common_tss, %eax
	movl	%edx, %ebx			/* pcb */
	addl	$(UPAGES * PAGE_SIZE), %ebx
	movl	%ebx, TSS_ESP0(%eax)

#ifdef TSS_IS_CACHED				/* example only */
	/* Reload task register to force reload of selector */
	movl	_tssptr, %ebx
	andb	$~0x02, 5(%ebx)			/* Flip 386BSY -> 386TSS */
	movl	_gsel_tss, %ebx
	ltr	%bx
#endif

	/* restore context */
	movl	PCB_EBX(%edx),%ebx
	movl	PCB_ESP(%edx),%esp
	movl	PCB_EBP(%edx),%ebp
	movl	PCB_ESI(%edx),%esi
	movl	PCB_EDI(%edx),%edi
	movl	PCB_EIP(%edx),%eax
	movl	%eax,(%esp)

#ifdef SMP
	movl	_cpuid,%eax
	movb	%al, P_ONCPU(%ecx)
#endif
	movl	%edx,_curpcb
	movl	%ecx,_curproc			/* into next process */

	movb	$0,_intr_nesting_level
#ifdef SMP
#if defined(TEST_LOPRIO)
	/* Set us to prefer to get irq's from the apic since we have the lock */
	movl	lapic_tpr, %eax			/* get TPR register contents */
	andl	$0xffffff00, %eax		/* clear the prio field */
	movl	%eax, lapic_tpr			/* now hold loprio for INTs */
#endif /* TEST_LOPRIO */
	movl	_cpu_lockid,%eax
	orl	PCB_MPNEST(%edx), %eax		/* add next count from PROC */
	movl	%eax, _mp_lock			/* load the mp_lock */
#endif /* SMP */

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

	/* This must be done after loading the user LDT. */
	.globl	cpu_switch_load_fs
cpu_switch_load_fs:
	movl	PCB_FS(%edx),%fs
	.globl	cpu_switch_load_gs
cpu_switch_load_gs:
	movl	PCB_GS(%edx),%gs

	sti
	ret

CROSSJUMPTARGET(idqr)
CROSSJUMPTARGET(nortqr)
CROSSJUMPTARGET(sw1a)

#ifdef DIAGNOSTIC
badsw1:
	pushl	$sw0_1
	call	_panic

sw0_1:	.asciz	"cpu_switch: has wchan"

badsw2:
	pushl	$sw0_2
	call	_panic

sw0_2:	.asciz	"cpu_switch: not SRUN"
#endif

#ifdef SMP
badsw3:
	pushl	$sw0_3
	call	_panic

sw0_3:	.asciz	"cpu_switch: went idle with smp_active"

badsw4:
	pushl	$sw0_4
	call	_panic

sw0_4:	.asciz	"cpu_switch: do not have lock"
#endif

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

	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)
	movl	%fs,PCB_FS(%ecx)
	movl	%gs,PCB_GS(%ecx)

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
	movl	_npxproc,%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	P_ADDR(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	_npxsave
	addl	$4,%esp
	popl	%eax
	popl	%ecx

	pushl	$PCB_SAVEFPU_SIZE
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	_bcopy
	addl	$12,%esp
#endif	/* NNPX > 0 */

1:
	ret
