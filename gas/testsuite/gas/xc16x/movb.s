	.section .text
	.global _fun
xc16x_movb:

	movb rl0,r2
	movb rl0,#0x12
	movb r3,[r2]
	movb rl0,[r2+]
	movb [-r2],rl0
	movb [r3],[r2+]
	movb [r3],[r2]
	movb [r2+],[r3]
	movb [r2],[r3+]
	movb rl0,[r3+#0x1234]
	movb [r3+#0x1234],rl0
	movb [r3],0x1234
	movb [r3],0xeeff
	movb 0x1234,[r3]
	movb rl0,0x12
	movb 0x12,rl0
	
	
	

	
	