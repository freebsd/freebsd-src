.text
_start:
	movq	%dr8, %rax
	movq	%dr8, %rdi
	movq	%rax, %dr8
	movq	%rdi, %dr8

.att_syntax noprefix
	movq	dr8, rax
	movq	dr8, rdi
	movq	rax, dr8
	movq	rdi, dr8

.intel_syntax noprefix
	mov	rax, dr8
	mov	rdi, dr8
	mov	dr8, rax
	mov	dr8, rdi
