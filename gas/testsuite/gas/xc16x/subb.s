	.section .text
	.global _fun
xc16x_subb:

	subb rl0,rl1
	subb rl0,[r1]
	subb rl0,[r1+]
	subb rl0,#0x1
	subb rl0,#0x43
	subb rl0,0x7643
	subb 0x7643,rl0
	
	
	

	
	

	