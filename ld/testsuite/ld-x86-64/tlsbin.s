	.section ".tbss", "awT", @nobits
	.globl bg1, bg2, bg3, bg4, bg5, bg6, bg7, bg8
bg1:	.space 4
bg2:	.space 4
bg3:	.space 4
bg4:	.space 4
bg5:	.space 4
bg6:	.space 4
bg7:	.space 4
bg8:	.space 4
bl1:	.space 4
bl2:	.space 4
bl3:	.space 4
bl4:	.space 4
bl5:	.space 4
bl6:	.space 4
bl7:	.space 4
bl8:	.space 4
	.text
	.globl	_start
	.type	_start,@function
_start:
	pushq	%rbp
	movq	%rsp, %rbp

	/* IE against global var  */
	movq	%fs:0, %r11
	nop;nop
	addq	sG6@gottpoff(%rip), %r11
	nop;nop;nop;nop

	/* IE -> LE against global var defined in exec  */
	movq	%fs:0, %rdx
	nop;nop
	addq	bg6@gottpoff(%rip), %rdx
	nop;nop;nop;nop

	/* IE -> LE against local var  */
	movq	%fs:0, %r12
	nop;nop
	addq	bl6@gottpoff(%rip), %r12
	nop;nop;nop;nop

	/* direct %fs access IE -> LE against local var  */
	movq	bl8@gottpoff(%rip), %rdx
	nop;nop
	movq	%fs:(%rdx), %rax
	nop;nop;nop;nop

	/* IE -> LE against hidden but not local var  */
	movq	%fs:0, %rdx
	nop;nop
	addq	sh6@gottpoff(%rip), %rdx
	nop;nop;nop;nop

	/* direct %fs access IE -> LE against hidden but not local var  */
	movq	sh8@gottpoff(%rip), %rdx
	nop;nop
	movq	%fs:(%rdx), %rax
	nop;nop;nop;nop

	/* LE, global var defined in exec  */
	movq	%fs:0, %rax
	nop;nop
	leaq	sg2@tpoff(%rax), %rdx
	nop;nop;nop;nop

	/* LE, local var, non-canonical sequence  */
	movq	$2+bl2@tpoff, %r9
	nop;nop
	movq	%fs:0, %rdx
	nop;nop
	addq	%r9, %rdx
	nop;nop;nop;nop

	/* LE, hidden var defined in exec, non-canonical sequence */
	movq	%fs:0, %rdx
	nop;nop
	addq	$sh2@tpoff+1, %rdx
	nop;nop;nop;nop

	/* Direct %fs access  */

	/* LE, global var defined in exec  */
	movq	%fs:sg3@tpoff, %rax
	nop;nop;nop;nop

	/* LE, local var  */
	movq	%fs:bl3@tpoff+3, %r10
	nop;nop;nop;nop

	/* LE, hidden var defined in exec  */
	movq	%fs:1+sh3@tpoff, %rdx
	nop;nop;nop;nop

	leave
	ret
