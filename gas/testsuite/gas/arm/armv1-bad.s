	.global entry
	.text
entry:
	str	r0, =0x00ff0000
	ldr	r0, {r1}
	ldr	r0, [r1, #4096]
	ldr	r0, [r1, #-4096]
	mov	r0, #0x1ff
	cmpl	r0, r0
	strh	r0, [r1]
	ldmfa	r4!, {r8, r9}^
	ldmfa	r4!, {r4, r8, r9}
	stmfa	r4!, {r8, r9}^
	stmdb	r4!, {r4, r8, r9}	@ This is OK.
	stmdb	r8!, {r4, r8, r9}
