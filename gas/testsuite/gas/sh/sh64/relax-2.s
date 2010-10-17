! Check relaxation for PTB.  This is like relax-1.s, but presumably we can
! have bugs in the slight differences in limit-checking compared to PT and
! PTA.

	.mode SHmedia
start:
	nop
start2:
	ptb	x0,tr3
	.mode SHcompact
x1:
	.mode SHmedia
a1:
	ptb	x0,tr4
	.space 32767*4-4,0
	.mode SHcompact
x0:
	.mode SHmedia
a0:
	ptb	x1,tr5
	ptb	x1,tr6
	ptb	x1,tr6
	ptb	x1,tr7
