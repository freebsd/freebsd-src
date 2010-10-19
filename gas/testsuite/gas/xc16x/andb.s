        .section .text
        .global _fun
xc16x_andb:
		andb rl0,rl1
		andb rl0,[r1]
		andb rl0,[r1+]
		andb rl0,#3
		andb rl0,#0xbe
		andb rl0,0x0230
		andb 0x320,rl0
