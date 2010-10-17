! Check "datalabel" qualifier.
! This is the most simple use; references to local symbols where it is
! completely redundant.  Code tests are for SHmedia mode.

	.mode SHmedia
	.text
start:
	movi datalabel foo,r3
	movi DataLabel foo2 + 42,r3
	movi (datalabel (foo3 + 46) >> 16) & 65535,r3
	movi datalabel myrodata3 & 65535, r45
	movi datalabel myrodata4 & 65535, r45
	movi DATALABEL (myrodata2 + 50) & 65535, r45

	.section .rodata
	.long datalabel foo4
myrodata1:
	.long DATALABEL foo5 + 56
myrodata2:
	.long datalabel $
	.global myrodata3
myrodata3:
	.long datalabel $+20
myrodata4:
	.long datalabel myrodata1+0x100

	.data
	.long datalabel myrodata1
foo:
	.long DATALABEL myrodata2+30
foo2:
	.long DataLabel foo
foo3:
	.long datalabel $
foo4:
	.long datalabel $+40
foo5:
	.long datalabel myrodata3
	.global foo6
foo6:
	.long datalabel foo6 + 42
