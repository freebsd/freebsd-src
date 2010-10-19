.text
_start:
	add r0,r1
	add r0,r2
	add r0,r3
	add r0,r4
	add r0,r5
	add r0,r6
	add r0,r7
	add r0,r8
	add r0,r9
	add r0,r10
	add r0,r11
	add r0,r12
	add r0,r13
	add r0,r14
	add r0,r15

	add r1,r0
	add r1,r2
	add r1,r3
	add r1,r4
	add r1,r5
	add r1,r6
	add r1,r7
	add r1,r8
	add r1,r9
	add r1,r10
	add r1,r11
	add r1,r12
	add r1,r13
	add r1,r14
	add r1,r15
	
   	add r2,r0
	add r2,r1
	add r2,r3
	add r2,r4
	add r2,r5
	add r2,r6
	add r2,r7
	add r2,r8
	add r2,r9
	add r2,r10
	add r2,r11
	add r2,r12
	add r2,r13
	add r2,r14
	add r2,r15
	
	add r3,r0
	add r3,r1
	add r3,r2
	add r3,r4
	add r3,r5
	add r3,r6
	add r3,r7
	add r3,r8
	add r3,r9
	add r3,r10
	add r3,r11
	add r3,r12
	add r3,r13
	add r3,r14
	add r3,r15
	
	add r0,[r1]
	add r0,[r1+]
	add r0,#3
	add r0,#0xffff
	add r0,0xffff
	add 0xffff,r0
	
	addb rl0,rh0
	addb rl0[r0]
	addb rl0,#3
	addb rl0,#0xff
	addb r0,0xff10
	addb 0xff10,r0
	
	addc r0,r1
	addc r0,[r1]
	addc r0,#3
	addc r0,#0xff12
	addc r0,#0xff12
	addc r0,0xff12
	addc 0xff12,r0
	
	addcb rl0,#3
	addcb rl0,#0xff
	addcb r0,0xff10
	addcb 0xff10,r0