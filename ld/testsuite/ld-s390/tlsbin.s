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
	/* Function prolog */
	stm	%r6,%r14,24(%r15)
	bras	%r13,.LTN1
	/* Literal pool */
.LT1:
.LC0:
	.long	sG6@indntpoff
.LC1:
	.long	bg6@indntpoff
.LC2:
	.long	bl6@indntpoff
.LC3:
	.long	sh6@indntpoff
.LC4:
	.long	sg3@indntpoff
.LTN1:
	/* Function prolog */
	lr	%r14,%r15
	ahi	%r15,-96
	st	%r14,0(%r14)

	/* Extract TCB */
	ear	%r9,%a0

	/* IE against global var  */
	l	%r3,.LC0-.LT1(%r13)
	l	%r3,0(%r3,%r12):tls_load:sG6
	la	%r3,0(%r3,%r9)

	/* IE -> LE against global var defined in exec  */
	l	%r3,.LC1-.LT1(%r13)
	l	%r4,0(%r3,%r12):tls_load:bg6
	la	%r5,0(%r4,%r9)

	/* IE -> LE against local var  */
	l	%r3,.LC2-.LT1(%r13)
	l	%r4,0(%r3,%r12):tls_load:bl6
	la	%r5,0(%r4,%r9)

	/* IE -> LE against hidden but not local var  */
	l	%r3,.LC3-.LT1(%r13)
	l	%r4,0(%r3,%r12):tls_load:sh6
	la	%r5,0(%r4,%r9)

	/* LE, global var defined in exec  */
	l	%r4,.LC4-.LT1(%r13)
	la	%r5,0(%r4,%r9)

	/* Function epilog */
	lm	%r6,%r14,120(%r15)
	br	%r14
