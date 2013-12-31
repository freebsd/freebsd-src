/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <machine/asmacros.h>

#include "vmx_assym.s"

#ifdef SMP
#define	LK	lock ;
#else
#define	LK
#endif

/*
 * Disable interrupts before updating %rsp in VMX_CHECK_AST or
 * VMX_GUEST_RESTORE.
 *
 * The location that %rsp points to is a 'vmxctx' and not a
 * real stack so we don't want an interrupt handler to trash it
 */
#define	VMX_DISABLE_INTERRUPTS		cli

/*
 * If the thread hosting the vcpu has an ast pending then take care of it
 * by returning from vmx_setjmp() with a return value of VMX_RETURN_AST.
 *
 * Assumes that %rdi holds a pointer to the 'vmxctx' and that interrupts
 * are disabled.
 */
#define	VMX_CHECK_AST							\
	movq	PCPU(CURTHREAD),%rax;					\
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%rax);	\
	je	9f;							\
	movq	$VMX_RETURN_AST,%rsi;					\
	movq	%rdi,%rsp;						\
	addq	$VMXCTX_TMPSTKTOP,%rsp;					\
	callq	vmx_return;						\
9:

/*
 * Assumes that %rdi holds a pointer to the 'vmxctx'.
 *
 * On "return" all registers are updated to reflect guest state. The two
 * exceptions are %rip and %rsp. These registers are atomically switched
 * by hardware from the guest area of the vmcs.
 *
 * We modify %rsp to point to the 'vmxctx' so we can use it to restore
 * host context in case of an error with 'vmlaunch' or 'vmresume'.
 */
#define	VMX_GUEST_RESTORE						\
	movq	%rdi,%rsp;						\
	movq	VMXCTX_GUEST_CR2(%rdi),%rsi;				\
	movq	%rsi,%cr2;						\
	movq	VMXCTX_GUEST_RSI(%rdi),%rsi;				\
	movq	VMXCTX_GUEST_RDX(%rdi),%rdx;				\
	movq	VMXCTX_GUEST_RCX(%rdi),%rcx;				\
	movq	VMXCTX_GUEST_R8(%rdi),%r8;				\
	movq	VMXCTX_GUEST_R9(%rdi),%r9;				\
	movq	VMXCTX_GUEST_RAX(%rdi),%rax;				\
	movq	VMXCTX_GUEST_RBX(%rdi),%rbx;				\
	movq	VMXCTX_GUEST_RBP(%rdi),%rbp;				\
	movq	VMXCTX_GUEST_R10(%rdi),%r10;				\
	movq	VMXCTX_GUEST_R11(%rdi),%r11;				\
	movq	VMXCTX_GUEST_R12(%rdi),%r12;				\
	movq	VMXCTX_GUEST_R13(%rdi),%r13;				\
	movq	VMXCTX_GUEST_R14(%rdi),%r14;				\
	movq	VMXCTX_GUEST_R15(%rdi),%r15;				\
	movq	VMXCTX_GUEST_RDI(%rdi),%rdi; /* restore rdi the last */

/*
 * Check for an error after executing a VMX instruction.
 * 'errreg' will be zero on success and non-zero otherwise.
 * 'ctxreg' points to the 'struct vmxctx' associated with the vcpu.
 */
#define	VM_INSTRUCTION_ERROR(errreg, ctxreg)				\
	jnc 	1f;							\
	movl 	$VM_FAIL_INVALID,errreg;		/* CF is set */	\
	jmp 	3f;							\
1:	jnz 	2f;							\
	movl 	$VM_FAIL_VALID,errreg;		/* ZF is set */		\
	jmp 	3f;							\
2:	movl 	$VM_SUCCESS,errreg;					\
3:	movl	errreg,VMXCTX_LAUNCH_ERROR(ctxreg)

/*
 * set or clear the appropriate bit in 'pm_active'
 * %rdi = vmxctx
 * %rax, %r11 = scratch registers
 */
#define	VMX_SET_PM_ACTIVE						\
	movq	VMXCTX_PMAP(%rdi), %r11;				\
	movl	PCPU(CPUID), %eax;					\
	LK btsl	%eax, PM_ACTIVE(%r11)

