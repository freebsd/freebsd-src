! Check inter-segment pt and pta
	.text
start:
	nop
start1:
	nop
start4:
	pt start1,tr5
	nop

	pt start2,tr7
	nop

	.section .text.other,"ax"
dummylabel:	! Needed to hang a marker that this is SHmedia.
	nop
start2:
	pta start3,tr4
	nop
start3:
	pta start4,tr3
	nop
