	.section ".tdata", "awT", @progbits
	.globl foo, baz
	.hidden baz
foo:	.long 25
bar:	.long 27
baz:	.long 29
	.text
	.globl	fn
	.type	fn,@function
fn:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%eax
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx

	/* Dynamic TLS model, foo not known to be in the current object  */
	leal	foo@TLSGD(,%ebx,1), %eax
	call	___tls_get_addr@PLT
	/* %eax now contains &foo  */

	/* Dynamic TLS model, bar and baz known to be in the current object  */
	leal	bar@TLSLDM(%ebx), %eax
	call	___tls_get_addr@PLT

	/* Just show that there can be arbitrary instructions here  */
	addl	$0, %edi

	leal	bar@DTPOFF(%eax), %edx
	/* %edx now contains &bar  */

	/* Again, arbitrary instructions  */
	addl	$0, %esi

	leal	baz@DTPOFF(%eax), %ecx
	/* %ecx now contains &baz  */

	movl    -4(%ebp), %ebx
	leave
	ret
