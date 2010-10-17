# This tests the XP-only instructions:
#  ldint.x, ldio.x, stio.x, scyc.b, pfld.q
# And control registers:
#  %bear, %ccr, %p0, %p1, %p2, %p3

	.text

	# XP-only control registers
	ld.c	%bear,%r31
	ld.c	%bear,%r0
	ld.c	%ccr,%r5
	ld.c	%ccr,%r30
	ld.c	%p0,%r10
	ld.c	%p0,%r2
	ld.c	%p1,%r21
	ld.c	%p1,%r0
	ld.c	%p2,%r28
	ld.c	%p2,%r12
	ld.c	%p3,%r31
	ld.c	%p3,%r6

	st.c	%r0,%bear
	st.c	%r30,%bear
	st.c	%r7,%ccr
	st.c	%r31,%ccr
	st.c	%r11,%p0
	st.c	%r3,%p0
	st.c	%r22,%p1
	st.c	%r15,%p1
	st.c	%r29,%p2
	st.c	%r13,%p2
	st.c	%r4,%p3
	st.c	%r6,%p3

	# ldint.{s,b,l}
	ldint.l	%r0,%r5
	ldint.l	%r6,%r31
	ldint.l	%r7,%r30
	ldint.l	%r8,%r29
	ldint.l	%r9,%r28
	ldint.l	%r0,%r27
	ldint.l	%r1,%r26
	ldint.l	%r12,%r25
	ldint.l	%r13,%r24
	ldint.l	%r14,%r23
	ldint.l	%r15,%r22
	ldint.l	%r16,%r21
	ldint.l	%r17,%r20
	ldint.l	%r28,%r19
	ldint.l	%r31,%r18

	ldint.s	%r0,%r5
	ldint.s	%r6,%r31
	ldint.s	%r7,%r30
	ldint.s	%r8,%r29
	ldint.s	%r9,%r28
	ldint.s	%r0,%r27
	ldint.s	%r1,%r26
	ldint.s	%r12,%r25
	ldint.s	%r13,%r24
	ldint.s	%r14,%r23
	ldint.s	%r15,%r22
	ldint.s	%r16,%r21
	ldint.s	%r17,%r20
	ldint.s	%r28,%r19
	ldint.s	%r31,%r18

	ldint.b	%r0,%r5
	ldint.b	%r6,%r31
	ldint.b	%r7,%r30
	ldint.b	%r8,%r29
	ldint.b	%r9,%r28
	ldint.b	%r0,%r27
	ldint.b	%r1,%r26
	ldint.b	%r12,%r25
	ldint.b	%r13,%r24
	ldint.b	%r14,%r23
	ldint.b	%r15,%r22
	ldint.b	%r16,%r21
	ldint.b	%r17,%r20
	ldint.b	%r28,%r19
	ldint.b	%r31,%r18

	# ldio.{s,b,l}
	ldio.l	%r0,%r5
	ldio.l	%r6,%r31
	ldio.l	%r7,%r30
	ldio.l	%r8,%r29
	ldio.l	%r9,%r28
	ldio.l	%r0,%r27
	ldio.l	%r1,%r26
	ldio.l	%r12,%r25
	ldio.l	%r13,%r24
	ldio.l	%r14,%r23
	ldio.l	%r15,%r22
	ldio.l	%r16,%r21
	ldio.l	%r17,%r20
	ldio.l	%r28,%r19
	ldio.l	%r31,%r18

	ldio.s	%r0,%r5
	ldio.s	%r6,%r31
	ldio.s	%r7,%r30
	ldio.s	%r8,%r29
	ldio.s	%r9,%r28
	ldio.s	%r0,%r27
	ldio.s	%r1,%r26
	ldio.s	%r12,%r25
	ldio.s	%r13,%r24
	ldio.s	%r14,%r23
	ldio.s	%r15,%r22
	ldio.s	%r16,%r21
	ldio.s	%r17,%r20
	ldio.s	%r28,%r19
	ldio.s	%r31,%r18

	ldio.b	%r0,%r5
	ldio.b	%r6,%r31
	ldio.b	%r7,%r30
	ldio.b	%r8,%r29
	ldio.b	%r9,%r28
	ldio.b	%r0,%r27
	ldio.b	%r1,%r26
	ldio.b	%r12,%r25
	ldio.b	%r13,%r24
	ldio.b	%r14,%r23
	ldio.b	%r15,%r22
	ldio.b	%r16,%r21
	ldio.b	%r17,%r20
	ldio.b	%r28,%r19
	ldio.b	%r31,%r18

	# stio.{s,b,l}
	stio.l	%r0,%r5
	stio.l	%r6,%r31
	stio.l	%r7,%r30
	stio.l	%r8,%r29
	stio.l	%r9,%r28
	stio.l	%r0,%r27
	stio.l	%r1,%r26
	stio.l	%r12,%r25
	stio.l	%r13,%r24
	stio.l	%r14,%r23
	stio.l	%r15,%r22
	stio.l	%r16,%r21
	stio.l	%r17,%r20
	stio.l	%r28,%r19
	stio.l	%r31,%r18

	stio.s	%r0,%r5
	stio.s	%r6,%r31
	stio.s	%r7,%r30
	stio.s	%r8,%r29
	stio.s	%r9,%r28
	stio.s	%r0,%r27
	stio.s	%r1,%r26
	stio.s	%r12,%r25
	stio.s	%r13,%r24
	stio.s	%r14,%r23
	stio.s	%r15,%r22
	stio.s	%r16,%r21
	stio.s	%r17,%r20
	stio.s	%r28,%r19
	stio.s	%r31,%r18

	stio.b	%r0,%r5
	stio.b	%r6,%r31
	stio.b	%r7,%r30
	stio.b	%r8,%r29
	stio.b	%r9,%r28
	stio.b	%r0,%r27
	stio.b	%r1,%r26
	stio.b	%r12,%r25
	stio.b	%r13,%r24
	stio.b	%r14,%r23
	stio.b	%r15,%r22
	stio.b	%r16,%r21
	stio.b	%r17,%r20
	stio.b	%r28,%r19
	stio.b	%r31,%r18

	# scyc.b
	scyc.b	%r0
	scyc.b	%r5
	scyc.b	%r6
	scyc.b	%r13
	scyc.b	%r14
	scyc.b	%r28
	scyc.b	%r29
	scyc.b	%r30
	scyc.b	%r31

	# pfld.q
	# Immediate form, no auto-increment.
	pfld.q	0(%r0),%f0
	pfld.q	128(%r1),%f28
	pfld.q	256(%r2),%f24
	pfld.q	512(%r3),%f20
	pfld.q	1024(%r4),%f16
	pfld.q	4096(%r5),%f12
	pfld.q	8192(%r6),%f8
	pfld.q	16384(%r7),%f4
	pfld.q	32760(%r7),%f0
	pfld.q	-32768(%r7),%f28
	pfld.q	-16384(%r8),%f24
	pfld.q	-8192(%r9),%f20
	pfld.q	-4096(%r10),%f16
	pfld.q	-1024(%r11),%f12
	pfld.q	-512(%r12),%f8
	pfld.q	-248(%r13),%f4
	pfld.q	-8(%r14),%f0

	# Immediate form, with auto-increment.
	pfld.q	0(%r0)++,%f0
	pfld.q	128(%r1)++,%f4
	pfld.q	256(%r2)++,%f8
	pfld.q	512(%r3)++,%f12
	pfld.q	1024(%r4)++,%f16
	pfld.q	4096(%r5)++,%f20
	pfld.q	8192(%r6)++,%f24
	pfld.q	16384(%r7)++,%f28
	pfld.q	32760(%r7)++,%f0
	pfld.q	-32768(%r7)++,%f4
	pfld.q	-16384(%r8)++,%f8
	pfld.q	-8192(%r9)++,%f12
	pfld.q	-4096(%r10)++,%f16
	pfld.q	-1024(%r11)++,%f20
	pfld.q	-512(%r12)++,%f24
	pfld.q	-248(%r13)++,%f28
	pfld.q	-8(%r14)++,%f16

	# Index form, no auto-increment.
	pfld.q	%r5(%r0),%f28
	pfld.q	%r6(%r1),%f24
	pfld.q	%r7(%r2),%f20
	pfld.q	%r8(%r3),%f16
	pfld.q	%r9(%r4),%f12
	pfld.q	%r0(%r5),%f8
	pfld.q	%r1(%r6),%f4
	pfld.q	%r12(%r7),%f0
	pfld.q	%r13(%r8),%f28
	pfld.q	%r14(%r9),%f24
	pfld.q	%r15(%r10),%f20
	pfld.q	%r16(%r11),%f16
	pfld.q	%r17(%r12),%f12
	pfld.q	%r28(%r13),%f8
	pfld.q	%r31(%r14),%f4

	# Index form, with auto-increment.
	pfld.q	%r5(%r0)++,%f0
	pfld.q	%r6(%r1)++,%f4
	pfld.q	%r7(%r2)++,%f8
	pfld.q	%r8(%r3)++,%f12
	pfld.q	%r9(%r4)++,%f16
	pfld.q	%r0(%r5)++,%f20
	pfld.q	%r1(%r6)++,%f24
	pfld.q	%r12(%r7)++,%f28
	pfld.q	%r13(%r8)++,%f0
	pfld.q	%r14(%r9)++,%f4
	pfld.q	%r15(%r10)++,%f8
	pfld.q	%r16(%r11)++,%f12
	pfld.q	%r17(%r12)++,%f16
	pfld.q	%r28(%r13)++,%f20
	pfld.q	%r31(%r14)++,%f24


