! Check MOVI expansion of local symbols that should get segment-relative
! relocations.
	.text
start:
	movi forw + 32,r33
	movi forwdata + 40,r54
	movi forwothertext + 44,r15
forw:
	movi forwotherdata + 48,r25

	.data
	.long 0		! To get a non-zero segment offset for "forwdata".
forwdata:
	.long 0

	.section .text.other,"ax"
forwdummylabel:		! Needed to hang a marker that this section is SHmedia.
	nop
	nop
forwothertext:
	nop

	.section .data.other,"aw"
	.long 0
	.long 0
forwotherdata:
	.long 0

