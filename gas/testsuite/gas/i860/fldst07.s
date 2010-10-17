# pfld.l (no relocations here)
	.text

	# Immediate form, no auto-increment.
	pfld.l	0(%r0),%f0
	pfld.l	124(%r1),%f31
	pfld.l	256(%r2),%f30
	pfld.l	512(%r3),%f29
	pfld.l	1024(%r4),%f28
	pfld.l	4096(%r5),%f27
	pfld.l	8192(%r6),%f26
	pfld.l	16384(%r7),%f25
	pfld.l	32764(%r7),%f25
	pfld.l	-32768(%r7),%f23
	pfld.l	-16384(%r8),%f2
	pfld.l	-8192(%r9),%f3
	pfld.l	-4096(%r10),%f8
	pfld.l	-1024(%r11),%f9
	pfld.l	-508(%r12),%f12
	pfld.l	-248(%r13),%f19
	pfld.l	-4(%r14),%f21

	# Immediate form, with auto-increment.
	pfld.l	0(%r0)++,%f0
	pfld.l	124(%r1)++,%f1
	pfld.l	256(%r2)++,%f2
	pfld.l	512(%r3)++,%f3
	pfld.l	1024(%r4)++,%f4
	pfld.l	4096(%r5)++,%f5
	pfld.l	8192(%r6)++,%f6
	pfld.l	16384(%r7)++,%f7
	pfld.l	32764(%r7)++,%f8
	pfld.l	-32768(%r7)++,%f9
	pfld.l	-16384(%r8)++,%f10
	pfld.l	-8192(%r9)++,%f11
	pfld.l	-4096(%r10)++,%f12
	pfld.l	-1024(%r11)++,%f13
	pfld.l	-508(%r12)++,%f14
	pfld.l	-248(%r13)++,%f15
	pfld.l	-4(%r14)++,%f16

	# Index form, no auto-increment.
	pfld.l	%r5(%r0),%f0
	pfld.l	%r6(%r1),%f31
	pfld.l	%r7(%r2),%f30
	pfld.l	%r8(%r3),%f29
	pfld.l	%r9(%r4),%f28
	pfld.l	%r0(%r5),%f27
	pfld.l	%r1(%r6),%f26
	pfld.l	%r12(%r7),%f25
	pfld.l	%r13(%r8),%f24
	pfld.l	%r14(%r9),%f23
	pfld.l	%r15(%r10),%f22
	pfld.l	%r16(%r11),%f21
	pfld.l	%r17(%r12),%f20
	pfld.l	%r28(%r13),%f19
	pfld.l	%r31(%r14),%f18

	# Index form, with auto-increment.
	pfld.l	%r5(%r0)++,%f0
	pfld.l	%r6(%r1)++,%f1
	pfld.l	%r7(%r2)++,%f2
	pfld.l	%r8(%r3)++,%f3
	pfld.l	%r9(%r4)++,%f4
	pfld.l	%r0(%r5)++,%f5
	pfld.l	%r1(%r6)++,%f6
	pfld.l	%r12(%r7)++,%f7
	pfld.l	%r13(%r8)++,%f8
	pfld.l	%r14(%r9)++,%f9
	pfld.l	%r15(%r10)++,%f10
	pfld.l	%r16(%r11)++,%f11
	pfld.l	%r17(%r12)++,%f12
	pfld.l	%r28(%r13)++,%f13
	pfld.l	%r31(%r14)++,%f14

