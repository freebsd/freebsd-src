	.text
	.globl	fc1
	.type	fc1,@function
fc1:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%eax
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sG3@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sG4@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* GD */
	leal	sG1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD */
	leal	sG1@tlsdesc(%ebx), %eax
	call	*sG1@tlscall(%eax)
	nop;nop;nop;nop

	/* GD */
	leal	sG2@tlsdesc(%ebx), %eax
	call	*sG2@tlscall(%eax)
	nop;nop;nop;nop

	/* GD */
	leal	sG2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE tpoff */
	leal	sG3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE tpoff */
	leal	sG3@tlsdesc(%ebx), %eax
	call	*sG3@tlscall(%eax)
	nop;nop;nop;nop

	/* GD -> IE ntpoff */
	leal	sG4@tlsdesc(%ebx), %eax
	call	*sG4@tlscall(%eax)
	nop;nop;nop;nop

	/* GD -> IE ntpoff */
	leal	sG4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE ntpoff */
	leal	sG5@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE ntpoff */
	leal	sG5@tlsdesc(%ebx), %eax
	call	*sG5@tlscall(%eax)
	nop;nop;nop;nop

	/* GD -> IE tpoff */
	leal	sG6@tlsdesc(%ebx), %eax
	call	*sG6@tlscall(%eax)
	nop;nop;nop;nop

	/* GD -> IE tpoff */
	leal	sG6@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sG5@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sG6@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	movl    -4(%ebp), %ebx
	leave
	ret
