# fld.l (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fld.l	0(%r0),%f0
	fld.l	124(%r1),%f31
	fld.l	256(%r2),%f30
	fld.l	512(%r3),%f29
	fld.l	1024(%r4),%f28
	fld.l	4096(%r5),%f27
	fld.l	8192(%r6),%f26
	fld.l	16384(%r7),%f25
	fld.l	32764(%r7),%f25
	fld.l	-32768(%r7),%f23
	fld.l	-16384(%r8),%f2
	fld.l	-8192(%r9),%f3
	fld.l	-4096(%r10),%f8
	fld.l	-1024(%r11),%f9
	fld.l	-508(%r12),%f12
	fld.l	-248(%r13),%f19
	fld.l	-4(%r14),%f21

	# Immediate form, with auto-increment.
	fld.l	0(%r0)++,%f0
	fld.l	124(%r1)++,%f1
	fld.l	256(%r2)++,%f2
	fld.l	512(%r3)++,%f3
	fld.l	1024(%r4)++,%f4
	fld.l	4096(%r5)++,%f5
	fld.l	8192(%r6)++,%f6
	fld.l	16384(%r7)++,%f7
	fld.l	32764(%r7)++,%f8
	fld.l	-32768(%r7)++,%f9
	fld.l	-16384(%r8)++,%f10
	fld.l	-8192(%r9)++,%f11
	fld.l	-4096(%r10)++,%f12
	fld.l	-1024(%r11)++,%f13
	fld.l	-508(%r12)++,%f14
	fld.l	-248(%r13)++,%f15
	fld.l	-4(%r14)++,%f16

	# Index form, no auto-increment.
	fld.l	%r5(%r0),%f0
	fld.l	%r6(%r1),%f31
	fld.l	%r7(%r2),%f30
	fld.l	%r8(%r3),%f29
	fld.l	%r9(%r4),%f28
	fld.l	%r0(%r5),%f27
	fld.l	%r1(%r6),%f26
	fld.l	%r12(%r7),%f25
	fld.l	%r13(%r8),%f24
	fld.l	%r14(%r9),%f23
	fld.l	%r15(%r10),%f22
	fld.l	%r16(%r11),%f21
	fld.l	%r17(%r12),%f20
	fld.l	%r28(%r13),%f19
	fld.l	%r31(%r14),%f18

	# Index form, with auto-increment.
	fld.l	%r5(%r0)++,%f0
	fld.l	%r6(%r1)++,%f1
	fld.l	%r7(%r2)++,%f2
	fld.l	%r8(%r3)++,%f3
	fld.l	%r9(%r4)++,%f4
	fld.l	%r0(%r5)++,%f5
	fld.l	%r1(%r6)++,%f6
	fld.l	%r12(%r7)++,%f7
	fld.l	%r13(%r8)++,%f8
	fld.l	%r14(%r9)++,%f9
	fld.l	%r15(%r10)++,%f10
	fld.l	%r16(%r11)++,%f11
	fld.l	%r17(%r12)++,%f12
	fld.l	%r28(%r13)++,%f13
	fld.l	%r31(%r14)++,%f14

