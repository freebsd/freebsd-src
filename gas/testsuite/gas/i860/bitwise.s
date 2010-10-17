# and, andh, andnot, andnoth, or, orh, xor, xorh 

	.text

	# Register forms (high variants do not have register forms).
	and	%r0,%r1,%r2
	and	%r3,%r4,%r5
	and	%r6,%r7,%r8
	and	%r9,%r10,%r11
	and	%r12,%r13,%r14
	and	%r15,%r16,%r17
	and	%r18,%r19,%r20
	and	%r21,%r22,%r23
	and	%r24,%r25,%r26
	and	%r27,%r28,%r29
	and	%r30,%r31,%r0

	andnot	%r0,%r1,%r2
	andnot	%r3,%r4,%r5
	andnot	%r6,%r7,%r8
	andnot	%r9,%r10,%r11
	andnot	%r12,%r13,%r14
	andnot	%r15,%r16,%r17
	andnot	%r18,%r19,%r20
	andnot	%r21,%r22,%r23
	andnot	%r24,%r25,%r26
	andnot	%r27,%r28,%r29
	andnot	%r30,%r31,%r0

	or	%r0,%r1,%r2
	or	%r3,%r4,%r5
	or	%r6,%r7,%r8
	or	%r9,%r10,%r11
	or	%r12,%r13,%r14
	or	%r15,%r16,%r17
	or	%r18,%r19,%r20
	or	%r21,%r22,%r23
	or	%r24,%r25,%r26
	or	%r27,%r28,%r29
	or	%r30,%r31,%r0

	xor	%r0,%r1,%r2
	xor	%r3,%r4,%r5
	xor	%r6,%r7,%r8
	xor	%r9,%r10,%r11
	xor	%r12,%r13,%r14
	xor	%r15,%r16,%r17
	xor	%r18,%r19,%r20
	xor	%r21,%r22,%r23
	xor	%r24,%r25,%r26
	xor	%r27,%r28,%r29
	xor	%r30,%r31,%r0

	# Immediate forms (all)
	and	0,%r1,%r2
	and	8192,%r4,%r5
	and	5109,%r7,%r8
	and	32768,%r10,%r11
	and	65000,%r13,%r14
	and	65535,%r16,%r17
	and	0xffff,%r19,%r20
	and	0xabcd,%r22,%r23
	and	0x1234,%r25,%r26
	and	0x0,%r28,%r29
	and	0x3,%r31,%r0

	andh	1,%r1,%r2
	andh	8193,%r4,%r5
	andh	5110,%r7,%r8
	andh	32769,%r10,%r11
	andh	65001,%r13,%r14
	andh	65535,%r16,%r17
	andh	0xffff,%r19,%r20
	andh	0xabcd,%r22,%r23
	andh	0x1234,%r25,%r26
	andh	0x0,%r28,%r29
	andh	0x3,%r31,%r0

	andnot	0,%r1,%r2
	andnot	8192,%r4,%r5
	andnot	5109,%r7,%r8
	andnot	32768,%r10,%r11
	andnot	65000,%r13,%r14
	andnot	65535,%r16,%r17
	andnot	0xffff,%r19,%r20
	andnot	0xabcd,%r22,%r23
	andnot	0x1234,%r25,%r26
	andnot	0x0,%r28,%r29
	andnot	0x3,%r31,%r0

	andnoth	1,%r1,%r2
	andnoth	8193,%r4,%r5
	andnoth	5110,%r7,%r8
	andnoth	32769,%r10,%r11
	andnoth	65001,%r13,%r14
	andnoth	65535,%r16,%r17
	andnoth	0xffff,%r19,%r20
	andnoth	0xabcd,%r22,%r23
	andnoth	0x1234,%r25,%r26
	andnoth	0x0,%r28,%r29
	andnoth	0x3,%r31,%r0

	or	0,%r1,%r2
	or	1,%r4,%r5
	or	2,%r7,%r8
	or	3,%r10,%r11
	or	65000,%r13,%r14
	or	65535,%r16,%r17
	or	0xffff,%r19,%r20
	or	0xabcd,%r22,%r23
	or	0x1234,%r25,%r26
	or	0x0,%r28,%r29
	or	0x3,%r31,%r0

	orh	0,%r1,%r2
	orh	1,%r4,%r5
	orh	2,%r7,%r8
	orh	3,%r10,%r11
	orh	65000,%r13,%r14
	orh	65535,%r16,%r17
	orh	0xffff,%r19,%r20
	orh	0xabcd,%r22,%r23
	orh	0x1234,%r25,%r26
	orh	0x0,%r28,%r29
	orh	0x3,%r31,%r0

	xor	0,%r1,%r2
	xor	1,%r4,%r5
	xor	2,%r7,%r8
	xor	3,%r10,%r11
	xor	65000,%r13,%r14
	xor	65535,%r16,%r17
	xor	0xffff,%r19,%r20
	xor	0xabcd,%r22,%r23
	xor	0x1234,%r25,%r26
	xor	0x0,%r28,%r29
	xor	0x3,%r31,%r0

	xorh	0,%r1,%r2
	xorh	1,%r4,%r5
	xorh	2,%r7,%r8
	xorh	3,%r10,%r11
	xorh	65000,%r13,%r14
	xorh	65535,%r16,%r17
	xorh	0xffff,%r19,%r20
	xorh	0xabcd,%r22,%r23
	xorh	0x1234,%r25,%r26
	xorh	0x0,%r28,%r29
	xorh	0x3,%r31,%r0

