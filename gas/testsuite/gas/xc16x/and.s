.section .text
.global  _fun

xc16x_and:

	and r0,r1
	and r0,[r1]
	and r0,[r1+]
	and r0,#3
	and r0,#0xfcbe
	and r0,0x0230
	and 0x320,r0
	
	