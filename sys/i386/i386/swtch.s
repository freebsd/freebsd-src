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
 *	$Id: swtch.s,v 1.73 1998/05/19 20:59:07 dufault Exp $
 */

#include "npx.h"
#include "opt_user_ldt.h"
#include "opt_vm86.h"

#include <sys/rtprio.h>

#include <machine/asmacros.h>

#ifdef SMP
#include <machine/pmap.h>
#include <machine/apic.h>
#include <machine/smptests.h>		/** GRAB_LOPRIO */
#include <machine/ipl.h>
#include <machine/lock.h>
#endif /* SMP */

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

	.globl	_whichqs, _whichrtqs, _whichidqs

_whichqs:	.long	0		/* which run queues have data */
_whichrtqs:	.long	0		/* which realtime run qs have data */
_whichidqs:	.long	0		/* which idletime run qs have data */

	.globl	_hlt_vector
_hlt_vector:	.long	_default_halt	/* pointer to halt routine */

	.globl	_qs,_cnt,_panic

	.globl	_want_resched
_want_resched:	.long	0		/* we need to re-run the scheduler */
#if defined(SWTCH_OPTIM_STATS)
	.globl	_swtch_optim_stats, _tlb_flush_count
_swtch_optim_stats:	.long	0		/* number of _swtch_optims */
_tlb_flush_count:	.long	0
#endif

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

	cmpw	$RTP_PRIO_REALTIME,P_RTPRIO_TYPE(%eax) /* RR realtime priority? */
	je	set_rt				/* RT priority */
	cmpw	$RTP_PRIO_FIFO,P_RTPRIO_TYPE(%eax) /* FIFO realtime priority? */
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

	cmpw	$RTP_PRIO_REALTIME,P_RTPRIO_TYPE(%eax) /* realtime priority process? */
	je	rem0rt
	cmpw	$RTP_PRIO_FIFO,P_RTPRIO_TYPE(%eax) /* FIFO realtime priority process? */
	jne	rem_id
		
rem0rt:
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
 */
	ALIGN_TEXT
_idle:
	xorl	%eax,%eax
	movl	%eax, _switchtime
	movl	%eax, _switchtime+4

#ifdef SMP
	/* when called, we have the mplock, intr disabled */

	xorl	%ebp,%ebp

	/* use our idleproc's "context" */
	movl	_my_idlePTD,%ecx
	movl	%ecx,%cr3
#if defined(SWTCH_OPTIM_STATS)
	incl	_tlb_flush_count
#endif
	/* Keep space for nonexisting return addr, or profiling bombs */
	movl	$_idlestack_top-4,%ecx	
	movl	%ecx,%esp

	/* update common_tss.tss_esp0 pointer */
#ifdef VM86
	movl	_my_tr, %esi
#endif /* VM86 */
	movl	%ecx, _common_tss + TSS_ESP0

#ifdef VM86
	btrl	%esi, _private_tss
	je	1f
	movl	$_common_tssd, %edi

	/* move correct tss descriptor into GDT slot, then reload tr */
	leal	_gdt(,%esi,8), %ebx		/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	shll	$3, %esi			/* GSEL(entry, SEL_KPL) */
	ltr	%si
1:
#endif /* VM86 */

	sti

	/*
	 * XXX callers of cpu_switch() do a bogus splclock().  Locking should
	 * be left to cpu_switch().
	 */
	call	_spl0

	cli

	/*
	 * _REALLY_ free the lock, no matter how deep the prior nesting.
	 * We will recover the nesting on the way out when we have a new
	 * proc to load.
	 *
	 * XXX: we had damn well better be sure we had it before doing this!
	 */
	CPL_LOCK			/* XXX */
	andl	$~SWI_AST_MASK, _ipending 			/* XXX */
	movl	$0, _cpl	/* XXX Allow ASTs on other CPU */
	CPL_UNLOCK			/* XXX */
	movl	$FREE_LOCK, %eax
	movl	%eax, _mp_lock

	/* do NOT have lock, intrs disabled */
	.globl	idle_loop
idle_loop:

#if defined(SWTCH_OPTIM_STATS)
	incl	_tlb_flush_count
#endif
	movl	%cr3,%eax			/* ouch! */
	movl	%eax,%cr3

	cmpl	$0,_smp_active
	jne	1f
	cmpl	$0,_cpuid
	je	1f
	jmp	2f

