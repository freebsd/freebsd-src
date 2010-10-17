# fld.q (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fld.q	0(%r0),%f0
	fld.q	128(%r1),%f28
	fld.q	256(%r2),%f24
	fld.q	512(%r3),%f20
	fld.q	1024(%r4),%f16
	fld.q	4096(%r5),%f12
	fld.q	8192(%r6),%f8
	fld.q	16384(%r7),%f4
	fld.q	32752(%r7),%f0
	fld.q	-32768(%r7),%f28
	fld.q	-16384(%r8),%f24
	fld.q	-8192(%r9),%f20
	fld.q	-4096(%r10),%f16
	fld.q	-1024(%r11),%f12
	fld.q	-512(%r12),%f8
	fld.q	-256(%r13),%f4
	fld.q	-16(%r14),%f0

	# Immediate form, with auto-increment.
	fld.q	0(%r0)++,%f0
	fld.q	128(%r1)++,%f4
	fld.q	256(%r2)++,%f8
	fld.q	512(%r3)++,%f12
	fld.q	1024(%r4)++,%f16
	fld.q	4096(%r5)++,%f20
	fld.q	8192(%r6)++,%f24
	fld.q	16384(%r7)++,%f28
	fld.q	32752(%r7)++,%f0
	fld.q	-32768(%r7)++,%f4
	fld.q	-16384(%r8)++,%f8
	fld.q	-8192(%r9)++,%f12
	fld.q	-4096(%r10)++,%f16
	fld.q	-1024(%r11)++,%f20
	fld.q	-512(%r12)++,%f24
	fld.q	-256(%r13)++,%f28
	fld.q	-16(%r14)++,%f16

	# Index form, no auto-increment.
	fld.q	%r5(%r0),%f0
	fld.q	%r6(%r1),%f20
	fld.q	%r7(%r2),%f16
	fld.q	%r8(%r3),%f12
	fld.q	%r9(%r4),%f8
	fld.q	%r0(%r5),%f4
	fld.q	%r1(%r6),%f0
	fld.q	%r12(%r7),%f28
	fld.q	%r13(%r8),%f24
	fld.q	%r14(%r9),%f20
	fld.q	%r15(%r10),%f16
	fld.q	%r16(%r11),%f12
	fld.q	%r17(%r12),%f8
	fld.q	%r28(%r13),%f4
	fld.q	%r31(%r14),%f0

	# Index form, with auto-increment.
	fld.q	%r5(%r0)++,%f0
	fld.q	%r6(%r1)++,%f4
	fld.q	%r7(%r2)++,%f8
	fld.q	%r8(%r3)++,%f12
	fld.q	%r9(%r4)++,%f16
	fld.q	%r0(%r5)++,%f20
	fld.q	%r1(%r6)++,%f24
	fld.q	%r12(%r7)++,%f28
	fld.q	%r13(%r8)++,%f0
	fld.q	%r14(%r9)++,%f4
	fld.q	%r15(%r10)++,%f8
	fld.q	%r16(%r11)++,%f12
	fld.q	%r17(%r12)++,%f16
	fld.q	%r28(%r13)++,%f20
	fld.q	%r31(%r14)++,%f24

