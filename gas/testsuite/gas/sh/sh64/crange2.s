! Check PT to SHcompact within same section as SHmedia, and that PT to
! nearby SHmedia still gets the right offset.
	.text
	.mode SHmedia
shmedia:
	pt shmedia1,tr3
	pt shcompact1,tr4
shmedia1:
	ptb shcompact2,tr5
shmedia2:
	nop

	.mode SHcompact
shcompact: ! Have a label, so disassembling unrelocated code works.
	nop
	nop
shcompact1:
	nop
	nop
shcompact2:
	nop
	nop
shcompact3:
	nop
	nop
shcompact4:
	nop
	nop

	.mode SHmedia
shmedia3:
	pt shcompact3,tr6
	ptb shcompact4,tr7
	pt shmedia2,tr0
