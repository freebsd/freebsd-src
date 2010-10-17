# fld.d (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fld.d	0(%r0),%f0
	fld.d	128(%r1),%f30
	fld.d	256(%r2),%f28
	fld.d	512(%r3),%f26
	fld.d	1024(%r4),%f24
	fld.d	4096(%r5),%f22
	fld.d	8192(%r6),%f20
	fld.d	16384(%r7),%f18
	fld.d	32760(%r7),%f16
	fld.d	-32768(%r7),%f14
	fld.d	-16384(%r8),%f12
	fld.d	-8192(%r9),%f10
	fld.d	-4096(%r10),%f8
	fld.d	-1024(%r11),%f6
	fld.d	-512(%r12),%f4
	fld.d	-248(%r13),%f2
	fld.d	-8(%r14),%f0

	# Immediate form, with auto-increment.
	fld.d	0(%r0)++,%f0
	fld.d	128(%r1)++,%f2
	fld.d	256(%r2)++,%f4
	fld.d	512(%r3)++,%f6
	fld.d	1024(%r4)++,%f8
	fld.d	4096(%r5)++,%f10
	fld.d	8192(%r6)++,%f12
	fld.d	16384(%r7)++,%f14
	fld.d	32760(%r7)++,%f16
	fld.d	-32768(%r7)++,%f18
	fld.d	-16384(%r8)++,%f20
	fld.d	-8192(%r9)++,%f22
	fld.d	-4096(%r10)++,%f24
	fld.d	-1024(%r11)++,%f26
	fld.d	-512(%r12)++,%f28
	fld.d	-248(%r13)++,%f30
	fld.d	-8(%r14)++,%f16

	# Index form, no auto-increment.
	fld.d	%r5(%r0),%f0
	fld.d	%r6(%r1),%f30
	fld.d	%r7(%r2),%f28
	fld.d	%r8(%r3),%f26
	fld.d	%r9(%r4),%f24
	fld.d	%r0(%r5),%f22
	fld.d	%r1(%r6),%f20
	fld.d	%r12(%r7),%f18
	fld.d	%r13(%r8),%f16
	fld.d	%r14(%r9),%f14
	fld.d	%r15(%r10),%f12
	fld.d	%r16(%r11),%f10
	fld.d	%r17(%r12),%f8
	fld.d	%r28(%r13),%f6
	fld.d	%r31(%r14),%f4

	# Index form, with auto-increment.
	fld.d	%r5(%r0)++,%f0
	fld.d	%r6(%r1)++,%f2
	fld.d	%r7(%r2)++,%f4
	fld.d	%r8(%r3)++,%f6
	fld.d	%r9(%r4)++,%f8
	fld.d	%r0(%r5)++,%f10
	fld.d	%r1(%r6)++,%f12
	fld.d	%r12(%r7)++,%f14
	fld.d	%r13(%r8)++,%f16
	fld.d	%r14(%r9)++,%f18
	fld.d	%r15(%r10)++,%f20
	fld.d	%r16(%r11)++,%f22
	fld.d	%r17(%r12)++,%f24
	fld.d	%r28(%r13)++,%f26
	fld.d	%r31(%r14)++,%f30

