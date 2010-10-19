	.section .text
	.global _fun
xc16x_subc:

	subc r0,r1
	subc r0,[r1]
	subc r0,[r1+]
	subc r0,#0x2
	subc r0,#0x43
	subc r0,0x7643
	subc 0x7643,r0
	
	
	

	
	

	