#define	VMX_CLEAR_PM_ACTIVE						\
	movq	VMXCTX_PMAP(%rdi), %r11;				\
	movl	PCPU(CPUID), %eax;					\
	LK btrl	%eax, PM_ACTIVE(%r11)

/*
 * If 'vmxctx->eptgen[curcpu]' is not identical to 'pmap->pm_eptgen'
 * then we must invalidate all mappings associated with this eptp.
 *
 * %rdi = vmxctx
 * %rax, %rbx, %r11 = scratch registers
 */
#define	VMX_CHECK_EPTGEN						\
	movl	PCPU(CPUID), %ebx;					\
	movq	VMXCTX_PMAP(%rdi), %r11;				\
	movq	PM_EPTGEN(%r11), %rax;					\
	cmpq	%rax, VMXCTX_EPTGEN(%rdi, %rbx, 8);			\
	je	9f;							\
									\
	/* Refresh 'vmxctx->eptgen[curcpu]' */				\
	movq	%rax, VMXCTX_EPTGEN(%rdi, %rbx, 8);			\
									\
	/* Setup the invept descriptor at the top of tmpstk */		\
	mov	%rdi, %r11;						\
	addq	$VMXCTX_TMPSTKTOP, %r11;				\
	movq	VMXCTX_EPTP(%rdi), %rax;				\
	movq	%rax, -16(%r11);					\
	movq	$0x0, -8(%r11);						\
	mov	$0x1, %eax;	/* Single context invalidate */		\
	invept	-16(%r11), %rax;					\
									\
	/* Check for invept error */					\
	VM_INSTRUCTION_ERROR(%eax, %rdi);				\
	testl	%eax, %eax;						\
	jz	9f;							\
									\
	/* Return via vmx_setjmp with retval of VMX_RETURN_INVEPT */	\
	movq	$VMX_RETURN_INVEPT, %rsi;				\
	movq	%rdi,%rsp;						\
	addq	$VMXCTX_TMPSTKTOP, %rsp;				\
	callq	vmx_return;						\
9:	;

	.text
/*
 * int vmx_setjmp(ctxp)
 * %rdi = ctxp
 *
 * Return value is '0' when it returns directly from here.
 * Return value is '1' when it returns after a vm exit through vmx_longjmp.
 */
ENTRY(vmx_setjmp)
	movq	(%rsp),%rax			/* return address */
	movq    %r15,VMXCTX_HOST_R15(%rdi)
	movq    %r14,VMXCTX_HOST_R14(%rdi)
	movq    %r13,VMXCTX_HOST_R13(%rdi)
	movq    %r12,VMXCTX_HOST_R12(%rdi)
	movq    %rbp,VMXCTX_HOST_RBP(%rdi)
	movq    %rsp,VMXCTX_HOST_RSP(%rdi)
	movq    %rbx,VMXCTX_HOST_RBX(%rdi)
	movq    %rax,VMXCTX_HOST_RIP(%rdi)

	/*
	 * XXX save host debug registers
	 */
	movl	$VMX_RETURN_DIRECT,%eax
	ret
END(vmx_setjmp)

/*
 * void vmx_return(struct vmxctx *ctxp, int retval)
 * %rdi = ctxp
 * %rsi = retval
 * Return to vmm context through vmx_setjmp() with a value of 'retval'.
 */
ENTRY(vmx_return)
	/* The pmap is no longer active on the host cpu */
	VMX_CLEAR_PM_ACTIVE

	/* Restore host context. */
	movq	VMXCTX_HOST_R15(%rdi),%r15
	movq	VMXCTX_HOST_R14(%rdi),%r14
	movq	VMXCTX_HOST_R13(%rdi),%r13
	movq	VMXCTX_HOST_R12(%rdi),%r12
	movq	VMXCTX_HOST_RBP(%rdi),%rbp
	movq	VMXCTX_HOST_RSP(%rdi),%rsp
	movq	VMXCTX_HOST_RBX(%rdi),%rbx
	movq	VMXCTX_HOST_RIP(%rdi),%rax
	movq	%rax,(%rsp)			/* return address */

	/*
	 * XXX restore host debug registers
	 */
	movl	%esi,%eax
	ret
