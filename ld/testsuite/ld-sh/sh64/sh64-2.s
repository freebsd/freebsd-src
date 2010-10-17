! Test that all common kinds of relocs get right for simple cases.
! Main part.
	.text
	.global start
	.mode SHmedia
start:
	movi	foo,r33
	movi	bar,r21
	pt/l	bar,tr3
	movi	foobar,r43
	movi	baz2,r53
	movi	foobar2,r4
	pta	xyzzy,tr5
	pt/u	plugh,tr1

	.data
	.global foobar
foobar:	.long	baz
foobar2:
	.long	bar

	.section .text.other,"ax"
	.global xyzzy
xyzzy:
	nop
plugh:
	nop
