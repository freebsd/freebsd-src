/*-
 * Copyright (c) 2011-2012 Spectra Logic Corporation
 * All rights reserved.
 *
 * This software was developed by Cherry G. Mathew <cherry@zyx.in>
 * under sponsorship from Spectra Logic Corporation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/specialreg.h>

#include "assym.s"

#define T_EVENT T_RESERVED /* XXX: */
#define VGCF_IN_SYSCALL 256 /* See: xen/interface/arch-x86/xen-x86_64.h */

/*
 * We're guaranteed that sizeof(struct vcpu_info) == 64 bytes.
 * log2(64) == 6;
 * See: interface/xen.h
 */
#define EVENTS_MASK(reg) \
	movq 	PCPU(CPUID), reg ; \
	shlq	$6, reg	/* cpuid * sizeof(struct vcpu_info) */ ; \
	addq	HYPERVISOR_shared_info, reg /* shared_info.vcpu_info[cpuid] */ ; \
	movq	$1, EVTCHN_UPCALL_MASK(reg)

#define EVENTS_UNMASK(reg) \
	movq 	PCPU(CPUID), reg  ; \
	shlq	$6, reg	/* cpuid * sizeof(struct vcpu_info) */ ; \
	addq	HYPERVISOR_shared_info, reg /* shared_info.vcpu_info[cpuid] */ ; \
	movq	$0, EVTCHN_UPCALL_MASK(reg)

/* Save all general registers to the stack */	
#define SAVE_GENERAL_REGS \
	movq	%rdi, TF_RDI(%rsp) ; \
	movq	%rsi, TF_RSI(%rsp) ; \
	movq	%rdx, TF_RDX(%rsp) ; \
	movq	%rcx, TF_RCX(%rsp) ; \
	movq	%r8, TF_R8(%rsp)   ; \
	movq	%r9, TF_R9(%rsp)   ; \
	movq	%rax, TF_RAX(%rsp) ; \
	movq	%rbx, TF_RBX(%rsp) ; \
	movq	%rbp, TF_RBP(%rsp) ; \
	movq	%r10, TF_R10(%rsp) ; \
	movq	%r11, TF_R11(%rsp) ; \
	movq	%r12, TF_R12(%rsp) ; \
	movq	%r13, TF_R13(%rsp) ; \
	movq	%r14, TF_R14(%rsp) ; \
	movq	%r15, TF_R15(%rsp)

/* Restore all general registers from the stack */
#define RESTORE_GENERAL_REGS \
	movq	TF_RDI(%rsp), %rdi ; \
	movq	TF_RSI(%rsp), %rsi ; \
	movq	TF_RDX(%rsp), %rdx ; \
	movq	TF_RCX(%rsp), %rcx ; \
	movq	TF_R8(%rsp), %r8 ; \
	movq	TF_R9(%rsp), %r9 ; \
	movq	TF_RAX(%rsp), %rax ; \
	movq	TF_RBX(%rsp), %rbx ; \
	movq	TF_RBP(%rsp), %rbp ; \
	movq	TF_R10(%rsp), %r10 ; \
	movq	TF_R11(%rsp), %r11 ; \
	movq	TF_R12(%rsp), %r12 ; \
	movq	TF_R13(%rsp), %r13 ; \
	movq	TF_R14(%rsp), %r14 ; \
	movq	TF_R15(%rsp), %r15

/* Note: %fs/%gs are saved/restored by the hypervisor */
/*
 * Save generic data segment registers to the stack
 * Indicate that segments have been saved.
 */
#define SAVE_SEGMENT_REGS \
	movw	%es, TF_ES(%rsp) ; \
	movw	%ds, TF_DS(%rsp) ; \
	movw	%fs, TF_FS(%rsp) ; \
	movw	%gs, TF_FS(%rsp) ; \
	movl	$TF_HASSEGS,TF_FLAGS(%rsp) ; \

/* Restore generic data segment registers from the stack */
/* This macro overwrites some general registers */
#define RESTORE_SEGMENT_REGS \
	call	restore_segment_regs

