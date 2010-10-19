.text
_start:
	movl	%cr8, %eax
	movl	%cr8, %edi
	movl	%eax, %cr8
	movl	%edi, %cr8

.att_syntax noprefix
	movl	cr8, eax
	movl	cr8, edi
	movl	eax, cr8
	movl	edi, cr8

.intel_syntax noprefix
	mov	eax, cr8
	mov	edi, cr8
	mov	cr8, eax
	mov	cr8, edi