END(vmx_return)

/*
 * void vmx_longjmp(void)
 * %rsp points to the struct vmxctx
 */
ENTRY(vmx_longjmp)
	/*
	 * Save guest state that is not automatically saved in the vmcs.
	 */
	movq	%rdi,VMXCTX_GUEST_RDI(%rsp)
	movq	%rsi,VMXCTX_GUEST_RSI(%rsp)
	movq	%rdx,VMXCTX_GUEST_RDX(%rsp)
	movq	%rcx,VMXCTX_GUEST_RCX(%rsp)
	movq	%r8,VMXCTX_GUEST_R8(%rsp)
	movq	%r9,VMXCTX_GUEST_R9(%rsp)
	movq	%rax,VMXCTX_GUEST_RAX(%rsp)
	movq	%rbx,VMXCTX_GUEST_RBX(%rsp)
	movq	%rbp,VMXCTX_GUEST_RBP(%rsp)
	movq	%r10,VMXCTX_GUEST_R10(%rsp)
	movq	%r11,VMXCTX_GUEST_R11(%rsp)
	movq	%r12,VMXCTX_GUEST_R12(%rsp)
	movq	%r13,VMXCTX_GUEST_R13(%rsp)
	movq	%r14,VMXCTX_GUEST_R14(%rsp)
	movq	%r15,VMXCTX_GUEST_R15(%rsp)

	movq	%cr2,%rdi
	movq	%rdi,VMXCTX_GUEST_CR2(%rsp)

	movq	%rsp,%rdi
	movq	$VMX_RETURN_LONGJMP,%rsi

	addq	$VMXCTX_TMPSTKTOP,%rsp
	callq	vmx_return
END(vmx_longjmp)

/*
 * void vmx_resume(struct vmxctx *ctxp)
 * %rdi = ctxp
 *
 * Although the return type is a 'void' this function may return indirectly
 * through vmx_setjmp() with a return value of 2.
 */
ENTRY(vmx_resume)
	VMX_DISABLE_INTERRUPTS

	VMX_CHECK_AST

	VMX_SET_PM_ACTIVE	/* This vcpu is now active on the host cpu */

	VMX_CHECK_EPTGEN	/* Check if we have to invalidate TLB */

	/*
	 * Restore guest state that is not automatically loaded from the vmcs.
	 */
	VMX_GUEST_RESTORE

	vmresume

	/*
	 * Capture the reason why vmresume failed.
	 */
	VM_INSTRUCTION_ERROR(%eax, %rsp)

	/* Return via vmx_setjmp with return value of VMX_RETURN_VMRESUME */
	movq	%rsp,%rdi
	movq	$VMX_RETURN_VMRESUME,%rsi

	addq	$VMXCTX_TMPSTKTOP,%rsp
	callq	vmx_return
END(vmx_resume)

/*
 * void vmx_launch(struct vmxctx *ctxp)
 * %rdi = ctxp
 *
 * Although the return type is a 'void' this function may return indirectly
 * through vmx_setjmp() with a return value of 3.
 */
ENTRY(vmx_launch)
	VMX_DISABLE_INTERRUPTS

	VMX_CHECK_AST

	VMX_SET_PM_ACTIVE	/* This vcpu is now active on the host cpu */

	VMX_CHECK_EPTGEN	/* Check if we have to invalidate TLB */

	/*
	 * Restore guest state that is not automatically loaded from the vmcs.
	 */
	VMX_GUEST_RESTORE

	vmlaunch

	/*
	 * Capture the reason why vmlaunch failed.
	 */
	VM_INSTRUCTION_ERROR(%eax, %rsp)

	/* Return via vmx_setjmp with return value of VMX_RETURN_VMLAUNCH */
	movq	%rsp,%rdi
	movq	$VMX_RETURN_VMLAUNCH,%rsi

	addq	$VMXCTX_TMPSTKTOP,%rsp
	callq	vmx_return
END(vmx_launch)
