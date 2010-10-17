! Check "datalabel" qualifier.
! This is the most simple use; references to local symbols where it is
! completely redundant.  Code tests are for SHcompact mode.

	.mode SHcompact
	.text
start:
	mova datalabel litpool1,r0
start1:
	mova datalabel litpool2 + 44,r0
start2:
	nop
	nop
litpool1:
	.long datalabel myrodata1
litpool2:
	.long datalabel myrodata2 + 20
	.long DATALABEL start1
	.long datalabel start2+42
	.long DataLabel $
	.long datalabel $+20

	.section .rodata
	.long datalabel foo4
myrodata1:
	.long DataLabel foo5 + 56
	.global myrodata2
myrodata2:
	.long datalabel $
	.long datalabel $+20

	.data
	.long DATALABEL myrodata2
foo:
	.long datalabel $
	.global foo2
foo2:
	.long datalabel $+20
	.global foo3
foo3:
	.long DataLabel foo2
foo4:
	.long datalabel foo3+20
foo5:
	.long DATALABEL start1
	.long datalabel start2+20
