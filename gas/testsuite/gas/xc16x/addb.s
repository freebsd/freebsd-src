	.section .text
	.global _fun
xc16x_add:

	addb rl0,rl1
	addb rl0,[r1]
	addb rl0,[r1+]
	addb rl0,#0x2
	addb rl0,#0x33
	addb rl0,0x2387
	addb 0x2387,rl0
