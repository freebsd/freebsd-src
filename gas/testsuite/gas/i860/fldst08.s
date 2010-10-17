# pfld.d (no relocations here)
	.text

	# Immediate form, no auto-increment.
	pfld.d	0(%r0),%f0
	pfld.d	128(%r1),%f30
	pfld.d	256(%r2),%f28
	pfld.d	512(%r3),%f26
	pfld.d	1024(%r4),%f24
	pfld.d	4096(%r5),%f22
	pfld.d	8192(%r6),%f20
	pfld.d	16384(%r7),%f18
	pfld.d	32760(%r7),%f16
	pfld.d	-32768(%r7),%f14
	pfld.d	-16384(%r8),%f12
	pfld.d	-8192(%r9),%f10
	pfld.d	-4096(%r10),%f8
	pfld.d	-1024(%r11),%f6
	pfld.d	-512(%r12),%f4
	pfld.d	-248(%r13),%f2
	pfld.d	-8(%r14),%f0

	# Immediate form, with auto-increment.
	pfld.d	0(%r0)++,%f0
	pfld.d	128(%r1)++,%f2
	pfld.d	256(%r2)++,%f4
	pfld.d	512(%r3)++,%f6
	pfld.d	1024(%r4)++,%f8
	pfld.d	4096(%r5)++,%f10
	pfld.d	8192(%r6)++,%f12
	pfld.d	16384(%r7)++,%f14
	pfld.d	32760(%r7)++,%f16
	pfld.d	-32768(%r7)++,%f18
	pfld.d	-16384(%r8)++,%f20
	pfld.d	-8192(%r9)++,%f22
	pfld.d	-4096(%r10)++,%f24
	pfld.d	-1024(%r11)++,%f26
	pfld.d	-512(%r12)++,%f28
	pfld.d	-248(%r13)++,%f30
	pfld.d	-8(%r14)++,%f16

	# Index form, no auto-increment.
	pfld.d	%r5(%r0),%f0
	pfld.d	%r6(%r1),%f30
	pfld.d	%r7(%r2),%f28
	pfld.d	%r8(%r3),%f26
	pfld.d	%r9(%r4),%f24
	pfld.d	%r0(%r5),%f22
	pfld.d	%r1(%r6),%f20
	pfld.d	%r12(%r7),%f18
	pfld.d	%r13(%r8),%f16
	pfld.d	%r14(%r9),%f14
	pfld.d	%r15(%r10),%f12
	pfld.d	%r16(%r11),%f10
	pfld.d	%r17(%r12),%f8
	pfld.d	%r28(%r13),%f6
	pfld.d	%r31(%r14),%f4

	# Index form, with auto-increment.
	pfld.d	%r5(%r0)++,%f0
	pfld.d	%r6(%r1)++,%f2
	pfld.d	%r7(%r2)++,%f4
	pfld.d	%r8(%r3)++,%f6
	pfld.d	%r9(%r4)++,%f8
	pfld.d	%r0(%r5)++,%f10
	pfld.d	%r1(%r6)++,%f12
	pfld.d	%r12(%r7)++,%f14
	pfld.d	%r13(%r8)++,%f16
	pfld.d	%r14(%r9)++,%f18
	pfld.d	%r15(%r10)++,%f20
	pfld.d	%r16(%r11)++,%f22
	pfld.d	%r17(%r12)++,%f24
	pfld.d	%r28(%r13)++,%f26
	pfld.d	%r31(%r14)++,%f30

