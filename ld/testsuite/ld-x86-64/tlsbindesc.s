	/* Force .data aligned to 4K, so that .got very likely gets at
	   0x5021a0 (0x60 bytes .tdata and 0x140 bytes .dynamic)  */
	.data
	.balign	4096
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
	/* Force .text aligned to 4K, so it very likely gets at 0x401000.  */
	.text
	.balign	4096
	.globl	fn2
	.type	fn2,@function
fn2:
	pushq	%rbp
	movq	%rsp, %rbp

	/* GD -> IE because variable is not defined in executable */
	leaq	sG1@tlsdesc(%rip), %rax
	call	*sG1@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through IE too */
	leaq	sG2@tlsdesc(%rip), %rax
	call	*sG2@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> LE with global variable defined in executable */
	leaq	sg1@tlsdesc(%rip), %rax
	call	*sg1@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> LE with local variable defined in executable */
	leaq	sl1@tlsdesc(%rip), %rax
	call	*sl1@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> LE with hidden variable defined in executable */
	leaq	sh1@tlsdesc(%rip), %rax
	call	*sh1@tlscall(%rax)
	nop;nop;nop;nop

	/* LD -> LE */
	leaq	_TLS_MODULE_BASE_@tlsdesc(%rip), %rax
	call	*_TLS_MODULE_BASE_@tlscall(%rax)
	nop;nop
	leaq	1+sl1@dtpoff(%rax), %rdx
	nop;nop
	leaq	sl2@dtpoff+2(%rax), %r9
	nop;nop;nop;nop

	/* LD -> LE against hidden variables */
	leaq	sh1@dtpoff(%rax), %rdx
	nop;nop
	leaq	3+sh2@dtpoff(%rax), %rcx
	nop;nop;nop;nop

	/* IE against global var  */
	movq	%fs:0, %r9
	nop;nop
	addq	sG2@gottpoff(%rip), %r9
	nop;nop;nop;nop

	/* IE -> LE against global var defined in exec */
	movq	%fs:0, %r10
	nop;nop
	addq	sg1@gottpoff(%rip), %r10
	nop;nop;nop;nop

	/* IE -> LE against local var */
	movq	%fs:0, %rax
	nop;nop
	addq	sl1@gottpoff(%rip), %rax
	nop;nop;nop;nop

	/* IE -> LE against hidden var */
	movq	%fs:0, %rcx
	nop;nop
	addq	sh1@gottpoff(%rip), %rcx
	nop;nop;nop;nop

	/* Direct access through %fs  */

	/* IE against global var  */
	movq	sG5@gottpoff(%rip), %rcx
	nop;nop
	movq	%fs:(%rcx), %rdx
	nop;nop;nop;nop

	/* IE->LE against local var  */
	movq	sl5@gottpoff(%rip), %r11
	nop;nop
	movq	%fs:(%r11), %r12
	nop;nop;nop;nop

	/* IE->LE against hidden var  */
	movq	sh5@gottpoff(%rip), %rdx
	nop;nop
	movq	%fs:(%rdx), %rdx
	nop;nop;nop;nop

	leave
	ret
