/* 
 * Some macros to handle stack frames in assembly.
 */ 

#include <linux/config.h>

#define R15 0
#define R14 8
#define R13 16
#define R12 24
#define RBP 32
#define RBX 40
/* arguments: interrupts/non tracing syscalls only save upto here*/
#define R11 48
#define R10 56	
#define R9 64
#define R8 72
#define RAX 80
#define RCX 88
#define RDX 96
#define RSI 104
#define RDI 112
#define ORIG_RAX 120       /* + error_code */ 
/* end of arguments */ 	
/* cpu exception frame or undefined in case of fast syscall. */
#define RIP 128
#define CS 136
#define EFLAGS 144
#define RSP 152
#define SS 160
#define ARGOFFSET R11

	.macro SAVE_ARGS addskip=0,norcx=0 	
	subq  $9*8+\addskip,%rsp
	movq  %rdi,8*8(%rsp) 
	movq  %rsi,7*8(%rsp) 
	movq  %rdx,6*8(%rsp)
	.if \norcx
	.else
	movq  %rcx,5*8(%rsp)
	.endif
	movq  %rax,4*8(%rsp) 
	movq  %r8,3*8(%rsp) 
	movq  %r9,2*8(%rsp) 
	movq  %r10,1*8(%rsp) 
	movq  %r11,(%rsp) 
	.endm

#define ARG_SKIP 9*8
	.macro RESTORE_ARGS skiprax=0,addskip=0,skiprcx=0
	movq (%rsp),%r11
	movq 1*8(%rsp),%r10
	movq 2*8(%rsp),%r9
	movq 3*8(%rsp),%r8
	.if \skiprax
	.else
	movq 4*8(%rsp),%rax
	.endif
	.if \skiprcx
	.else
	movq 5*8(%rsp),%rcx
	.endif
	movq 6*8(%rsp),%rdx
	movq 7*8(%rsp),%rsi
	movq 8*8(%rsp),%rdi
	.if ARG_SKIP+\addskip > 0
	addq $ARG_SKIP+\addskip,%rsp
	.endif
	.endm	

	.macro LOAD_ARGS offset
	movq \offset(%rsp),%r11
	movq \offset+8(%rsp),%r10
	movq \offset+16(%rsp),%r9
	movq \offset+24(%rsp),%r8
	movq \offset+40(%rsp),%rcx
	movq \offset+48(%rsp),%rdx
	movq \offset+56(%rsp),%rsi
	movq \offset+64(%rsp),%rdi
	movq \offset+72(%rsp),%rax
	.endm
			
	.macro SAVE_REST
	subq $6*8,%rsp
	movq %rbx,5*8(%rsp) 
	movq %rbp,4*8(%rsp) 
	movq %r12,3*8(%rsp) 
	movq %r13,2*8(%rsp) 
	movq %r14,1*8(%rsp) 
	movq %r15,(%rsp) 
	.endm		

#define REST_SKIP 6*8
	.macro RESTORE_REST
	movq (%rsp),%r15
	movq 1*8(%rsp),%r14
	movq 2*8(%rsp),%r13
	movq 3*8(%rsp),%r12
	movq 4*8(%rsp),%rbp
	movq 5*8(%rsp),%rbx
	addq $REST_SKIP,%rsp
	.endm
		
	.macro SAVE_ALL
	SAVE_ARGS
	SAVE_REST
	.endm
		
	.macro RESTORE_ALL addskip=0
	RESTORE_REST
	RESTORE_ARGS 0,\addskip
	.endm

	/* push in order ss, rsp, eflags, cs, rip */
	.macro FAKE_STACK_FRAME child_rip
	xorl %eax,%eax
	subq $6*8,%rsp
	movq %rax,5*8(%rsp)  /* ss */
	movq %rax,4*8(%rsp)  /* rsp */
	movq $(1<<9),3*8(%rsp)  /* eflags - enable interrupts */
	movq $__KERNEL_CS,2*8(%rsp) /* cs */
	movq \child_rip,1*8(%rsp)  /* rip */ 
	movq %rax,(%rsp)   /* orig_rax */ 
	.endm

	.macro UNFAKE_STACK_FRAME
	addq $8*6, %rsp
	.endm

	.macro icebp
	.byte 0xf1
	.endm	

#ifdef CONFIG_FRAME_POINTER
#define ENTER enter
#define LEAVE leave
#else
#define ENTER
#define LEAVE
#endif
