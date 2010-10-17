! Check relaxation for MOVI PC-relative expansions.  Unfortunately, we
! can't check the 32 and 48 bit limit on a host with 32-bit longs, so we
! just check going from first state to the second state.

	.mode SHmedia
	.text
start:
	nop
start2:
	movi	(x0-4-$),r3
x1:
	movi	(x0-1-$),r4
	.space 32768-4,0
x0:
	movi	(x1-$),r5
	movi	(x1+3-$),r6

! These PC-relative expressions are here because of past bugs leading to
! premature symbol evaluation and assignment when they were exposed to
! relaxation.
! The expected result may need future tweaking if advances are done in
! relaxation.  At the time of this writing the expressions are not
! relaxed although the numbers will be in the right range finally.

	movi	(x1-x0),r7
	movi	(x0-1-x1),r8
	movi	(y1-y0),r8

	.section .text.another,"ax"
y0:
	movi	(x1-start2),r9
y1:
