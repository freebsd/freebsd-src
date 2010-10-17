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
	pushl	%ebp
	movl	%esp, %ebp
	/* Set up .GOT pointer for non-pic @gottpoff sequences */
	call	1f
1:	popl	%ecx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ecx
	nop;nop;nop;nop

	/* @gottpoff IE against global var  */
	movl	%gs:0, %edx
	nop;nop
	subl	sG6@gottpoff(%ecx), %edx
	nop;nop;nop;nop

	/* @indntpoff IE against global var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sG7@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE against global var  */
	movl	sG8@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE -> LE against global var defined in exec  */
	movl	%gs:0, %edx
	nop;nop
	subl	bg6@gottpoff(%ecx), %edx
	nop;nop;nop;nop

	/* @indntpoff IE -> LE against global var defined in exec */
	movl	%gs:0, %eax
	nop;nop
	addl	bg7@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE -> LE against global var defined
	   in exec  */
	movl	bg8@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE -> LE against local var  */
	movl	%gs:0, %edx
	nop;nop
	subl	bl6@gottpoff(%ecx), %edx
	nop;nop;nop;nop

	/* @indntpoff IE -> LE against local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	bl7@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE -> LE against local var  */
	movl	bl8@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* @gottpoff IE -> LE against hidden but not local var  */
	movl	%gs:0, %edx
	nop;nop
	subl	sh6@gottpoff(%ecx), %edx
	nop;nop;nop;nop

	/* @indntpoff IE -> LE against hidden but not local var  */
	movl	%gs:0, %eax
	nop;nop
	addl	sh7@indntpoff, %eax
	nop;nop;nop;nop

	/* @indntpoff direct %gs access IE -> LE against hidden but not
	   local var  */
	movl	sh8@indntpoff, %edx
	nop;nop
	movl	%gs:(%edx), %eax
	nop;nop;nop;nop

	/* LE @tpoff, global var defined in exec  */
	movl	$sg1@tpoff, %edx
	nop;nop
	movl	%gs:0, %eax
	nop;nop
	subl	%edx, %eax
	nop;nop;nop;nop

	/* LE @tpoff, local var  */
	movl	$-1+bl1@tpoff, %eax
	nop;nop
	movl	%gs:0, %edx
	nop;nop
	subl	%eax, %edx
	nop;nop;nop;nop

	/* LE @tpoff, hidden var defined in exec  */
	movl	$sh1@tpoff-3, %eax
	nop;nop
	movl	%gs:0, %edx
	nop;nop
	subl	%eax, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, global var defined in exec  */
	movl	%gs:0, %eax
	nop;nop
	leal	sg2@ntpoff(%eax), %edx
	nop;nop;nop;nop

	/* LE @ntpoff, local var, non-canonical sequence  */
	movl	$2+bl2@ntpoff, %eax
	nop;nop
	movl	%gs:0, %edx
	nop;nop
	addl	%eax, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, hidden var defined in exec, non-canonical sequence */
	movl	%gs:0, %edx
	nop;nop
	addl	$sh2@ntpoff+1, %edx
	nop;nop;nop;nop

	/* Direct %gs access  */

	/* LE @ntpoff, global var defined in exec  */
	movl	%gs:sg3@ntpoff, %eax
	nop;nop;nop;nop

	/* LE @ntpoff, local var  */
	movl	%gs:bl3@ntpoff+3, %edx
	nop;nop;nop;nop

	/* LE @ntpoff, hidden var defined in exec  */
	movl	%gs:1+sh3@ntpoff, %edx
	nop;nop;nop;nop

	movl    -4(%ebp), %ebx
	leave
	ret
