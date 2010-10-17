#; $ as -o test.o gas-cfi-test.s && gcc -nostdlib -o test test.o

	.text

#; func_locvars
#; - function with a space on the stack 
#;   allocated for local variables

	.type	func_locvars,@function
func_locvars:
	.cfi_startproc
	
	#; alocate space for local vars
	sub	$0x1234,%esp
	.cfi_adjust_cfa_offset	0x1234
	
	#; dummy body
	movl	$1,%eax
	
	#; release space of local vars and return
	add	$0x1234,%esp
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
	pushl	%ebp
	.cfi_def_cfa_offset	8
	.cfi_offset		ebp,-8
	movl	%esp, %ebp
	.cfi_def_cfa_register	ebp

	#; function body
	call	func_locvars
	addl	$3, %eax

	#; epilogue with valid CFI
	#; (we're better than gcc :-)
	leave
	.cfi_def_cfa_register	esp
	ret
	.cfi_endproc

#; func_otherreg
#; - function that moves frame pointer to 
#;   another register (r12) and then allocates 
#;   a space for local variables

	.type	func_otherreg,@function
func_otherreg:
	.cfi_startproc

	#; save frame pointer to ebx
	mov	%esp,%ebx
	.cfi_def_cfa_register	ebx

	#; alocate space for local vars
	#;  (no .cfi_{def,adjust}_cfa_offset here,
	#;   because CFA is computed from r12!)
	sub	$100,%esp

	#; function body
	call	func_prologue
	add	$2, %eax
	
	#; restore frame pointer from r12
	mov	%ebx,%esp
	.cfi_def_cfa		esp,4
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
	movl	%eax,%edi
	movl	$0x1,%eax
	int	$0x80
	hlt
	.cfi_endproc
