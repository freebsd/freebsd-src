.text
_start:
	movq	%cr8, %rax
	movq	%cr8, %rdi
	movq	%rax, %cr8
	movq	%rdi, %cr8

.att_syntax noprefix
	movq	cr8, rax
	movq	cr8, rdi
	movq	rax, cr8
	movq	rdi, cr8

.intel_syntax noprefix
	mov	rax, cr8
	mov	rdi, cr8
	mov	cr8, rax
	mov	cr8, rdi