/* stackframe management for trap()
 *
 * Xen creates a "bounce frame" in the following format:
 *   { RCX, R11, [DS-GS,] [ERRCODE,] RIP, CS, RFLAGS, RSP, SS }
 *
 * Erratum: Comments in the Xen sources talk about [CR2] saved on the
 *   stackframe, but the code for this is not to be found.
 *	c.f: xen/arch/x86/x86_64/entry.S
 *
 * [DS-GS,] is only saved for the failsafe callback.
 *
 * [ERRCODE], is optional, depending on the type of (hardware) exception.
 *  See: the "AMD64 Architecture Programmer's Manual, Volume 2:
 *  System Programming: Section 8.2 for individual error code
 *  reporting status
 *
 */
/*
 * Prepare the frame for a non-failsafe entry point.
 * We frob the stack so it looks like the native entry point.
 * See: "hardware defined" part of x86/frame.h struct trapframe;
 */
	
#define TRAP_FRAME_PREPARE \
	movq 	(%rsp), %rcx 	; \
	movq 	8(%rsp), %r11	; \
	addq 	$16, %rsp

/*
 * Setup the trapframe for exceptions that the CPU does not
 * push an error code on the stack.
 */

#define TRAP_FRAME_ENTER_NOERR \
	TRAP_FRAME_PREPARE	; \
	subq	$TF_RIP, %rsp 	; \
	movq 	$0, TF_ERR(%rsp)

/*
 * Setup the trapframe for exceptions that the CPU pushes an
 * error code on the stack.
 */
#define TRAP_FRAME_ENTER_ERR \
	TRAP_FRAME_PREPARE	; \
	subq 	$TF_ERR, %rsp

/*
 * Setting up the exit stackframe involves resetting the stack layout
 * identically to that of an exception without error code. The reason
 * for this is that in order to "iret", we make a hypervisor call, and
 * this hypervisor call is a syscall which expects an 'error code' on
 * the stack. We accomplish this by pushing quadword '0' onto the
 * stack in the INTR_EXIT() stub.
 *
 */

#define TRAP_FRAME_EXIT_NOERR \
	addq	$TF_RIP, %rsp

#define TRAP_FRAME_EXIT_ERR \
	addq 	$TF_RIP, %rsp

#define TRAP_PROLOGUE(a) \
	movl	$(a), TF_TRAPNO(%rsp) 	; \
	movq	$0, TF_ADDR(%rsp)

/* Fetch page fault address (%cr2) from hypervisor */
#define SETUP_TF_ADDR 			/* Clobbers %rsi %rdi */  \
	movq 	PCPU(CPUID), %rsi	; \
	shlq	$6, %rsi		/* cpuid * sizeof(struct vcpu_info) */ 	   ; \
	addq	HYPERVISOR_shared_info, %rsi 	/* shared_info.vcpu_info[cpuid] */ ; \
	movq	VCPU_RCR2(%rsi), %rdi	     ; \
	movq	%rdi, TF_ADDR(%rsp)

#define CALLTRAP \
	cld			; \
	movq	%rsp, %rdi	; \
	call	trap

#define EVENT_UPCALL \
	cld			; \
	movq	%rsp, %rdi	; \
	call	xen_intr_handle_upcall

#define DO_AST_MAYBE \
	testb	$SEL_RPL_MASK, TF_CS(%rsp) /* are we returning to user mode? */	; \
	jz	2f		  	  /* can't handle ASTs now if not */	; \
1: \
	/* XXX: cli */		\
	movq	PCPU(CURTHREAD), %rax		; \
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED, TD_FLAGS(%rax) ; \
	je	2f		; \
	/* XXX: sti */		\
	movq	%rsp, %rdi 	; \
	call ast		; \
	jmp	1b		; \
2:
	
#define DO_STI_MAYBE \
	testl	$PSL_I, TF_RFLAGS(%rsp)	; \
	jz	1f			; \
	EVENTS_UNMASK(%rdi)		; \
1:
	
