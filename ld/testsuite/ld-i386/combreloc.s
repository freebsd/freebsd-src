	.text
	.globl	_start, foo
	.type	_start,@function
_start:
	pushl	%ebp
	movl	%esp, %ebp
        pushl	%ebx
        call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	movl	_start@GOT(%ebx), %eax
	movl	(%eax), %eax
	call	foo@PLT
	movl	(%esp), %ebx
	leave
foo:	ret
	.data
	.long	_start
	.section "__libc_subfreeres", "aw", @progbits
	.long	_start
	.section "__libc_atexit", "aw", @progbits
	.long	_start