1:	cmpl	$0,_whichrtqs			/* real-time queue */
	jne	3f
	cmpl	$0,_whichqs			/* normal queue */
	jne	3f
	cmpl	$0,_whichidqs			/* 'idle' queue */
	jne	3f

	cmpl	$0,_do_page_zero_idle
	je	2f

	/* XXX appears to cause panics */
	/*
	 * Inside zero_idle we enable interrupts and grab the mplock
	 * as needed.  It needs to be careful about entry/exit mutexes.
	 */
	call	_vm_page_zero_idle		/* internal locking */
	testl	%eax, %eax
	jnz	idle_loop
2:

	/* enable intrs for a halt */
#ifdef SMP
	movl	$0, lapic_tpr			/* 1st candidate for an INT */
#endif
	sti
	call	*_hlt_vector			/* wait for interrupt */
	cli
	jmp	idle_loop

3:
	movl	$LOPRIO_LEVEL, lapic_tpr	/* arbitrate for INTs */
	call	_get_mplock
	CPL_LOCK					/* XXX */
	movl	$SWI_AST_MASK, _cpl	/* XXX Disallow ASTs on other CPU */
	CPL_UNLOCK					/* XXX */
	cmpl	$0,_whichrtqs			/* real-time queue */
	CROSSJUMP(jne, sw1a, je)
	cmpl	$0,_whichqs			/* normal queue */
	CROSSJUMP(jne, nortqr, je)
	cmpl	$0,_whichidqs			/* 'idle' queue */
	CROSSJUMP(jne, idqr, je)
	CPL_LOCK				/* XXX */
	movl	$0, _cpl		/* XXX Allow ASTs on other CPU */
	CPL_UNLOCK				/* XXX */
	call	_rel_mplock
	jmp	idle_loop

#else
	xorl	%ebp,%ebp
	movl	$HIDENAME(tmpstk),%esp
#if defined(OVERLY_CONSERVATIVE_PTD_MGMT)
#if defined(SWTCH_OPTIM_STATS)
	incl	_swtch_optim_stats
#endif
	movl	_IdlePTD, %ecx
	movl	%cr3, %eax
	cmpl	%ecx, %eax
	je		2f
#if defined(SWTCH_OPTIM_STATS)
	decl	_swtch_optim_stats
	incl	_tlb_flush_count
#endif
	movl	%ecx, %cr3
2:
#endif

	/* update common_tss.tss_esp0 pointer */
#ifdef VM86
	movl	_my_tr, %esi
#endif /* VM86 */
	movl	%esp, _common_tss + TSS_ESP0

#ifdef VM86
	btrl	%esi, _private_tss
	je	1f
	movl	$_common_tssd, %edi

	/* move correct tss descriptor into GDT slot, then reload tr */
	leal	_gdt(,%esi,8), %ebx		/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	shll	$3, %esi			/* GSEL(entry, SEL_KPL) */
	ltr	%si
1:
#endif /* VM86 */

	sti

	/*
	 * XXX callers of cpu_switch() do a bogus splclock().  Locking should
	 * be left to cpu_switch().
	 */
	call	_spl0

	ALIGN_TEXT
idle_loop:
	cli
	cmpl	$0,_whichrtqs			/* real-time queue */
	CROSSJUMP(jne, sw1a, je)
	cmpl	$0,_whichqs			/* normal queue */
	CROSSJUMP(jne, nortqr, je)
	cmpl	$0,_whichidqs			/* 'idle' queue */
	CROSSJUMP(jne, idqr, je)
	call	_vm_page_zero_idle
	testl	%eax, %eax
	jnz	idle_loop
	sti
	call	*_hlt_vector			/* wait for interrupt */
	jmp	idle_loop
#endif

CROSSJUMPTARGET(_idle)

ENTRY(default_halt)
#ifndef SMP
	hlt					/* XXX:	 until a wakeup IPI */
#endif
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
#endif /* SMP */

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
	/* XXX FIXME: we should be saving the local APIC TPR */
#ifdef DIAGNOSTIC
	cmpl	$FREE_LOCK, %eax		/* is it free? */
	je	badsw4				/* yes, bad medicine! */
#endif /* DIAGNOSTIC */
	andl	$COUNT_FIELD, %eax		/* clear CPU portion */
	movl	%eax, PCB_MPNEST(%ecx)		/* store it */
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

	movl	$0,_curproc			/* out of process */

	/* save is done, now choose a new process or idle */