#define INTR_EXIT \
	pushq	$0		; /* struct iret_context.flags */ \
	jmp hypercall_page + (__HYPERVISOR_iret * 32)

#define CALLSYSCALL \
	cld			     ;\
	movq	PCPU(CURTHREAD),%rdi ;\
	movq	%rsp, TD_FRAME(%rdi) ;\
	movl	TF_RFLAGS(%rsp),%esi ;\
	andl	$PSL_T,%esi	     ;\
	call	amd64_syscall

#define SYSRET \
	/* XXX: watchout for:
	 * http:	//cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2006-0744
	 * Explained here:
	 * http://blog.xen.org/index.php/2012/06/13/the-intel-sysret-privilege-escalation/
	 * Also see comments in trap.c
	 */
	pushq	$VGCF_IN_SYSCALL ;\
	jmp hypercall_page + (__HYPERVISOR_iret * 32)
	
NON_GPROF_ENTRY(restore_segment_regs)
	.globl	doreti_iret
	.globl  ld_es
	.globl	ld_ds
	.globl	ld_fs
	.globl	ld_gs
doreti_iret:
	/* Note: The trapframe is on the *caller* stackframe */
	movq	PCPU(CURPCB),%r8

	/*
	 * Do not reload segment registers for kernel.
	 * Since we do not reload segments registers with sane
	 * values on kernel entry, descriptors referenced by
	 * segments registers might be not valid.  This is fatal
	 * for user mode, but is not a problem for the kernel.
	 */
	testb	$SEL_RPL_MASK,(TF_CS + 8)(%rsp)
	jz	segs_done
	testl	$PCB_FULL_IRET,PCB_FLAGS(%r8)
	jz	segs_done
	testl	$TF_HASSEGS,(TF_FLAGS + 8)(%rsp)
	jne	1f

	/* reload with sane values */
	movw	$KUDSEL,%ax
	movw	%ax,(TF_DS + 8)(%rsp)
	movw	%ax,(TF_ES + 8)(%rsp)
	movw	$KUF32SEL,(TF_FS + 8)(%rsp)
	movw	$KUG32SEL,(TF_GS + 8)(%rsp)
	
1:
ld_fs:
	xorq	%rbx, %rbx
	movw	(TF_FS + 8)(%rsp), %ax
	movw	%ax, %fs	/* blew away fsbase here */
	cmpw	$KUF32SEL, %ax
	jne	2f
	movq	$0x666, %rbx	/* Kernel doesn't use %fs */

2:	
ld_gs:
	movl	PCPU(CPUID), %edi
	callq	pcpu_find
	movq	%rax, %rdi	/* kgsbase == pcpu_find(PCPU_GET(cpuid)) */
	
#if 0 /* XXX: Investigate tls restore to userland further */
	movw	(TF_GS + 8)(%rsp), %ax
	movw	%ax, %gs	/* blew away (k)gsbase here */

	/* Kernel gsbase reload */
	callq	xen_load_kgsbase /* reload kgsbase from %rdi */

	movw	%gs, %ax
	cmpw	$KUG32SEL, %ax
	jne	3f
	movq	$0x666, %rbx
3:	
	cmpq	$0x666, %rbx	/* reload user %fs/%gs ? */
	jne	4f
#endif
	movq	PCPU(CURPCB), %rdi
	callq	xen_load_tls 	/* Update user %fs/%gs to pcb_fsbase and pcb_gsbase */
	
4:	/* done with %fs/%gs */

ld_es:
	movw	(TF_ES + 8)(%rsp), %es ;
ld_ds:
	movw	(TF_DS + 8)(%rsp), %ds ;

segs_done:	
	ret
	
	/* The path below should not have been reached. */
	.globl	doreti_iret_fault
	.globl	ld_gsbase
	.globl	ld_fsbase
	.globl	gsbase_load_fault
	.globl	fsbase_load_fault
	.globl	mca_intr
	.globl	fs_load_fault /* XXX: revisit */
	.globl	gs_load_fault /* XXX: revisit */
