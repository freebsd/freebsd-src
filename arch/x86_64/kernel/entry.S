/*
 *  linux/arch/x86_64/entry.S
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002  Andi Kleen SuSE Labs
 *  Copyright (C) 2000  Pavel Machek <pavel@suse.cz>
 * 
 *  $Id: entry.S,v 1.99 2003/10/24 17:48:32 ak Exp $		
 */

/*
 * entry.S contains the system-call and fault low-level handling routines.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after an interrupt and after each system call.
 * 
 * Normal syscalls and interrupts don't save a full stack frame, this is 
 * only done for PT_TRACESYS, signals or fork/exec et.al.
 * 
 * TODO:	 
 * - schedule it carefully for the final hardware.		 	
 *
 */

#define ASSEMBLY 1
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/current.h>	
#include <asm/smp.h>
#include <asm/cache.h>
#include <asm/errno.h>
#include <asm/calling.h>
#include <asm/offset.h>
#include <asm/msr.h>
#include <asm/unistd.h>
#include <asm/hw_irq.h>

	.code64

#define PDAREF(field) %gs:field	 		

/*
 * C code is not supposed to know about partial frames. Everytime a C function
 * that looks at the pt_regs is called these two macros are executed around it.
 * RESTORE_TOP_OF_STACK syncs the syscall state after any possible ptregs
 * manipulation.
 */        	
		
	/* %rsp:at FRAMEEND */ 
	.macro FIXUP_TOP_OF_STACK tmp
	movq	PDAREF(pda_oldrsp),\tmp
	movq  	\tmp,RSP(%rsp)
	movq    $__USER_DS,SS(%rsp)
	movq    $__USER_CS,CS(%rsp)
	movq	$-1,RCX(%rsp)	/* contains return address, already in RIP */
	movq	R11(%rsp),\tmp  /* get eflags */
	movq	\tmp,EFLAGS(%rsp)
	.endm

	.macro RESTORE_TOP_OF_STACK tmp,offset=0
	movq   RSP-\offset(%rsp),\tmp
	movq   \tmp,PDAREF(pda_oldrsp)
	movq   EFLAGS-\offset(%rsp),\tmp
	movq   \tmp,R11-\offset(%rsp)
	.endm


/*
 * A newly forked process directly context switches into this.
 */ 	
ENTRY(ret_from_fork)
	movq %rax,%rdi		/* return value of __switch_to -> prev task */
	call schedule_tail
	GET_CURRENT(%rcx)
	testb $PT_TRACESYS,tsk_ptrace(%rcx)
	jnz 2f
1:
	RESTORE_REST
	testl $3,CS-ARGOFFSET(%rsp) # from kernel_thread?
	jz   int_ret_from_sys_call
	testl $ASM_THREAD_IA32,tsk_thread+thread_flags(%rcx)
	jnz  int_ret_from_sys_call
	RESTORE_TOP_OF_STACK %rdi,ARGOFFSET
	jmp ret_from_sys_call
2:
	movq %rsp,%rdi	
	call syscall_trace
	GET_CURRENT(%rcx)
	jmp 1b

/*
 * System call entry. Upto 6 arguments in registers are supported.
 *
 * SYSCALL does not save anything on the stack and does not change the
 * stack pointer. Gets the per CPU area from the hidden GS MSR and finds the
 * current kernel stack.
 */
		
/*
 * Register setup:	
 * rax  system call number
 * rdi  arg0
 * rcx  return address for syscall/sysret, C arg3 
 * rsi  arg1
 * rdx  arg2	
 * r10  arg3 	(--> moved to rcx for C)
 * r8   arg4
 * r9   arg5
 * r11  eflags for syscall/sysret, temporary for C
 * r12-r15,rbp,rbx saved by C code, not touched. 		
 * 
 * Interrupts are off on entry.
 * Only called from user space.	
 */ 			 		

ENTRY(system_call)
	swapgs
	movq	%rsp,PDAREF(pda_oldrsp) 
	movq	PDAREF(pda_kernelstack),%rsp
	sti
	SAVE_ARGS 8,1
	movq  %rax,ORIG_RAX-ARGOFFSET(%rsp) 
	movq  %rcx,RIP-ARGOFFSET(%rsp)	
	GET_CURRENT(%rcx)
	testl $PT_TRACESYS,tsk_ptrace(%rcx)
	jne tracesys
	cmpq $__NR_syscall_max,%rax
	ja badsys
	movq %r10,%rcx
	call *sys_call_table(,%rax,8)  # XXX:	 rip relative
	movq %rax,RAX-ARGOFFSET(%rsp)
	.globl ret_from_sys_call
