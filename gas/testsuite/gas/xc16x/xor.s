       .section .text
       .global _fun
xc16x_or:
	xor  r0,r1
	xor  r0,[r1]
	xor  r0,[r1+]
	xor  r0,#3
	xor  r0,#0x0234
	xor  r0,0x0234
	xor  0x0234,r0
