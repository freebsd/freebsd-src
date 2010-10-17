# shl, shr, shra, shrd 

	.text

	# Register forms (all)
	shl	%r0,%r1,%r2
	shl	%r3,%r4,%r5
	shl	%r6,%r7,%r8
	shl	%r9,%r10,%r11
	shl	%r31,%r13,%r14
	shl	%r15,%r16,%r17
	shl	%r18,%r19,%r20
	shl	%r21,%r22,%r23
	shl	%r24,%r25,%r31
	shl	%r27,%r28,%r29
	shl	%r30,%r31,%r0

	shr	%r0,%r1,%r2
	shr	%r3,%r4,%r5
	shr	%r6,%r7,%r8
	shr	%r9,%r10,%r11
	shr	%r31,%r13,%r14
	shr	%r15,%r16,%r17
	shr	%r18,%r19,%r20
	shr	%r21,%r22,%r23
	shr	%r24,%r25,%r31
	shr	%r27,%r28,%r29
	shr	%r30,%r31,%r0

	shra	%r0,%r1,%r2
	shra	%r3,%r4,%r5
	shra	%r6,%r7,%r8
	shra	%r9,%r10,%r11
	shra	%r31,%r13,%r14
	shra	%r15,%r16,%r17
	shra	%r18,%r19,%r20
	shra	%r21,%r22,%r23
	shra	%r24,%r25,%r31
	shra	%r27,%r28,%r29
	shra	%r30,%r31,%r0

	shrd	%r0,%r1,%r2
	shrd	%r3,%r4,%r5
	shrd	%r6,%r7,%r8
	shrd	%r9,%r10,%r11
	shrd	%r31,%r13,%r14
	shrd	%r15,%r16,%r17
	shrd	%r18,%r19,%r20
	shrd	%r21,%r22,%r23
	shrd	%r24,%r25,%r31
	shrd	%r27,%r28,%r29
	shrd	%r30,%r31,%r0

	# Immediate forms (shrd does not have an immediate form)
	shl	0,%r1,%r2
	shl	8192,%r4,%r5
	shl	5109,%r7,%r8
	shl	32767,%r10,%r11
	shl	-32768,%r13,%r14
	shl	-8192,%r16,%r17
	shl	-1,%r19,%r20
	shl	-21555,%r22,%r23
	shl	0x1234,%r25,%r26
	shl	0x0,%r28,%r29
	shl	0x3,%r31,%r0

	shr	0,%r1,%r2
	shr	8192,%r4,%r5
	shr	5109,%r7,%r8
	shr	32767,%r10,%r11
	shr	-32768,%r13,%r14
	shr	-8192,%r16,%r17
	shr	-1,%r19,%r20
	shr	-21555,%r22,%r23
	shr	0x1234,%r25,%r26
	shr	0x0,%r28,%r29
	shr	0x3,%r31,%r0

	shra	1,%r1,%r2
	shra	8193,%r4,%r5
	shra	5110,%r7,%r8
	shra	32767,%r10,%r11
	shra	-32768,%r13,%r14
	shra	-8192,%r16,%r17
	shra	-1,%r19,%r20
	shra	-21555,%r22,%r23
	shra	0x1234,%r25,%r26
	shra	0x0,%r28,%r29
	shra	0x3,%r31,%r0