ret_from_sys_call:	
sysret_with_reschedule:
	GET_CURRENT(%rcx)
	cli 
	cmpq $0,tsk_need_resched(%rcx)
	jne sysret_reschedule
	cmpl $0,tsk_sigpending(%rcx)
	jne sysret_signal
sysret_restore_args:
	movq    RIP-ARGOFFSET(%rsp),%rcx
	RESTORE_ARGS 0,-ARG_SKIP,1
	movq	PDAREF(pda_oldrsp),%rsp
	swapgs
	sysretq
	
sysret_signal:
	sti
	xorl %esi,%esi		# oldset
	leaq -ARGOFFSET(%rsp),%rdi	# regs
	leaq do_signal(%rip),%rax
	call ptregscall_common	
sysret_signal_test:
	GET_CURRENT(%rcx)
	cli
	cmpq $0,tsk_need_resched(%rcx)
	je   sysret_restore_args
	sti
	call schedule
	jmp sysret_signal_test
	
sysret_reschedule:
	sti
	call schedule
	jmp sysret_with_reschedule	
	
tracesys:			 
	SAVE_REST
	movq $-ENOSYS,RAX(%rsp)
	FIXUP_TOP_OF_STACK %rdi
	movq %rsp,%rdi
	call syscall_trace
	LOAD_ARGS ARGOFFSET  /* reload args from stack in case ptrace changed it */
	RESTORE_REST
	cmpq $__NR_syscall_max,%rax
	ja  tracesys_done
tracesys_call:		/* backtrace marker */		
	movq %r10,%rcx	/* fixup for C */
	call *sys_call_table(,%rax,8)
	movq %rax,RAX-ARGOFFSET(%rsp)
tracesys_done:		/* backtrace marker */	
	SAVE_REST
	movq %rsp,%rdi
	call syscall_trace
	RESTORE_TOP_OF_STACK %rbx
	RESTORE_REST
	jmp ret_from_sys_call
		
badsys:
	movq $0,ORIG_RAX-ARGOFFSET(%rsp)
	movq $-ENOSYS,RAX-ARGOFFSET(%rsp)
	jmp ret_from_sys_call

/*
 * Syscall return path ending with IRET.
 * This can be either 64bit calls that require restoring of all registers 
 * (impossible with sysret) or 32bit calls. 	 
 */	
ENTRY(int_ret_from_sys_call)	
intret_test_kernel:
	testl $3,CS-ARGOFFSET(%rsp)		
	je retint_restore_args
intret_with_reschedule:
	GET_CURRENT(%rcx)
	cli 
	cmpq $0,tsk_need_resched(%rcx)
	jne intret_reschedule
	cmpl $0,tsk_sigpending(%rcx)
	jne intret_signal
	jmp retint_restore_args_swapgs
	
intret_reschedule:
	sti
	call schedule
	jmp intret_with_reschedule	

intret_signal:
	sti
	SAVE_REST
	xorq %rsi,%rsi		# oldset -> arg2 
	movq %rsp,%rdi		# &ptregs -> arg1		
	call do_signal
	RESTORE_REST
intret_signal_test:		
	GET_CURRENT(%rcx)
	cli
	cmpq $0,tsk_need_resched(%rcx)
	je   retint_restore_args_swapgs
	sti
	call schedule
	# RED-PEN: can we lose signals here?
	jmp  intret_signal_test
	
/* 
 * Certain special system calls that need to save a complete stack frame.
 */ 								
	
	.macro PTREGSCALL label,func
	.globl \label
\label:
	leaq	\func(%rip),%rax
	jmp	ptregscall_common
	.endm

	PTREGSCALL stub_clone, sys_clone
	PTREGSCALL stub_fork, sys_fork
	PTREGSCALL stub_vfork, sys_vfork
	PTREGSCALL stub_rt_sigsuspend, sys_rt_sigsuspend
	PTREGSCALL stub_sigaltstack, sys_sigaltstack
	PTREGSCALL stub_iopl, sys_iopl