doreti_iret_fault:
ld_gsbase:
ld_fsbase:
gsbase_load_fault:
fsbase_load_fault:
fs_load_fault:
gs_load_fault:
mca_intr:
	movq	msgflt, %rdi
	call	panic		/* panic("..."); */

msgflt:	.asciz	"Unknown kernel fault rip location\n"

	.globl	ds_load_fault
	.globl	es_load_fault
ds_load_fault:
es_load_fault:
	movl	$T_PROTFLT,TF_TRAPNO(%rsp)
	movq	%rsp,%rdi
	call	trap
	movw	$KUDSEL,TF_ES(%rsp)
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;
	
IDTVEC(de)			/* Divide-By-Zero-Error */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_DIVIDE)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;
	
IDTVEC(db)			/* Debug */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_TRCTRAP);
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(nmi)			/* Non-Maskable-Interrupt */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_NMI)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(bp)			/* Breakpoint */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_BPTFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(of)			/* Overflow */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_OFLOW)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(br)			/* Bound-Range */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_BOUND)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(ud)			/* Invalid-Opcode */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_PRIVINFLT) ;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(nm)			/* Device-Not-Available */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_DNA)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(df)			/* Double-Fault */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_DOUBLEFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(ts)			/* Invalid-TSS */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_TSSFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	;
	RESTORE_GENERAL_REGS	; /* overwrites some general registers */
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(np)			/* Segment-Not-Present */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_SEGNPFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	;
	RESTORE_GENERAL_REGS	; /* overwrites some general registers */
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(ss)			/* Stack */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_STKFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(gp)			/* General-Protection */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_PROTFLT);
	SAVE_SEGMENT_REGS	;
	SAVE_GENERAL_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_GENERAL_REGS	;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(pf)			/* Page-Fault */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_PAGEFLT);
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	SETUP_TF_ADDR		; /* Fault Address - clobbers %rsi %rdi */
	CALLTRAP		; /* %rsi is ignored */
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(mf)			/* x87 Floating-Point Exception Pending */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_ARITHTRAP)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(ac)			/* Alignment-Check */
	TRAP_FRAME_ENTER_ERR	;
	TRAP_PROLOGUE(T_ALIGNFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_ERR   	;
	INTR_EXIT		;

IDTVEC(mc)			/* Machine-Check */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_MCHK)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(xf)			/* SIMD Floating-Point */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_XMMFLT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(rs)			/* Reserved */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_RESERVED) ;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	DO_STI_MAYBE		;
	CALLTRAP		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

ENTRY(fork_trampoline)
	movq	%r12,%rdi		/* function */
	movq	%rbx,%rsi		/* arg1 */
	movq	%rsp,%rdx		/* trapframe pointer */
	call	fork_exit
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

IDTVEC(hypervisor_callback)	/* Xen only */
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_EVENT)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	EVENT_UPCALL		;
	DO_STI_MAYBE		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	;
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		;

ENTRY(failsafe_callback)
	movq	msgfailsafe, %rdi ;
	call	panic		; /* panic("..."); */
msgfailsafe:	.asciz	"Failsafe upcall triggered\n"

IDTVEC(syscall_callback)
	TRAP_FRAME_ENTER_NOERR	;
	TRAP_PROLOGUE(T_USER)	;
	SAVE_GENERAL_REGS	;
	SAVE_SEGMENT_REGS	;
	movq	%r11, TF_RFLAGS(%rsp) ; /* Tweak for INTR_EXIT */
	movq	%r10, TF_RCX(%rsp) ; /* Translate to C abi. see trap.c:cpu_fetch_syscall_args() */
	DO_STI_MAYBE		; /* Clobbers %rdi */
	movq	TF_RDI(%rsp), %rdi ;
	CALLSYSCALL		;
	DO_AST_MAYBE		;
	RESTORE_SEGMENT_REGS	; /* overwrites some general registers */
	RESTORE_GENERAL_REGS	; /* XXX: optimise for SYSRET */
	TRAP_FRAME_EXIT_NOERR   ;
	INTR_EXIT		; /* XXX: SYSRET is more optimal */