sw1:
	cli

#ifdef SMP
	/* Stop scheduling if smp_active goes zero and we are not BSP */
	cmpl	$0,_smp_active
	jne	1f
	cmpl	$0,_cpuid
	je	1f
	CROSSJUMP(je, _idle, jne)		/* wind down */
1:
#endif

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

#ifdef SMP
	movl	PCB_CR3(%edx),%ebx
	/* Grab the private PT pointer from the outgoing process's PTD */
	movl	$_PTD, %esi
	movl	4*MPPTDI(%esi), %eax		/* fetch cpu's prv pt */
#else
#if defined(SWTCH_OPTIM_STATS)
	incl	_swtch_optim_stats
#endif
	/* switch address space */
	movl	%cr3,%ebx
	cmpl	PCB_CR3(%edx),%ebx
	je		4f
#if defined(SWTCH_OPTIM_STATS)
	decl	_swtch_optim_stats
	incl	_tlb_flush_count
#endif
	movl	PCB_CR3(%edx),%ebx
#endif /* SMP */
	movl	%ebx,%cr3
4:

#ifdef SMP
	/* Copy the private PT to the new process's PTD */
	/* XXX yuck, the _PTD changes when we switch, so we have to
	 * reload %cr3 after changing the address space.
	 * We need to fix this by storing a pointer to the virtual
	 * location of the per-process PTD in the PCB or something quick.
	 * Dereferencing proc->vm_map->pmap->p_pdir[] is painful in asm.
	 */
	movl	%eax, 4*MPPTDI(%esi)		/* restore cpu's prv page */

#if defined(SWTCH_OPTIM_STATS)
	incl	_tlb_flush_count
#endif
	/* XXX: we have just changed the page tables.. reload.. */
	movl	%ebx, %cr3
#endif /* SMP */

#ifdef VM86
	movl	_my_tr, %esi
	cmpl	$0, PCB_EXT(%edx)		/* has pcb extension? */
	je	1f
	btsl	%esi, _private_tss		/* mark use of private tss */
	movl	PCB_EXT(%edx), %edi		/* new tss descriptor */
	jmp	2f
1:
#endif

	/* update common_tss.tss_esp0 pointer */
	movl	$_common_tss, %eax
	movl	%edx, %ebx			/* pcb */
#ifdef VM86
	addl	$(UPAGES * PAGE_SIZE - 16), %ebx
#else
	addl	$(UPAGES * PAGE_SIZE), %ebx
#endif /* VM86 */
	movl	%ebx, TSS_ESP0(%eax)

#ifdef VM86
	btrl	%esi, _private_tss
	je	3f
	movl	$_common_tssd, %edi
2:
	/* move correct tss descriptor into GDT slot, then reload tr */
	leal	_gdt(,%esi,8), %ebx		/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	shll	$3, %esi			/* GSEL(entry, SEL_KPL) */
	ltr	%si
3:
#endif /* VM86 */

	/* restore context */
	movl	PCB_EBX(%edx),%ebx
	movl	PCB_ESP(%edx),%esp
	movl	PCB_EBP(%edx),%ebp
	movl	PCB_ESI(%edx),%esi
	movl	PCB_EDI(%edx),%edi
	movl	PCB_EIP(%edx),%eax
	movl	%eax,(%esp)

#ifdef SMP
#ifdef GRAB_LOPRIO				/* hold LOPRIO for INTs */
#ifdef CHEAP_TPR
	movl	$0, lapic_tpr
#else
	andl	$~APIC_TPR_PRIO, lapic_tpr
#endif /** CHEAP_TPR */
#endif /** GRAB_LOPRIO */
	movl	_cpuid,%eax
	movb	%al, P_ONCPU(%ecx)
#endif /* SMP */
	movl	%edx, _curpcb
	movl	%ecx, _curproc			/* into next process */

#ifdef SMP
	movl	_cpu_lockid, %eax
	orl	PCB_MPNEST(%edx), %eax		/* add next count from PROC */
	movl	%eax, _mp_lock			/* load the mp_lock */
	/* XXX FIXME: we should be restoring the local APIC TPR */
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

#if defined(SMP) && defined(DIAGNOSTIC)
badsw4:
	pushl	$sw0_4
	call	_panic

sw0_4:	.asciz	"cpu_switch: do not have lock"
#endif /* SMP && DIAGNOSTIC */

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