ENTRY(ptregscall_common)
	popq %r11
	SAVE_REST
	movq %r11, %r15
	FIXUP_TOP_OF_STACK %r11
	call *%rax
	RESTORE_TOP_OF_STACK %r11
	movq %r15, %r11
	RESTORE_REST
	pushq %r11
	ret
	
ENTRY(stub_execve)
	popq %r11
	SAVE_REST
	movq %r11, %r15
	FIXUP_TOP_OF_STACK %r11
	call sys_execve
	GET_CURRENT(%rcx)
	testl $ASM_THREAD_IA32,tsk_thread+thread_flags(%rcx)
	jnz exec_32bit
	RESTORE_TOP_OF_STACK %r11
	movq %r15, %r11
	RESTORE_REST
	push %r11
	ret

exec_32bit:
	movq %rax,RAX(%rsp)
	RESTORE_REST
	jmp int_ret_from_sys_call
	
/*
 * sigreturn is special because it needs to restore all registers on return.
 * This cannot be done with SYSRET, so use the IRET return path instead.
 */                
ENTRY(stub_rt_sigreturn)
	addq $8, %rsp		
	SAVE_REST
	FIXUP_TOP_OF_STACK %r11
	call sys_rt_sigreturn
	movq %rax,RAX(%rsp) # fixme, this could be done at the higher layer
	RESTORE_REST
	jmp int_ret_from_sys_call

/* 
 * Interrupt entry/exit.
 *
 * Interrupt entry points save only callee clobbered registers, except
 * for signals again.
 *	
 * Entry runs with interrupts off.	
 */ 

/* 0(%rsp): interrupt number */ 
ENTRY(common_interrupt)
	testl $3,16(%rsp)	# from kernel?
	je   1f
	swapgs
1:	cld
#ifdef CONFIG_X86_REMOTE_DEBUG
	SAVE_ALL
	movq %rsp,%rdi
#else			
	SAVE_ARGS
	leaq -ARGOFFSET(%rsp),%rdi	# arg1 for handler
#endif	
	addl $1,PDAREF(pda_irqcount)	# XXX: should be merged with irq.c irqcount
	movq PDAREF(pda_irqstackptr),%rax
	cmoveq %rax,%rsp
	pushq %rdi			# save old stack
	call do_IRQ
	/* 0(%rsp): oldrsp-ARGOFFSET */ 
ENTRY(ret_from_intr)
	cli
	popq  %rdi
	subl $1,PDAREF(pda_irqcount)
	leaq ARGOFFSET(%rdi),%rsp
	testl $3,CS(%rdi)	# from kernel?
	je	retint_restore_args
	/* Interrupt came from user space */
retint_with_reschedule:
	GET_CURRENT(%rcx)
	cmpq $0,tsk_need_resched(%rcx) 
	jne retint_reschedule
	cmpl $0,tsk_sigpending(%rcx)
	jne retint_signal
retint_restore_args_swapgs:		
	swapgs
retint_restore_args:				
	RESTORE_ARGS 0,8						
iret_label:	
	iretq
	.section __ex_table,"a"
	.align 8
	.quad iret_label,bad_iret
	.previous
	.section .fixup,"ax"
	/* force a signal here? this matches i386 behaviour */
bad_iret:
	/* runs with kernelgs again */	
	movq $-9999,%rdi	/* better code? */
	jmp do_exit			
	.previous	

retint_signal:	
	sti
	SAVE_REST
	movq $-1,ORIG_RAX(%rsp) 			
	xorq %rsi,%rsi		# oldset
	movq %rsp,%rdi		# &pt_regs
	call do_signal
	RESTORE_REST
retint_signal_test:		
	cli
	GET_CURRENT(%rcx) 
	cmpq $0,tsk_need_resched(%rcx) 
	je   retint_restore_args_swapgs
	sti
	call schedule
	jmp retint_signal_test			
			
retint_reschedule:
	sti
	call schedule
	cli
	jmp retint_with_reschedule
		
/* IF:off, stack contains irq number on origrax */ 	
	.macro IRQ_ENTER	
	cld
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	leaq -48(%rsp),%rdi
	testl $3,136(%rdi)
	je 1f
	swapgs
1:	addl $1,%gs:pda_irqcount
	movq %gs:pda_irqstackptr,%rax
	cmoveq %rax,%rsp
	pushq %rdi
	.endm	

	.macro BUILD_SMP_INTERRUPT x,v
ENTRY(\x)
	push $\v-256
	IRQ_ENTER
	call smp_\x
	jmp ret_from_intr
	.endm

