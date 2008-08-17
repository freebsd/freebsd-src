/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
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

#include "opt_apic.h"
#include "opt_npx.h"

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/trap.h>


#include "assym.s"

#define	SEL_RPL_MASK	0x0002
#define __HYPERVISOR_iret	23
	
/* Offsets into shared_info_t. */
#define evtchn_upcall_pending /* 0 */
#define evtchn_upcall_mask       1
#define XEN_BLOCK_EVENTS(reg)     movb $1,evtchn_upcall_mask(reg)
#define XEN_UNBLOCK_EVENTS(reg)   movb $0,evtchn_upcall_mask(reg)
#define XEN_TEST_PENDING(reg)     testb $0x1,evtchn_upcall_pending(reg)

#define POPA \
        popl %edi; \
        popl %esi; \
        popl %ebp; \
        popl %ebx; \
        popl %ebx; \
        popl %edx; \
        popl %ecx; \
        popl %eax;

	.text

/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines.
 *
 * Most traps are 'trap gates', SDT_SYS386TGT.  A trap gate pushes state on
 * the stack that mostly looks like an interrupt, but does not disable 
 * interrupts.  A few of the traps we are use are interrupt gates, 
 * SDT_SYS386IGT, which are nearly the same thing except interrupts are
 * disabled on entry.
 *
 * The cpu will push a certain amount of state onto the kernel stack for
 * the current process.  The amount of state depends on the type of trap 
 * and whether the trap crossed rings or not.  See i386/include/frame.h.  
 * At the very least the current EFLAGS (status register, which includes 
 * the interrupt disable state prior to the trap), the code segment register,
 * and the return instruction pointer are pushed by the cpu.  The cpu 
 * will also push an 'error' code for certain traps.  We push a dummy 
 * error code for those traps where the cpu doesn't in order to maintain 
 * a consistent frame.  We also push a contrived 'trap number'.
 *
 * The cpu does not push the general registers, we must do that, and we 
 * must restore them prior to calling 'iret'.  The cpu adjusts the %cs and
 * %ss segment registers, but does not mess with %ds, %es, or %fs.  Thus we
 * must load them with appropriate values for supervisor mode operation.
 */

MCOUNT_LABEL(user)
MCOUNT_LABEL(btrap)

#define	TRAP(a)		pushl $(a) ; jmp alltraps

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
	pushl $0; TRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
	pushl $0; TRAP(T_BPTFLT)
IDTVEC(ofl)
	pushl $0; TRAP(T_OFLOW)
IDTVEC(bnd)
	pushl $0; TRAP(T_BOUND)
IDTVEC(ill)
	pushl $0; TRAP(T_PRIVINFLT)
IDTVEC(dna)
	pushl $0; TRAP(T_DNA)
IDTVEC(fpusegm)
	pushl $0; TRAP(T_FPOPFLT)
IDTVEC(tss)
	TRAP(T_TSSFLT)
IDTVEC(missing)
	TRAP(T_SEGNPFLT)
IDTVEC(stk)
	TRAP(T_STKFLT)
IDTVEC(prot)
	TRAP(T_PROTFLT)
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(mchk)
	pushl $0; TRAP(T_MCHK)
IDTVEC(rsvd)
	pushl $0; TRAP(T_RESERVED)
IDTVEC(fpu)
	pushl $0; TRAP(T_ARITHTRAP)
IDTVEC(align)
	TRAP(T_ALIGNFLT)
IDTVEC(xmm)
	pushl $0; TRAP(T_XMMFLT)

IDTVEC(hypervisor_callback)
	pushl $0; 
	pushl $0; 
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
upcall_with_regs_pushed:
	SET_KERNEL_SREGS
	FAKE_MCOUNT(TF_EIP(%esp))
call_evtchn_upcall:
	movl	TF_EIP(%esp),%eax
	cmpl	$scrit,%eax
	jb	10f
	cmpl	$ecrit,%eax
	jb	critical_region_fixup
	
10:	pushl	%esp
	call	evtchn_do_upcall
	addl	$4,%esp

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti

	
hypervisor_callback_pending:
	movl	HYPERVISOR_shared_info,%esi
	XEN_BLOCK_EVENTS(%esi)				/*	cli */	
	jmp	10b

	/*
	 * alltraps entry point.  Interrupts are enabled if this was a trap
	 * gate (TGT), else disabled if this was an interrupt gate (IGT).
	 * Note that int0x80_syscall is a trap gate.  Only page faults
	 * use an interrupt gate.
	 */

	SUPERALIGN_TEXT
	.globl	alltraps
	.type	alltraps,@function
