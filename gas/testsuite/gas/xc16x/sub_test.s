	.text
_start:
	sub r0,r1
	sub r0,[r1]
	sub r0,[r1+]
	sub r0,#0x1
	sub r0,#0x7643
	sub r0,0x7643
	sub 0x7643,r0
	
	sub r1,r0
	sub r1,[r0]
	sub r1,[r0+]
	sub r1,#0x1
	sub r1,#0xCDEF
	sub r1,0xCDEF
	sub 0xCDEF,r1


	subb rl0,rl1
	subb rl0,[r1]
	subb rl0,[r1+]
	subb rl0,#0x1
	subb rl0,#0x43
	subb rl0,0x7643
	subb 0x7643,rl0
	
	subb rl1,rl0
	subb rl1,[r0]
	subb rl1,[r0+]
	subb rl1,#0x1
	subb rl1,#0xCD
	subb rl1,0xCDEF
	subb 0xCDEF,rl1
	


	subc r0,r1
	subc r0,[r1]
	subc r0,[r1+]
	subc r0,#0x2
	subc r0,#0x43
	subc r0,0x7643
	subc 0x7643,r0
	
	subc r1,r0
	subc r1,[r0]
	subc r1,[r0+]
	subc r1,#0xC
	subc r1,#0xCD
	subc r1,0xCDEF
	subc 0xCDEF,r1

	subcb rl0,rl1
	subcb rl0,[r1]
	subcb rl0,[r1+]
	subcb rl0,#0x2
	subcb rl0,#0x43
	subcb rl0,0x7643
	subcb 0x7643,rl0

	subcb rl0,rl1
	subcb rl0,[r1]
	subcb rl0,[r1+]
	subcb rl0,#0x2
	subcb rl0,#0x43
	subcb rl0,0x7643
	subcb 0x7643,rl0
	
	