#ifdef CONFIG_SMP				
	BUILD_SMP_INTERRUPT reschedule_interrupt,RESCHEDULE_VECTOR
	BUILD_SMP_INTERRUPT invalidate_interrupt,INVALIDATE_TLB_VECTOR
	BUILD_SMP_INTERRUPT call_function_interrupt,CALL_FUNCTION_VECTOR
#endif
#ifdef CONFIG_X86_LOCAL_APIC
	BUILD_SMP_INTERRUPT apic_timer_interrupt,LOCAL_TIMER_VECTOR
	BUILD_SMP_INTERRUPT error_interrupt,ERROR_APIC_VECTOR
	BUILD_SMP_INTERRUPT spurious_interrupt,SPURIOUS_APIC_VECTOR
#endif
		
		
/*
 * Exception entry points.
 */ 		
	.macro zeroentry sym
	pushq $0	/* push error code/oldrax */ 
	pushq %rax	/* push real oldrax to the rdi slot */ 
	leaq  \sym(%rip),%rax
	jmp error_entry
	.endm	

	.macro errorentry sym
	pushq %rax
	leaq  \sym(%rip),%rax
	jmp error_entry
	.endm

/*
 * Exception entry point. This expects an error code/orig_rax on the stack
 * and the exception handler in %rax.	
 */ 		  				
 	ALIGN
error_entry:
	/* rdi slot contains rax, oldrax contains error code */
	pushq %rsi
	movq  8(%rsp),%rsi	/* load rax */
	pushq %rdx
	pushq %rcx
	pushq %rsi	/* store rax */ 
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	cld
	SAVE_REST
	xorl %r15d,%r15d	
	testl $3,CS(%rsp)
	je error_kernelspace
	swapgs	
error_action:
	movq  %rdi,RDI(%rsp) 	
	movq %rsp,%rdi
	movq ORIG_RAX(%rsp),%rsi	/* get error code */ 
	movq $-1,ORIG_RAX(%rsp)
	call *%rax
	/* r15d: swapgs flag */
error_exit:
	testl %r15d,%r15d
	jnz   error_restore
error_test:		
	cli	
	GET_CURRENT(%rcx)
	cmpq $0,tsk_need_resched(%rcx)
	jne  error_reschedule
	cmpl $0,tsk_sigpending(%rcx)
	jne  error_signal
error_restore_swapgs:					
	swapgs
error_restore:	
	RESTORE_REST
	jmp retint_restore_args
	
error_reschedule:
	sti
	call schedule
	jmp  error_test

error_signal:	
	sti
	xorq %rsi,%rsi
	movq %rsp,%rdi
	call do_signal
error_signal_test:
	GET_CURRENT(%rcx)	
	cli
	cmpq $0,tsk_need_resched(%rcx)
	je   error_restore_swapgs
	sti
	call schedule
	jmp  error_signal_test
	
error_kernelspace:	
	incl %r15d
	/* There are two places in the kernel that can potentially fault with
	   usergs. Handle them here. */	   
	leaq iret_label(%rip),%rdx
	cmpq %rdx,RIP(%rsp)
	je 1f
	/* check truncated address too. This works around a CPU issue */
	movl %edx,%edx	/* zero extend */
	cmpq %rdx,RIP(%rsp)
	je   1f
	cmpq $gs_change,RIP(%rsp)
	jne  error_action
	/* iret_label and gs_change are handled by exception handlers
	   and the exit points run with kernelgs again */
1:	swapgs
	jmp error_action

	/* Reload gs selector with exception handling */
	/* edi:	 new selector */ 
ENTRY(load_gs_index)
	pushf
	cli
	swapgs
gs_change:	
	movl %edi,%gs	
2:	mfence		/* workaround for opteron errata #88 */
	swapgs
	popf
	ret
	
	.section __ex_table,"a"
	.align 8
	.quad gs_change,bad_gs
	.previous

bad_gs:
	swapgs
	xorl %eax,%eax
	movl %eax,%gs
	jmp 2b
/*
 * Create a kernel thread.
 *
 * C extern interface:
 *	extern long arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
 *
 * asm input arguments:
 *	rdi: fn, rsi: arg, rdx: flags
 */
