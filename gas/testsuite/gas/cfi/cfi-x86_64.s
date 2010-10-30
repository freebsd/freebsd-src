#; $ as -o test.o gas-cfi-test.s && gcc -nostdlib -o test test.o

	.text

#; func_locvars
#; - function with a space on the stack 
#;   allocated for local variables

	.type	func_locvars,@function
func_locvars:
	.cfi_startproc
	
	#; alocate space for local vars
	sub	$0x1234,%rsp
	.cfi_adjust_cfa_offset	0x1234
	
	#; dummy body
	movl	$1,%eax
	
	#; release space of local vars and return
	add	$0x1234,%rsp
	.cfi_adjust_cfa_offset	-0x1234
	ret
	.cfi_endproc

#; func_prologue
#; - functions that begins with standard
#;   prologue: "pushq %rbp; movq %rsp,%rbp"

	.type	func_prologue,@function
func_prologue:
	.cfi_startproc
	
	#; prologue, CFI is valid after 
	#; each instruction.
	pushq	%rbp
	.cfi_def_cfa_offset	16
	.cfi_offset		%rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register	%rbp

	#; function body
	call	func_locvars
	addl	$3, %eax

	#; epilogue with valid CFI
	#; (we're better than gcc :-)
	leaveq
	.cfi_def_cfa		%rsp, 8
	ret
	.cfi_endproc

#; func_otherreg
#; - function that moves frame pointer to 
#;   another register (r12) and then allocates 
#;   a space for local variables

	.type	func_otherreg,@function
func_otherreg:
	.cfi_startproc

	#; save frame pointer to r8
	movq	%rsp,%r8
	.cfi_def_cfa_register	r8

	#; alocate space for local vars
	#;  (no .cfi_{def,adjust}_cfa_offset here,
	#;   because CFA is computed from r8!)
	sub	$100,%rsp

	#; function body
	call	func_prologue
	addl	$2, %eax
	
	#; restore frame pointer from r8
	movq	%r8,%rsp
	.cfi_def_cfa_register	rsp
	ret
	.cfi_endproc

#; main
#; - typical function
	.type	main,@function
main:
	.cfi_startproc
	
	#; only function body that doesn't
	#; touch the stack at all.
	call	func_otherreg
	
	#; return
	ret
	.cfi_endproc

#; _start
#; - standard entry point

	.type	_start,@function
	.globl	_start
_start:
	.cfi_startproc
	call	main
	movq	%rax,%rdi
	movq	$0x3c,%rax
	syscall
	hlt
	.cfi_endproc

#; func_alldirectives
#; - test for all .cfi directives. 
#;   This function is never called and the CFI info doesn't make sense.

	.type	func_alldirectives,@function
func_alldirectives:
	.cfi_startproc simple
	.cfi_def_cfa	rsp,8
	nop
	.cfi_def_cfa_offset	16
	nop
	.cfi_def_cfa_register	r8
	nop
	.cfi_adjust_cfa_offset	0x1234
	nop
	.cfi_offset	%rsi, 0x10
	nop
	.cfi_register	%r8, %r9
	nop
	.cfi_remember_state
	nop
	.cfi_restore %rbp
	nop
	.cfi_undefined %rip
	nop
	.cfi_same_value rbx
	nop
	.cfi_restore_state
	ret
	.cfi_endproc
