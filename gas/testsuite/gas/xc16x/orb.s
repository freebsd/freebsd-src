       .section .text
       .global _fun
xc16x_or:
	orb  rl0,rl1
	orb  rl0,[r1]
	orb  rl0,[r1+]
	orb  rl0,#3
	orb  rl0,#0x23
	orb  rl0,0x0234
	orb  0x0234,rl0
