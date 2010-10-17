# ld.b (no relocations here)
	.text

	ld.b	0(%r0),%r0
	ld.b	1(%r1),%r31
	ld.b	2(%r2),%r30
	ld.b	513(%r3),%r29
	ld.b	1028(%r4),%r28
	ld.b	4090(%r5),%r27
	ld.b	8190(%r6),%r26
	ld.b	16385(%r7),%r25
	ld.b	32007(%r7),%r25
	ld.b	32767(%r7),%r25
	ld.b	-32768(%r7),%r25
	ld.b	-32767(%r7),%r25
	ld.b	-16383(%r8),%r24
	ld.b	-8101(%r9),%r23
	ld.b	-4091(%r10),%r22
	ld.b	-1023(%r11),%r21
	ld.b	-509(%r12),%r20
	ld.b	-23(%r13),%r19
	ld.b	-1(%r14),%r18

	ld.b	%r5(%r0),%r0
	ld.b	%r6(%r1),%r31
	ld.b	%r7(%r2),%r30
	ld.b	%r8(%r3),%r29
	ld.b	%r9(%r4),%r28
	ld.b	%r0(%r5),%r27
	ld.b	%r1(%r6),%r26
	ld.b	%r12(%r7),%r25
	ld.b	%r13(%r8),%r24
	ld.b	%r14(%r9),%r23
	ld.b	%r15(%r10),%r22
	ld.b	%r16(%r11),%r21
	ld.b	%r17(%r12),%r20
	ld.b	%r28(%r13),%r19
	ld.b	%r31(%r14),%r18

