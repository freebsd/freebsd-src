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
 * $FreeBSD$
 */

#include "opt_npx.h"
#include "opt_sched.h"

#include <machine/asmacros.h>

#include "assym.s"

#if defined(SMP) && defined(SCHED_ULE)
#define	SETOP		xchgl
#define	BLOCK_SPIN(reg)							\
		movl		$blocked_lock,%eax ;			\
	100: ;								\
		lock ;							\
		cmpxchgl	%eax,TD_LOCK(reg) ;			\
		jne		101f ;					\
		pause ;							\
		jmp		100b ;					\
	101:
#else
#define	SETOP		movl
#define	BLOCK_SPIN(reg)
#endif

/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.text

/*
 * cpu_throw()
 *
 * This is the second half of cpu_switch(). It is used when the current
 * thread is either a dummy or slated to die, and we no longer care
 * about its state.  This is only a slight optimization and is probably
 * not worth it anymore.  Note that we need to clear the pm_active bits so
 * we do need the old proc if it still exists.
 * 0(%esp) = ret
 * 4(%esp) = oldtd
 * 8(%esp) = newtd
 */
ENTRY(cpu_throw)
	movl	PCPU(CPUID), %esi
	movl	4(%esp),%ecx			/* Old thread */
	testl	%ecx,%ecx			/* no thread? */
	jz	1f
	/* release bit from old pm_active */
	movl	PCPU(CURPMAP), %ebx
#ifdef SMP
	lock
#endif
	btrl	%esi, PM_ACTIVE(%ebx)		/* clear old */
1:
	movl	8(%esp),%ecx			/* New thread */
	movl	TD_PCB(%ecx),%edx
	movl	PCB_CR3(%edx),%eax
	LOAD_CR3(%eax)
	/* set bit in new pm_active */
	movl	TD_PROC(%ecx),%eax
	movl	P_VMSPACE(%eax), %ebx
	addl	$VM_PMAP, %ebx
	movl	%ebx, PCPU(CURPMAP)
#ifdef SMP
	lock
#endif
	btsl	%esi, PM_ACTIVE(%ebx)		/* set new */
	jmp	sw1
END(cpu_throw)

/*
 * cpu_switch(old, new)
 *
 * Save the current thread state, then select the next thread to run
 * and load its state.
 * 0(%esp) = ret
 * 4(%esp) = oldtd
 * 8(%esp) = newtd
 * 12(%esp) = newlock
 */
ENTRY(cpu_switch)

	/* Switch to new thread.  First, save context. */
	movl	4(%esp),%ecx

#ifdef INVARIANTS
	testl	%ecx,%ecx			/* no thread? */
	jz	badsw2				/* no, panic */
#endif

	movl	TD_PCB(%ecx),%edx

	movl	(%esp),%eax			/* Hardware registers */
	movl	%eax,PCB_EIP(%edx)
	movl	%ebx,PCB_EBX(%edx)
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)
	movl	%esi,PCB_ESI(%edx)
	movl	%edi,PCB_EDI(%edx)
	movl	%gs,PCB_GS(%edx)
	pushfl					/* PSL */
	popl	PCB_PSL(%edx)
	/* Test if debug registers should be saved. */
	testl	$PCB_DBREGS,PCB_FLAGS(%edx)
	jz      1f                              /* no, skip over */
	movl    %dr7,%eax                       /* yes, do the save */
	movl    %eax,PCB_DR7(%edx)
	andl    $0x0000fc00, %eax               /* disable all watchpoints */
	movl    %eax,%dr7
	movl    %dr6,%eax
	movl    %eax,PCB_DR6(%edx)
	movl    %dr3,%eax
	movl    %eax,PCB_DR3(%edx)
	movl    %dr2,%eax
	movl    %eax,PCB_DR2(%edx)
	movl    %dr1,%eax
	movl    %eax,PCB_DR1(%edx)
	movl    %dr0,%eax
	movl    %eax,PCB_DR0(%edx)
1:

#ifdef DEV_NPX
	/* have we used fp, and need a save? */
	cmpl	%ecx,PCPU(FPCURTHREAD)
	jne	1f
	addl	$PCB_SAVEFPU,%edx		/* h/w bugs make saving complicated */
	pushl	%edx
	call	npxsave				/* do it in a big C function */
	popl	%eax
