	.section .text
	.global _fun
xc16x_add:

	addcb rl0,rl1
	addcb rl0,[r1]
	addcb rl0,[r1+]
	addcb rl0,#0x02
	addcb rl0,#0x23
	addcb 0x2387,rl0
	
	

	
	

	