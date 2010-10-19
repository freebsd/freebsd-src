	.section .text
	.global _fun
xc16x_subcb:

	subcb rl0,rl1
	subcb rl0,[r1]
	subcb rl0,[r1+]
	subcb rl0,#0x2
	subcb rl0,#0x43
	subcb rl0,0x7643
	subcb 0x7643,rl0
	
	
	
	

	
	

	