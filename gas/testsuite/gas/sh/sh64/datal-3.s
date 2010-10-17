! Check "datalabel" qualifier.
! This is the next most simple use; references symbols defined in this file.
! Code tests are for SHmedia mode.

	.mode SHmedia
	.text
start:
	movi datalabel foo,r3
	movi DataLabel foo2 + 42,r3
	movi ((datalabel foo3 + 46) >> 16) & 65535,r3

	.section .rodata
	.long datalabel foo4
myrodata1:
	.long DATALABEL foo5 + 56
myrodata2:
	.long datalabel $
	.global myrodata3
myrodata3:
	.long datalabel $+20

	.text
	movi datalabel foo7 + 42,r30
	movi datalabel foo8,r30
	movi ((datalabel foo9 + 64) >> 16) & 65535,r3
	movi datalabel myrodata1,r56
foo:
	movi DATALABEL myrodata2+30,r21
foo2:
	movi DataLabel foo,r10
foo3:
	movi datalabel $,r33
foo4:
	movi datalabel $+40,r8
foo5:
	movi datalabel myrodata3,r44
	.global foo6
foo6:
	movi datalabel foo6 + 42,r30
	.global foo7
foo7:
	nop
	.global foo8
foo8:
	nop
	.global foo9
foo9:
	nop
