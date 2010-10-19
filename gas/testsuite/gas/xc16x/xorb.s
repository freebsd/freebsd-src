       .section .text
       .global _fun
xc16x_xorb:
	xorb  rl0,rl1
	xorb  rl0,[r1]
	xorb  rl0,[r1+]
	xorb  rl0,#3
	xorb  rl0,#0x34
	xorb  rl0,0x2403
	xorb  0x2403,rl0