ENTRY(arch_kernel_thread)
	FAKE_STACK_FRAME $child_rip
	SAVE_ALL

	# rdi: flags, rsi: usp, rdx: will be &pt_regs
	movq %rdx,%rdi
	orq  $CLONE_VM, %rdi

	movq $-1, %rsi

	movq %rsp, %rdx

	# clone now
	call do_fork
	# save retval on the stack so it's popped before `ret`
	movq %rax, RAX(%rsp)

	/*
	 * It isn't worth to check for reschedule here,
	 * so internally to the x86_64 port you can rely on kernel_thread()
	 * not to reschedule the child before returning, this avoids the need
	 * of hacks for example to fork off the per-CPU idle tasks.
         * [Hopefully no generic code relies on the reschedule -AK]	
	 */
	RESTORE_ALL
	UNFAKE_STACK_FRAME
	ret
	
child_rip:
	/*
	 * Here we are in the child and the registers are set as they were
	 * at kernel_thread() invocation in the parent.
	 */
	movq %rdi, %rax
	movq %rsi, %rdi
	call *%rax
	# exit
	xorq %rdi, %rdi
	call do_exit

/*
 * execve(). This function needs to use IRET, not SYSRET, to set up all state properly.
 *
 * C extern interface:
 *	 extern long execve(char *name, char **argv, char **envp)
 *
 * asm input arguments:
 *	rdi: name, rsi: argv, rdx: envp
 *
 * We want to fallback into:
 *	extern long sys_execve(char *name, char **argv,char **envp, struct pt_regs regs)
 *
 * do_sys_execve asm fallback arguments:
 *	rdi: name, rsi: argv, rdx: envp, fake frame on the stack
 */
ENTRY(execve)
	FAKE_STACK_FRAME $0
	SAVE_ALL	
	call sys_execve
	movq %rax, RAX(%rsp)	
	RESTORE_REST
	testq %rax,%rax
	je int_ret_from_sys_call
	RESTORE_ARGS
	UNFAKE_STACK_FRAME
	ret

ENTRY(page_fault)
	errorentry do_page_fault

ENTRY(coprocessor_error)
	zeroentry do_coprocessor_error

ENTRY(simd_coprocessor_error)
	zeroentry do_simd_coprocessor_error	

ENTRY(device_not_available)
	pushq $-1	
	SAVE_ALL
	xorl %r15d,%r15d
	testl $3,CS(%rsp)
	jz 1f
	swapgs 
2:	movq  %cr0,%rax
	leaq  math_state_restore(%rip),%rcx
	leaq  math_emulate(%rip),%rbx
	testl $0x4,%eax
	cmoveq %rcx,%rbx
	call  *%rbx
	jmp  error_exit
1:	incl %r15d
	jmp  2b

ENTRY(debug)
	zeroentry do_debug

ENTRY(nmi)
	pushq $-1
	SAVE_ALL
	/* NMI could happen inside the critical section of a swapgs,
	   so it is needed to use this expensive way to check.
	   Rely on arch_prctl forbiding user space from setting a negative
	   GS. Only the kernel value is negative. */
	movl  $MSR_GS_BASE,%ecx
	rdmsr
	xorl  %ebx,%ebx
	testl %edx,%edx
	js    1f
	swapgs
	movl  $1,%ebx
1:	movq %rsp,%rdi
	call do_nmi
	cli
	testl %ebx,%ebx
	jz error_restore
	swapgs	
	jmp error_restore
	
ENTRY(int3)
	zeroentry do_int3	

ENTRY(overflow)
	zeroentry do_overflow

ENTRY(bounds)
	zeroentry do_bounds

ENTRY(invalid_op)
	zeroentry do_invalid_op	

ENTRY(coprocessor_segment_overrun)
	zeroentry do_coprocessor_segment_overrun

ENTRY(reserved)
	zeroentry do_reserved

ENTRY(double_fault)
	errorentry do_double_fault	

ENTRY(invalid_TSS)
	errorentry do_invalid_TSS

ENTRY(segment_not_present)
	errorentry do_segment_not_present

ENTRY(stack_segment)
	errorentry do_stack_segment

ENTRY(general_protection)
	errorentry do_general_protection

ENTRY(alignment_check)
	errorentry do_alignment_check

ENTRY(divide_error)
	zeroentry do_divide_error

ENTRY(spurious_interrupt_bug)
	zeroentry do_spurious_interrupt_bug

ENTRY(machine_check)
	zeroentry do_machine_check	

ENTRY(call_debug)
	zeroentry do_call_debug
	