alltraps:
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs

alltraps_with_regs_pushed:
	SET_KERNEL_SREGS
	FAKE_MCOUNT(TF_EIP(%esp))

calltrap:
	push	%esp
	call	trap
	add	$4, %esp

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti

/*
 * SYSCALL CALL GATE (old entry point for a.out binaries)
 *
 * The intersegment call has been set up to specify one dummy parameter.
 *
 * This leaves a place to put eflags so that the call frame can be
 * converted to a trap frame. Note that the eflags is (semi-)bogusly
 * pushed into (what will be) tf_err and then copied later into the
 * final spot. It has to be done this way because esp can't be just
 * temporarily altered for the pushfl - an interrupt might come in
 * and clobber the saved cs/eip.
 */
	SUPERALIGN_TEXT
IDTVEC(lcall_syscall)
	pushfl				/* save eflags */
	popl	8(%esp)			/* shuffle into tf_eflags */
	pushl	$7			/* sizeof "lcall 7,0" */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	SET_KERNEL_SREGS
	FAKE_MCOUNT(TF_EIP(%esp))
	pushl	%esp
	call	syscall
	add	$4, %esp
	MEXITCOUNT
	jmp	doreti

/*
 * Call gate entry for FreeBSD ELF and Linux/NetBSD syscall (int 0x80)
 *
 * Even though the name says 'int0x80', this is actually a TGT (trap gate)
 * rather then an IGT (interrupt gate).  Thus interrupts are enabled on
 * entry just as they are for a normal syscall.
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	pushl	$2			/* sizeof "int 0x80" */
	pushl	$0xBEEF			/* for debug */
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	SET_KERNEL_SREGS
	FAKE_MCOUNT(TF_EIP(%esp))
	pushl	%esp
	call	syscall
	add	$4, %esp
	MEXITCOUNT
	jmp	doreti

ENTRY(fork_trampoline)
	pushl	%esp			/* trapframe pointer */
	pushl	%ebx			/* arg1 */
	pushl	%esi			/* function */
	call	fork_exit
	addl	$12,%esp
	/* cut from syscall */

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti


/*
 * To efficiently implement classification of trap and interrupt handlers
 * for profiling, there must be only trap handlers between the labels btrap
 * and bintr, and only interrupt handlers between the labels bintr and
 * eintr.  This is implemented (partly) by including files that contain
 * some of the handlers.  Before including the files, set up a normal asm
 * environment so that the included files doen't need to know that they are
 * included.
 */

	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
MCOUNT_LABEL(bintr)

#ifdef DEV_ATPIC	
#include <i386/isa/atpic_vector.s>
#endif
	
#ifdef DEV_APIC
	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT

#include <i386/i386/apic_vector.s>
#endif

	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
#include <i386/i386/vm86bios.s>

	.text
MCOUNT_LABEL(eintr)

/*
 * void doreti(struct trapframe)
 *
 * Handle return from interrupts, traps and syscalls.
 */
	.text
	SUPERALIGN_TEXT
	.type	doreti,@function
doreti:
	FAKE_MCOUNT($bintr)		/* init "from" bintr -> doreti */
doreti_next:
#ifdef notyet
	/*
	 * Check if ASTs can be handled now.  PSL_VM must be checked first
	 * since segment registers only have an RPL in non-VM86 mode.
	 */
	testl	$PSL_VM,TF_EFLAGS(%esp)	/* are we in vm86 mode? */
	jz	doreti_notvm86
	movl	PCPU(CURPCB),%ecx
	testl	$PCB_VM86CALL,PCB_FLAGS(%ecx)	/* are we in a vm86 call? */
	jz	doreti_ast		/* can handle ASTS now if not */
  	jmp	doreti_exit

doreti_notvm86:
#endif
	testb	$SEL_RPL_MASK,TF_CS(%esp) /* are we returning to user mode? */
	jz	doreti_exit		/* can't handle ASTs now if not */

