# fst.q (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fst.q	%f0,0(%r0)
	fst.q	%f28,128(%r1)
	fst.q	%f24,256(%r2)
	fst.q	%f20,512(%r3)
	fst.q	%f16,1024(%r4)
	fst.q	%f12,4096(%r5)
	fst.q	%f8,8192(%r6)
	fst.q	%f4,16384(%r7)
	fst.q	%f0,32752(%r7)
	fst.q	%f28,-32768(%r7)
	fst.q	%f24,-16384(%r8)
	fst.q	%f20,-8192(%r9)
	fst.q	%f16,-4096(%r10)
	fst.q	%f12,-1024(%r11)
	fst.q	%f8,-512(%r12)
	fst.q	%f4,-256(%r13)
	fst.q	%f0,-16(%r14)

	# Immediate form, with auto-increment.
	fst.q	%f0,0(%r0)++
	fst.q	%f4,128(%r1)++
	fst.q	%f8,256(%r2)++
	fst.q	%f12,512(%r3)++
	fst.q	%f16,1024(%r4)++
	fst.q	%f20,4096(%r5)++
	fst.q	%f24,8192(%r6)++
	fst.q	%f28,16384(%r7)++
	fst.q	%f0,32752(%r7)++
	fst.q	%f4,-32768(%r7)++
	fst.q	%f8,-16384(%r8)++
	fst.q	%f12,-8192(%r9)++
	fst.q	%f16,-4096(%r10)++
	fst.q	%f20,-1024(%r11)++
	fst.q	%f24,-512(%r12)++
	fst.q	%f28,-256(%r13)++
	fst.q	%f16,-16(%r14)++

	# Index form, no auto-increment.
	fst.q	%f0,%r5(%r0)
	fst.q	%f20,%r6(%r1)
	fst.q	%f16,%r7(%r2)
	fst.q	%f12,%r8(%r3)
	fst.q	%f8,%r9(%r4)
	fst.q	%f4,%r0(%r5)
	fst.q	%f0,%r1(%r6)
	fst.q	%f28,%r12(%r7)
	fst.q	%f24,%r13(%r8)
	fst.q	%f20,%r14(%r9)
	fst.q	%f16,%r15(%r10)
	fst.q	%f12,%r16(%r11)
	fst.q	%f8,%r17(%r12)
	fst.q	%f4,%r28(%r13)
	fst.q	%f0,%r31(%r14)

	# Index form, with auto-increment.
	fst.q	%f0,%r5(%r0)++
	fst.q	%f4,%r6(%r1)++
	fst.q	%f8,%r7(%r2)++
	fst.q	%f12,%r8(%r3)++
	fst.q	%f16,%r9(%r4)++
	fst.q	%f20,%r0(%r5)++
	fst.q	%f24,%r1(%r6)++
	fst.q	%f28,%r12(%r7)++
	fst.q	%f0,%r13(%r8)++
	fst.q	%f4,%r14(%r9)++
	fst.q	%f8,%r15(%r10)++
	fst.q	%f12,%r16(%r11)++
	fst.q	%f16,%r17(%r12)++
	fst.q	%f20,%r28(%r13)++
	fst.q	%f24,%r31(%r14)++

