# ld.s (no relocations here)
	.text

	ld.s	0(%r0),%r0
	ld.s	122(%r1),%r31
	ld.s	258(%r2),%r30
	ld.s	512(%r3),%r29
	ld.s	1028(%r4),%r28
	ld.s	4090(%r5),%r27
	ld.s	8190(%r6),%r26
	ld.s	16384(%r7),%r25
	ld.s	-16384(%r8),%r24
	ld.s	-8192(%r9),%r23
	ld.s	-4096(%r10),%r22
	ld.s	-1024(%r11),%r21
	ld.s	-508(%r12),%r20
	ld.s	-242(%r13),%r19
	ld.s	-2(%r14),%r18

	ld.s	%r5(%r0),%r0
	ld.s	%r6(%r1),%r31
	ld.s	%r7(%r2),%r30
	ld.s	%r8(%r3),%r29
	ld.s	%r9(%r4),%r28
	ld.s	%r0(%r5),%r27
	ld.s	%r1(%r6),%r26
	ld.s	%r12(%r7),%r25
	ld.s	%r13(%r8),%r24
	ld.s	%r14(%r9),%r23
	ld.s	%r15(%r10),%r22
	ld.s	%r16(%r11),%r21
	ld.s	%r17(%r12),%r20
	ld.s	%r28(%r13),%r19
	ld.s	%r31(%r14),%r18

