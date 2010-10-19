       .section .text
       .global _fun
xc16x_or:

	or  r0,r1
	or  r0,[r1]
	or  r0,[r1+]
	or  r0,#3
	or  r0,#0x0234
	or  r0,0x4536
	or  0x4536,r0
