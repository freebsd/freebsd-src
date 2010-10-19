	.section .text
	.global _fun
xc16x_sub:

	sub r0,r1
	sub r0,[r1]
	sub r0,[r1+]
	sub r0,#0x1
	sub r0,#0x7643
	sub r0,0x7643
	sub 0x7643,r0
	
	
	

	
	

	