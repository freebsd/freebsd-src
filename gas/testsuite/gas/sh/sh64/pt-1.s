! Check simple use of PT/PTA.
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
start2:
	pta start3,tr4
	nop
start3:
	pta start4,tr3
	nop
