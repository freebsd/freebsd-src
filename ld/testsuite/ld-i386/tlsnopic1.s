	.section ".data.rel.ro", "aw", @progbits
	/* Align, so that .got is likely at address 0x2080.  */
	.balign	4096
	.section ".tbss", "awT", @nobits
bl1:	.space 4
bl2:	.space 4
bl3:	.space 4
bl4:	.space 4
bl5:	.space 4
	.text
	/* Align, so that fn3 is likely at address 0x1000.  */
	.balign	4096
	.globl	fn3
	.type	fn3,@function
fn3:
	pushl	%ebp
	movl	%esp, %ebp

	/* @indntpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sg1@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE against global var  */
	movl	sg2@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* @indntpoff IE against hidden var */
	movl	%gs:0, %eax
	nop;nop
	addl	sh1@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE against hidden var */
	movl	sh2@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* @indntpoff IE against local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	bl1@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE against local var  */
	movl	bl2@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* LE @tpoff, global var  */
	movl	$-3+sg3@tpoff, %edx
	nop;nop
	movl	%gs:0, %eax
	nop;nop
	subl	%edx, %eax
	nop;nop;nop;nop

	/* LE @tpoff, local var  */
	movl	$-1+bl3@tpoff, %eax
	nop;nop
	movl	%gs:0, %edx
	nop;nop
	subl	%eax, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, global var  */
	movl	%gs:0, %eax
	nop;nop
	leal	2+sg4@ntpoff(%eax), %edx
	nop;nop;nop;nop

	/* LE @ntpoff, hidden var, non-canonical sequence  */
	movl	$sh3@ntpoff, %eax
	nop;nop
	movl	%gs:0, %edx
	nop;nop
	addl	%eax, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, local var, non-canonical sequence */
	movl	%gs:0, %edx
	nop;nop
	addl	$bl4@ntpoff+1, %edx
	nop;nop;nop;nop

	/* Direct %gs access  */

	/* LE @ntpoff, global var  */
	movl	%gs:sg5@ntpoff, %eax
	nop;nop;nop;nop

	/* LE @ntpoff, local var  */
	movl	%gs:bl5@ntpoff+3, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, hidden var  */
	movl	%gs:1+sh4@ntpoff, %edx
	nop;nop;nop;nop

	movl    -4(%ebp), %ebx
	leave
	ret