1:
#endif

	/* Save is done.  Now fire up new thread. Leave old vmspace. */
	movl	4(%esp),%edi
	movl	8(%esp),%ecx			/* New thread */
	movl	12(%esp),%esi			/* New lock */
#ifdef INVARIANTS
	testl	%ecx,%ecx			/* no thread? */
	jz	badsw3				/* no, panic */
#endif
	movl	TD_PCB(%ecx),%edx

	/* switch address space */
	movl	PCB_CR3(%edx),%eax
#ifdef PAE
	cmpl	%eax,IdlePDPT			/* Kernel address space? */
#else
	cmpl	%eax,IdlePTD			/* Kernel address space? */
#endif
	je	sw0
	READ_CR3(%ebx)				/* The same address space? */
	cmpl	%ebx,%eax
	je	sw0
	LOAD_CR3(%eax)				/* new address space */
	movl	%esi,%eax
	movl	PCPU(CPUID),%esi
	SETOP	%eax,TD_LOCK(%edi)		/* Switchout td_lock */

	/* Release bit from old pmap->pm_active */
	movl	PCPU(CURPMAP), %ebx
#ifdef SMP
	lock
#endif
	btrl	%esi, PM_ACTIVE(%ebx)		/* clear old */

	/* Set bit in new pmap->pm_active */
	movl	TD_PROC(%ecx),%eax		/* newproc */
	movl	P_VMSPACE(%eax), %ebx
	addl	$VM_PMAP, %ebx
	movl	%ebx, PCPU(CURPMAP)
#ifdef SMP
	lock
#endif
	btsl	%esi, PM_ACTIVE(%ebx)		/* set new */
	jmp	sw1

sw0:
	SETOP	%esi,TD_LOCK(%edi)		/* Switchout td_lock */
sw1:
	BLOCK_SPIN(%ecx)
#ifdef XEN
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	call	xen_handle_thread_switch
	popl	%edx
	popl	%ecx
	popl	%eax
	/*
	 * XXX set IOPL
	 */
#else		
	/*
	 * At this point, we've switched address spaces and are ready
	 * to load up the rest of the next context.
	 */
	cmpl	$0, PCB_EXT(%edx)		/* has pcb extension? */
	je	1f				/* If not, use the default */
	movl	$1, PCPU(PRIVATE_TSS) 		/* mark use of private tss */
	movl	PCB_EXT(%edx), %edi		/* new tss descriptor */
	jmp	2f				/* Load it up */

1:	/*
	 * Use the common default TSS instead of our own.
	 * Set our stack pointer into the TSS, it's set to just
	 * below the PCB.  In C, common_tss.tss_esp0 = &pcb - 16;
	 */
	leal	-16(%edx), %ebx			/* leave space for vm86 */
	movl	%ebx, PCPU(COMMON_TSS) + TSS_ESP0

	/*
	 * Test this CPU's  bit in the bitmap to see if this
	 * CPU was using a private TSS.
	 */
	cmpl	$0, PCPU(PRIVATE_TSS)		/* Already using the common? */
	je	3f				/* if so, skip reloading */
	movl	$0, PCPU(PRIVATE_TSS)
	PCPU_ADDR(COMMON_TSSD, %edi)
2:
	/* Move correct tss descriptor into GDT slot, then reload tr. */
	movl	PCPU(TSS_GDT), %ebx		/* entry in GDT */
	movl	0(%edi), %eax
	movl	4(%edi), %esi
	movl	%eax, 0(%ebx)
	movl	%esi, 4(%ebx)
	movl	$GPROC0_SEL*8, %esi		/* GSEL(GPROC0_SEL, SEL_KPL) */
	ltr	%si
3:

	/* Copy the %fs and %gs selectors into this pcpu gdt */
	leal	PCB_FSD(%edx), %esi
	movl	PCPU(FSGS_GDT), %edi
	movl	0(%esi), %eax		/* %fs selector */
	movl	4(%esi), %ebx
	movl	%eax, 0(%edi)
	movl	%ebx, 4(%edi)
	movl	8(%esi), %eax		/* %gs selector, comes straight after */
	movl	12(%esi), %ebx
	movl	%eax, 8(%edi)
	movl	%ebx, 12(%edi)
