# fst.d (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fst.d	%f0,0(%r0)
	fst.d	%f30,128(%r1)
	fst.d	%f28,256(%r2)
	fst.d	%f26,512(%r3)
	fst.d	%f24,1024(%r4)
	fst.d	%f22,4096(%r5)
	fst.d	%f20,8192(%r6)
	fst.d	%f18,16384(%r7)
	fst.d	%f16,32760(%r7)
	fst.d	%f14,-32768(%r7)
	fst.d	%f12,-16384(%r8)
	fst.d	%f10,-8192(%r9)
	fst.d	%f8,-4096(%r10)
	fst.d	%f6,-1024(%r11)
	fst.d	%f4,-512(%r12)
	fst.d	%f2,-248(%r13)
	fst.d	%f0,-8(%r14)

	# Immediate form, with auto-increment.
	fst.d	%f0,0(%r0)++
	fst.d	%f2,128(%r1)++
	fst.d	%f4,256(%r2)++
	fst.d	%f6,512(%r3)++
	fst.d	%f8,1024(%r4)++
	fst.d	%f10,4096(%r5)++
	fst.d	%f12,8192(%r6)++
	fst.d	%f14,16384(%r7)++
	fst.d	%f16,32760(%r7)++
	fst.d	%f18,-32768(%r7)++
	fst.d	%f20,-16384(%r8)++
	fst.d	%f22,-8192(%r9)++
	fst.d	%f24,-4096(%r10)++
	fst.d	%f26,-1024(%r11)++
	fst.d	%f28,-512(%r12)++
	fst.d	%f30,-248(%r13)++
	fst.d	%f16,-8(%r14)++

	# Index form, no auto-increment.
	fst.d	%f0,%r5(%r0)
	fst.d	%f30,%r6(%r1)
	fst.d	%f28,%r7(%r2)
	fst.d	%f26,%r8(%r3)
	fst.d	%f24,%r9(%r4)
	fst.d	%f22,%r0(%r5)
	fst.d	%f20,%r1(%r6)
	fst.d	%f18,%r12(%r7)
	fst.d	%f16,%r13(%r8)
	fst.d	%f14,%r14(%r9)
	fst.d	%f12,%r15(%r10)
	fst.d	%f10,%r16(%r11)
	fst.d	%f8,%r17(%r12)
	fst.d	%f6,%r28(%r13)
	fst.d	%f4,%r31(%r14)

	# Index form, with auto-increment.
	fst.d	%f0,%r5(%r0)++
	fst.d	%f2,%r6(%r1)++
	fst.d	%f4,%r7(%r2)++
	fst.d	%f6,%r8(%r3)++
	fst.d	%f8,%r9(%r4)++
	fst.d	%f10,%r0(%r5)++
	fst.d	%f12,%r1(%r6)++
	fst.d	%f14,%r12(%r7)++
	fst.d	%f16,%r13(%r8)++
	fst.d	%f18,%r14(%r9)++
	fst.d	%f20,%r15(%r10)++
	fst.d	%f22,%r16(%r11)++
	fst.d	%f24,%r17(%r12)++
	fst.d	%f26,%r28(%r13)++
	fst.d	%f30,%r31(%r14)++