doreti_ast:
	/*
	 * Check for ASTs atomically with returning.  Disabling CPU
	 * interrupts provides sufficient locking even in the SMP case,
	 * since we will be informed of any new ASTs by an IPI.
	 */
	movl	HYPERVISOR_shared_info,%esi
	XEN_BLOCK_EVENTS(%esi)				/*	cli */
	movl	PCPU(CURTHREAD),%eax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%eax)
	je	doreti_exit
	XEN_UNBLOCK_EVENTS(%esi)	/* sti */
	pushl	%esp			/* pass a pointer to the trapframe */
	call	ast
	add	$4,%esp
	jmp	doreti_ast

	/*
	 * doreti_exit:	pop registers, iret.
	 *
	 *	The segment register pop is a special case, since it may
	 *	fault if (for example) a sigreturn specifies bad segment
	 *	registers.  The fault is handled in trap.c.
	 */
doreti_exit:
	movl	HYPERVISOR_shared_info,%esi
	XEN_UNBLOCK_EVENTS(%esi) # reenable event callbacks (sti)

	.globl	scrit
scrit:
	XEN_TEST_PENDING(%esi)
        jnz	hypervisor_callback_pending	/* More to go  */

	MEXITCOUNT

	.globl	doreti_popl_fs
doreti_popl_fs:
	popl	%fs
	.globl	doreti_popl_es
doreti_popl_es:
	popl	%es
	.globl	doreti_popl_ds
doreti_popl_ds:
	popl	%ds

	/*
	 * This is important: as nothing is atomic over here (we can get
	 * interrupted any time), we use the critical_region_fixup() in
	 * order to figure out where out stack is. Therefore, do NOT use
	 * 'popal' here without fixing up the table!
	 */
	POPA
	addl	$8,%esp
	.globl	doreti_iret
doreti_iret:
	jmp	hypercall_page + (__HYPERVISOR_iret * 32)
	.globl	ecrit
ecrit:
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

	/*
# [How we do the fixup]. We want to merge the current stack frame with the
# just-interrupted frame. How we do this depends on where in the critical
# region the interrupted handler was executing, and so how many saved
# registers are in each frame. We do this quickly using the lookup table
# 'critical_fixup_table'. For each byte offset in the critical region, it
# provides the number of bytes which have already been popped from the
# interrupted stack frame.
*/

.globl critical_region_fixup
critical_region_fixup:
	addl $critical_fixup_table-scrit,%eax
	movzbl (%eax),%eax    # %eax contains num bytes popped
        movl  %esp,%esi
        add  %eax,%esi        # %esi points at end of src region
        movl  %esp,%edi
        add  $0x40,%edi       # %edi points at end of dst region
        movl  %eax,%ecx
        shr  $2,%ecx          # convert bytes to words
        je   16f              # skip loop if nothing to copy
15:     subl $4,%esi          # pre-decrementing copy loop
        subl $4,%edi
        movl (%esi),%eax
        movl %eax,(%edi)
        loop 15b
16:     movl %edi,%esp        # final %edi is top of merged stack
	jmp  hypervisor_callback_pending


critical_fixup_table:        
.byte   0x0,0x0,0x0			#testb  $0x1,(%esi)
.byte   0x0,0x0,0x0,0x0,0x0,0x0		#jne    ea 
.byte   0x0,0x0				#pop    %fs
.byte   0x04				#pop    %es
.byte   0x08				#pop    %ds
.byte   0x0c				#pop    %edi
.byte   0x10	                        #pop    %esi
.byte   0x14	                        #pop    %ebp
.byte   0x18	                        #pop    %ebx
.byte   0x1c	                        #pop    %ebx
.byte   0x20	                        #pop    %edx
.byte   0x24	                        #pop    %ecx
.byte   0x28	                        #pop    %eax
.byte   0x2c,0x2c,0x2c                  #add    $0x8,%esp
#if 0
	.byte   0x34	                        #iret   
#endif
.byte   0x34,0x34,0x34,0x34,0x34        #HYPERVISOR_iret 
	
	
/* # Hypervisor uses this for application faults while it executes.*/
ENTRY(failsafe_callback)
	pushal
	call xen_failsafe_handler
/*#	call install_safe_pf_handler */
        movl 28(%esp),%ebx
1:      movl %ebx,%ds
        movl 32(%esp),%ebx
2:      movl %ebx,%es
        movl 36(%esp),%ebx
3:      movl %ebx,%fs
        movl 40(%esp),%ebx
4:      movl %ebx,%gs
/*#        call install_normal_pf_handler */
	popal
	addl $12,%esp
	iret


