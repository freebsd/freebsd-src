
	.macro SAVE_CALLEE_CLOBBERED
	cld
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rbx
	pushq %rax
	pushq %r8
	pushq %r9" 
	.endm
