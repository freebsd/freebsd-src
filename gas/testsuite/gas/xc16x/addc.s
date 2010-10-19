	.section .text
	.global _fun
xc16x_add:

	addc r0,r1
	addc r0,[r1]
	addc r0,[r1+]
	addc r0,#0x34
	addc r0,#0x3456
	addc r0,0x2387
	addc 0x2387,r0
