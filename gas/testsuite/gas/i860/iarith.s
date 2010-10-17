# addu, adds, subu, subs

	.text

	# Register forms
	addu	%r0,%r1,%r2
	addu	%r3,%r4,%r5
	addu	%r6,%r7,%r8
	addu	%r9,%r10,%r11
	addu	%r31,%r13,%r14
	addu	%r15,%r16,%r17
	addu	%r18,%r19,%r20
	addu	%r21,%r22,%r23
	addu	%r24,%r25,%r31
	addu	%r27,%r28,%r29
	addu	%r30,%r31,%r0

	adds	%r0,%r1,%r2
	adds	%r3,%r4,%r5
	adds	%r6,%r7,%r8
	adds	%r9,%r10,%r11
	adds	%r31,%r13,%r14
	adds	%r15,%r16,%r17
	adds	%r18,%r19,%r20
	adds	%r21,%r22,%r23
	adds	%r24,%r25,%r31
	adds	%r27,%r28,%r29
	adds	%r30,%r31,%r0

	subu	%r0,%r1,%r2
	subu	%r3,%r4,%r5
	subu	%r6,%r7,%r8
	subu	%r9,%r10,%r11
	subu	%r31,%r13,%r14
	subu	%r15,%r16,%r17
	subu	%r18,%r19,%r20
	subu	%r21,%r22,%r23
	subu	%r24,%r25,%r31
	subu	%r27,%r28,%r29
	subu	%r30,%r31,%r0

	subs	%r0,%r1,%r2
	subs	%r3,%r4,%r5
	subs	%r6,%r7,%r8
	subs	%r9,%r10,%r11
	subs	%r31,%r13,%r14
	subs	%r15,%r16,%r17
	subs	%r18,%r19,%r20
	subs	%r21,%r22,%r23
	subs	%r24,%r25,%r31
	subs	%r27,%r28,%r29
	subs	%r30,%r31,%r0

	# Immediate forms (all)
	addu	0,%r1,%r2
	addu	8192,%r4,%r5
	addu	5109,%r7,%r8
	addu	32767,%r10,%r11
	addu	-32768,%r13,%r14
	addu	-8192,%r16,%r17
	addu	-1,%r19,%r20
	addu	-21555,%r22,%r23
	addu	0x1234,%r25,%r26
	addu	0x0,%r28,%r29
	addu	0x3,%r31,%r0

	adds	0,%r1,%r2
	adds	8192,%r4,%r5
	adds	5109,%r7,%r8
	adds	32767,%r10,%r11
	adds	-32768,%r13,%r14
	adds	-8192,%r16,%r17
	adds	-1,%r19,%r20
	adds	-21555,%r22,%r23
	adds	0x1234,%r25,%r26
	adds	0x0,%r28,%r29
	adds	0x3,%r31,%r0

	subu	1,%r1,%r2
	subu	8193,%r4,%r5
	subu	5110,%r7,%r8
	subu	32767,%r10,%r11
	subu	-32768,%r13,%r14
	subu	-8192,%r16,%r17
	subu	-1,%r19,%r20
	subu	-21555,%r22,%r23
	subu	0x1234,%r25,%r26
	subu	0x0,%r28,%r29
	subu	0x3,%r31,%r0

	subs	1,%r1,%r2
	subs	8193,%r4,%r5
	subs	5110,%r7,%r8
	subs	32767,%r10,%r11
	subs	-32768,%r13,%r14
	subs	-8192,%r16,%r17
	subs	-1,%r19,%r20
	subs	-21555,%r22,%r23
	subs	0x1234,%r25,%r26
	subs	0x0,%r28,%r29
	subs	0x3,%r31,%r0

