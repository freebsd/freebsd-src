# ld.l (no relocations here)
	.text

	ld.l	0(%r0),%r0
	ld.l	124(%r1),%r31
	ld.l	256(%r2),%r30
	ld.l	512(%r3),%r29
	ld.l	1024(%r4),%r28
	ld.l	4096(%r5),%r27
	ld.l	8192(%r6),%r26
	ld.l	16384(%r7),%r25
	ld.l	-16384(%r8),%r24
	ld.l	-8192(%r9),%r23
	ld.l	-4096(%r10),%r22
	ld.l	-1024(%r11),%r21
	ld.l	-508(%r12),%r20
	ld.l	-248(%r13),%r19
	ld.l	-4(%r14),%r18

	ld.l	%r5(%r0),%r0
	ld.l	%r6(%r1),%r31
	ld.l	%r7(%r2),%r30
	ld.l	%r8(%r3),%r29
	ld.l	%r9(%r4),%r28
	ld.l	%r0(%r5),%r27
	ld.l	%r1(%r6),%r26
	ld.l	%r12(%r7),%r25
	ld.l	%r13(%r8),%r24
	ld.l	%r14(%r9),%r23
	ld.l	%r15(%r10),%r22
	ld.l	%r16(%r11),%r21
	ld.l	%r17(%r12),%r20
	ld.l	%r28(%r13),%r19
	ld.l	%r31(%r14),%r18

