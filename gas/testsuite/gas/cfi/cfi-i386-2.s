	.text
	.globl foo
	.type foo,@function
	.cfi_startproc
foo:
	push %ebp
	.cfi_adjust_cfa_offset 4
	.cfi_offset %ebp, -8
	.align 4
	push %ebx
	.cfi_offset %ebx, -12
	.cfi_adjust_cfa_offset 4
	nop
	pop %ebx
	pop %ebp
	ret
	.cfi_endproc
