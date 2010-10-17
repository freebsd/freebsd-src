! Test MOVI pc-relative expansion within text section.

	.text
	.mode SHmedia
start:
	nop
	movi start2+8 - datalabel $,r30
	movi start3+4 - $,r30
	movi datalabel start4 + 8 - datalabel $,r30
	movi datalabel start5 + 12 - $,r30
	movi (datalabel start6 + 24 - datalabel $) & 65535,r40
	movi ((datalabel start7 + 32 - datalabel $) >> 16) & 65535,r50
	movi gstart2+8 - datalabel $,r30
	movi gstart3+4 - $,r30
	movi datalabel gstart4 + 8 - datalabel $,r30
	movi datalabel gstart5 + 12 - $,r30
	movi (datalabel gstart6 + 24 - datalabel $) & 65535,r40
	movi ((datalabel gstart7 + 32 - datalabel $) >> 16) & 65535,r50
start2:
	nop
start3:
	nop
start4:
	nop
start5:
	nop
start6:
	nop
start7:
	nop
	.global gstart2
gstart2:
	nop
	.global gstart3
gstart3:
	nop
	.global gstart4
gstart4:
	nop
	.global gstart5
gstart5:
	nop
	.global gstart6
gstart6:
	nop
	.global gstart7
gstart7:
	nop