#endif
	/* Restore context. */
	movl	PCB_EBX(%edx),%ebx
	movl	PCB_ESP(%edx),%esp
	movl	PCB_EBP(%edx),%ebp
	movl	PCB_ESI(%edx),%esi
	movl	PCB_EDI(%edx),%edi
	movl	PCB_EIP(%edx),%eax
	movl	%eax,(%esp)
	pushl	PCB_PSL(%edx)
	popfl

	movl	%edx, PCPU(CURPCB)
	movl	TD_TID(%ecx),%eax
	movl	%ecx, PCPU(CURTHREAD)		/* into next thread */

	/*
	 * Determine the LDT to use and load it if is the default one and
	 * that is not the current one.
	 */
	movl	TD_PROC(%ecx),%eax
	cmpl    $0,P_MD+MD_LDT(%eax)
	jnz	1f
	movl	_default_ldt,%eax
	cmpl	PCPU(CURRENTLDT),%eax
	je	2f
	LLDT(_default_ldt)
	movl	%eax,PCPU(CURRENTLDT)
	jmp	2f
1:
	/* Load the LDT when it is not the default one. */
	pushl	%edx				/* Preserve pointer to pcb. */
	addl	$P_MD,%eax			/* Pointer to mdproc is arg. */
	pushl	%eax
	call	set_user_ldt
	addl	$4,%esp
	popl	%edx
2:

	/* This must be done after loading the user LDT. */
	.globl	cpu_switch_load_gs
cpu_switch_load_gs:
	movl	PCB_GS(%edx),%gs

	/* Test if debug registers should be restored. */
	testl	$PCB_DBREGS,PCB_FLAGS(%edx)
	jz      1f

	/*
	 * Restore debug registers.  The special code for dr7 is to
	 * preserve the current values of its reserved bits.
	 */
	movl    PCB_DR6(%edx),%eax
	movl    %eax,%dr6
	movl    PCB_DR3(%edx),%eax
	movl    %eax,%dr3
	movl    PCB_DR2(%edx),%eax
	movl    %eax,%dr2
	movl    PCB_DR1(%edx),%eax
	movl    %eax,%dr1
	movl    PCB_DR0(%edx),%eax
	movl    %eax,%dr0
	movl	%dr7,%eax
	andl    $0x0000fc00,%eax
	movl    PCB_DR7(%edx),%ecx
	andl	$~0x0000fc00,%ecx
	orl     %ecx,%eax
	movl    %eax,%dr7
1:
	ret

#ifdef INVARIANTS
badsw1:
	pushal
	pushl	$sw0_1
	call	panic
sw0_1:	.asciz	"cpu_throw: no newthread supplied"

badsw2:
	pushal
	pushl	$sw0_2
	call	panic
sw0_2:	.asciz	"cpu_switch: no curthread supplied"

badsw3:
	pushal
	pushl	$sw0_3
	call	panic
sw0_3:	.asciz	"cpu_switch: no newthread supplied"
#endif
END(cpu_switch)

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* Fetch PCB. */
	movl	4(%esp),%ecx

	/* Save caller's return address.  Child won't execute this routine. */
	movl	(%esp),%eax
	movl	%eax,PCB_EIP(%ecx)

	movl	%cr3,%eax
	movl	%eax,PCB_CR3(%ecx)

	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)
	movl	%gs,PCB_GS(%ecx)
	pushfl
	popl	PCB_PSL(%ecx)

#ifdef DEV_NPX
	/*
	 * If fpcurthread == NULL, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If fpcurthread != NULL, then we have to save the npx h/w state to
	 * fpcurthread's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's npx state for forks by forgetting to reload.
	 */
	pushfl
	CLI
	movl	PCPU(FPCURTHREAD),%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	TD_PCB(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	npxsave
	addl	$4,%esp
	popl	%eax
	popl	%ecx

	pushl	$PCB_SAVEFPU_SIZE
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	bcopy
	addl	$12,%esp
1:
	popfl
#endif	/* DEV_NPX */

	ret
END(savectx)
