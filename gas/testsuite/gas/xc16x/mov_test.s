	.xc16x
	mov r0,r1
	mov r0,#02
	mov r0,#0x0001
	mov r0,[r1]
	mov r0,[r1+]
	mov [r0],r1
	mov [-r0],r1
	mov [r0],[r1]
	mov [r0+],[r1]
	mov [r0],[r1+]
	mov r0,[r0+#0x0001]
	mov [r0+#0x0001],r0
	mov [r0],0x0001
	mov 0x0001,[r0]
	mov r0,0x0001
	mov 0x0001,r0
 
 	mov r0,r1
 	mov r0,#02
 	mov r0,#0xffff
 	mov r0,[r1]
 	mov r0,[r1+]
 	mov [r0],r1
 	mov [-r0],r1
 	mov [r0],[r1]
 	mov [r0+],[r1]
 	mov [r0],[r1+]
 	mov r0,[r0+#0xffff]
 	mov [r0+#0xffff],r0
 	mov [r0],0xffff
 	mov 0xffff,[r0]
 	mov r0,0xffff
	mov 0xffff,r0
 
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
 	movb [r3],0x1234
 	movb 0x1234,[r3]
 	movb rl0,0x12
 	movb 0x12,rl0
 	
 	movb rl0,r2
 	movb rl0,#0xff
 	movb r3,[r2]
 	movb rl0,[r2+]
 	movb [-r2],rl0
 	movb [r3],[r2+]
 	movb [r3],[r2]
 	movb [r2+],[r3]
 	movb [r2],[r3+]
 	movb rl0,[r3+#0xffff]
 	movb [r3+#0xffff],rl0
 	movb [r3],0xffff
 	movb [r3],0xffff
 	movb 0xffff,[r3]
 	movb rl0,0xff
 	movb 0xff,rl0	
 	
 	movbs  r0,rl1
	movbs  r0,0x12
	movbs  0x1234,rl0
	
	movbs  r0,rl1
	movbs  r0,0xff
	movbs  0xffff,rl0

	movbz r2,rl0
	movbz r0,0x1234
	movbz 0x1234,rl0
 	
	movbz r2,rl0
	movbz r0,0xffff
	movbz 0xffff,rl0
	