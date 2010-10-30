@ test conditional compilation 

	.arm
	.text
	.syntax unified

	vldreq.32 d3,[r4]
	vmovlt.16 d3[1], r5
	vmovge d3, r4, r7
	vmovcc r3, r4, d30
	vmovne.32 d2[1],r3
	vmovcs r1,r2,d3
	vmovcc d4,r1,r2
