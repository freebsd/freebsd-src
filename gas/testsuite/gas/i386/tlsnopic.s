	.section ".tdata", "awT", @progbits
	.globl baz
	.hidden baz
	.globl var
	.hidden var2
bar:	.long 27
baz:	.long 29
var:	.long 31
var2:	.long 33
	.text
	.globl	fn
	.type	fn,@function
fn:
	/* Main binary, no PIC.  */
1:	movl	1b, %edx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %edx

	/* foo can be anywhere in startup TLS.  */
	movl	%gs:0, %eax
	subl	foo@GOTTPOFF(%edx), %eax
	/* %eax now contains &foo.  */

	/* bar only in the main program.  */
	movl	%gs:0, %eax
	subl	$bar@TPOFF, %eax
	/* %eax now contains &bar.  */

	/* baz only in the main program.  */
	movl	%gs:0, %ecx
	/* Arbitrary instructions in between.  */
	nop
	subl	$baz@TPOFF, %ecx
	/* %ecx now contains &baz.  */

	/* var and var2 only in the main program.  */
	movl	%gs:0, %ecx
	/* Arbitrary instructions in between.  */
	nop
	nop
	leal	var@NTPOFF(%ecx), %eax
	/* Arbitrary instructions in between.  */
	nop
	leal	var2@NTPOFF(%ecx), %edx

	/* foo can be anywhere in startup TLS.  */
	movl	foo@INDNTPOFF, %eax
	movl	%gs:(%eax), %eax
	/* %eax now contains foo.  */

	movl	%gs:0, %eax
	addl	foo@INDNTPOFF, %eax
	/* %eax now contains &foo.  */

	ret
