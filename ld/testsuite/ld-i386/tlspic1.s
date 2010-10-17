	.section ".tdata", "awT", @progbits
	.globl sg1, sg2, sg3, sg4, sg5, sg6, sg7, sg8
	.globl sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
sg1:	.long 17
sg2:	.long 18
sg3:	.long 19
sg4:	.long 20
sg5:	.long 21
sg6:	.long 22
sg7:	.long 23
sg8:	.long 24
sl1:	.long 65
sl2:	.long 66
sl3:	.long 67
sl4:	.long 68
sl5:	.long 69
sl6:	.long 70
sl7:	.long 71
sl8:	.long 72
sh1:	.long 257
sh2:	.long 258
sh3:	.long 259
sh4:	.long 260
sh5:	.long 261
sh6:	.long 262
sh7:	.long 263
sh8:	.long 264
	.text
	.globl	fn1
	.type	fn1,@function
fn1:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%eax
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	nop;nop;nop;nop

	/* GD */
	leal	sg1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is referenced through @gottpoff too */
	leal	sg2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is referenced through @gotntpoff too */
	leal	sg3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is referenced through @gottpoff and
	   @gotntpoff too */
	leal	sg4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD against local variable */
	leal	sl1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against local variable referenced through @gottpoff too */
	leal	sl2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against local variable referenced through @gotntpoff
	   too */
	leal	sl3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against local variable referenced through @gottpoff and
	   @gotntpoff too */
	leal	sl4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD against hidden and local variable */
	leal	sh1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden and local variable referenced through
	   @gottpoff too */
	leal	sh2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden and local variable referenced through
	   @gotntpoff too */
	leal	sh3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden and local variable referenced through
	   @gottpoff and @gotntpoff too */
	leal	sh4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD against hidden but not local variable */
	leal	sH1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden but not local variable referenced through
	   @gottpoff too */
	leal	sH2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden but not local variable referenced through
	   @gotntpoff too */
	leal	sH3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE against hidden but not local variable referenced through
	   @gottpoff and @gotntpoff too */
	leal	sH4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* LD */
	leal	sl1@tlsldm(%ebx), %eax
	call	___tls_get_addr@PLT
	nop;nop
	leal	sl1@dtpoff(%eax), %edx
	nop;nop
	leal	2+sl2@dtpoff(%eax), %ecx
	nop;nop;nop;nop

	/* LD against hidden and local variables */
	leal	sh1@tlsldm(%ebx), %eax
	call	___tls_get_addr@PLT
	nop;nop
	leal	sh1@dtpoff(%eax), %edx
	nop;nop
	leal	sh2@dtpoff+3(%eax), %ecx
	nop;nop;nop;nop

	/* LD against hidden but not local variables */
	leal	sH1@tlsldm(%ebx), %eax
	call	___tls_get_addr@PLT
	nop;nop
	leal	sH1@dtpoff(%eax), %edx
	nop;nop
	leal	sH2@dtpoff+1(%eax), %ecx
	nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sg2@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	subl	sg4@gottpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sg3@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sg4@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE against local var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sl2@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against local var  */
	movl	%gs:0, %eax
	nop;nop
	subl	sl4@gottpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gotntpoff IE against local var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sl3@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sl4@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE against hidden and local var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sh2@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against hidden and local var  */
	movl	%gs:0, %eax
	nop;nop
	subl	sh4@gottpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden and local var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sh3@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden and local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sh4@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE against hidden but not local var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sH2@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against hidden but not local var  */
	movl	%gs:0, %eax
	nop;nop
	subl	sH4@gottpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden but not local var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sH3@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden but not local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sH4@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* Direct access through %gs  */

	/* @gotntpoff IE against global var  */
	movl	sg5@gotntpoff(%ebx), %ecx
	nop;nop
	movl	%gs:(%ecx), %edx
	nop;nop;nop;nop

	/* @gotntpoff IE against local var  */
	movl	sl5@gotntpoff(%ebx), %eax
	nop;nop
	movl	%gs:(%eax), %edx
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden and local var  */
	movl	sh5@gotntpoff(%ebx), %edx
	nop;nop
	movl	%gs:(%edx), %edx
	nop;nop;nop;nop

	/* @gotntpoff IE against hidden but not local var  */
	movl	sH5@gotntpoff(%ebx), %ecx
	nop;nop
	movl	%gs:(%ecx), %edx
	nop;nop;nop;nop

	movl    -4(%ebp), %ebx
	leave
	ret
