 .text

 .code16
_start16:
	movl	xtrn@got(%ebx), %eax
	calll	xtrn@plt

 .code32
_start32:
	movl	xtrn@got(%ebx), %eax
	calll	xtrn@plt

 .code64
_start64:
	movq	xtrn@got(%rbx), %rax
	callq	xtrn@plt
