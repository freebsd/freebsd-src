	/* Force .got aligned to 4K, so it very likely gets at 0x804a100
	   (0x60 bytes .tdata and 0xa0 bytes .dynamic)  */
	.section ".tdata", "awT", @progbits
	.balign	4096
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
	/* Force .text aligned to 4K, so it very likely gets at 0x8049000.  */
	.text
	.balign	4096
	.globl	fn2
	.type	fn2,@function
fn2:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%eax
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable */
	leal	sG1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through @gottpoff too */
	leal	sG2@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through @gotntpoff too */
	leal	sG3@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through @gottpoff and @gotntpoff too */
	leal	sG4@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> LE with global variable defined in executable */
	leal	sg1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> LE with local variable defined in executable */
	leal	sl1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> LE with hidden variable defined in executable */
	leal	sh1@tlsgd(,%ebx,1), %eax
	call	___tls_get_addr@plt
	nop;nop;nop;nop

	/* LD -> LE */
	leal	sl1@tlsldm(%ebx), %eax
	call	___tls_get_addr@PLT
	nop;nop
	leal	sl1@dtpoff(%eax), %edx
	nop;nop
	leal	sl2@dtpoff(%eax), %ecx
	nop;nop;nop;nop

	/* LD -> LE against hidden variables */
	leal	sh1@tlsldm(%ebx), %eax
	call	___tls_get_addr@PLT
	nop;nop
	leal	sh1@dtpoff(%eax), %edx
	nop;nop
	leal	sh2@dtpoff(%eax), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	subl	sG2@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	subl	sG4@gottpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %ecx
	nop;nop
	addl	sG3@gotntpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sG4@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE -> LE against global var defined in exec */
	movl	%gs:0, %ecx
	nop;nop
	subl	sg1@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* @gotntpoff IE -> LE against local var */
	movl	%gs:0, %ecx
	nop;nop
	addl	sl1@gotntpoff(%ebx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE -> LE against hidden var */
	movl	%gs:0, %ecx
	nop;nop
	subl	sh1@gottpoff(%ebx), %ecx
	nop;nop;nop;nop

	/* Direct access through %gs  */

	/* @gotntpoff IE against global var  */
	movl	sG5@gotntpoff(%ebx), %ecx
	nop;nop
	movl	%gs:(%ecx), %edx
	nop;nop;nop;nop

	/* @gotntpoff IE->LE against local var  */
	movl	sl5@gotntpoff(%ebx), %eax
	nop;nop
	movl	%gs:(%eax), %edx
	nop;nop;nop;nop

	/* @gotntpoff IE->LE against hidden var  */
	movl	sh5@gotntpoff(%ebx), %edx
	nop;nop
	movl	%gs:(%edx), %edx
	nop;nop;nop;nop

	movl    -4(%ebp), %ebx
	leave
	ret
