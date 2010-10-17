 .data
	.byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0
 .section A
	.byte 1,1,1,1,1,1,1,1,1,1,1
 .previous
	.byte 0
 .previous
	.byte 1
 .pushsection B
	.byte 2,2,2,2,2,2,2,2,2,2,2,2
 .previous
	.byte 1
 .previous
	.byte 2
 .pushsection C
	.byte 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
 .previous
	.byte 2
 .previous
	.byte 3
 .popsection
	.byte 2
 .previous
	.byte 1
 .previous
	.byte 2
 .popsection
	.byte 1
 .previous
	.byte 0
 .previous
	.byte 1
