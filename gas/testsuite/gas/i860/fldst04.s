# fst.l (no relocations here)
	.text

	# Immediate form, no auto-increment.
	fst.l	%f0,0(%r0)
	fst.l	%f31,124(%r1)
	fst.l	%f30,256(%r2)
	fst.l	%f29,512(%r3)
	fst.l	%f28,1024(%r4)
	fst.l	%f27,4096(%r5)
	fst.l	%f26,8192(%r6)
	fst.l	%f25,16384(%r7)
	fst.l	%f25,32764(%r7)
	fst.l	%f23,-32768(%r7)
	fst.l	%f2,-16384(%r8)
	fst.l	%f3,-8192(%r9)
	fst.l	%f8,-4096(%r10)
	fst.l	%f9,-1024(%r11)
	fst.l	%f12,-508(%r12)
	fst.l	%f19,-248(%r13)
	fst.l	%f21,-4(%r14)

	# Immediate form, with auto-increment.
	fst.l	%f0,0(%r0)++
	fst.l	%f1,124(%r1)++
	fst.l	%f2,256(%r2)++
	fst.l	%f3,512(%r3)++
	fst.l	%f4,1024(%r4)++
	fst.l	%f5,4096(%r5)++
	fst.l	%f6,8192(%r6)++
	fst.l	%f7,16384(%r7)++
	fst.l	%f8,32764(%r7)++
	fst.l	%f9,-32768(%r7)++
	fst.l	%f10,-16384(%r8)++
	fst.l	%f11,-8192(%r9)++
	fst.l	%f12,-4096(%r10)++
	fst.l	%f13,-1024(%r11)++
	fst.l	%f14,-508(%r12)++
	fst.l	%f15,-248(%r13)++
	fst.l	%f16,-4(%r14)++

	# Index form, no auto-increment.
	fst.l	%f0,%r5(%r0)
	fst.l	%f31,%r6(%r1)
	fst.l	%f30,%r7(%r2)
	fst.l	%f29,%r8(%r3)
	fst.l	%f28,%r9(%r4)
	fst.l	%f27,%r0(%r5)
	fst.l	%f26,%r1(%r6)
	fst.l	%f25,%r12(%r7)
	fst.l	%f24,%r13(%r8)
	fst.l	%f23,%r14(%r9)
	fst.l	%f22,%r15(%r10)
	fst.l	%f21,%r16(%r11)
	fst.l	%f20,%r17(%r12)
	fst.l	%f19,%r28(%r13)
	fst.l	%f18,%r31(%r14)

	# Index form, with auto-increment.
	fst.l	%f0,%r5(%r0)++
	fst.l	%f1,%r6(%r1)++
	fst.l	%f2,%r7(%r2)++
	fst.l	%f3,%r8(%r3)++
	fst.l	%f4,%r9(%r4)++
	fst.l	%f5,%r0(%r5)++
	fst.l	%f6,%r1(%r6)++
	fst.l	%f7,%r12(%r7)++
	fst.l	%f8,%r13(%r8)++
	fst.l	%f9,%r14(%r9)++
	fst.l	%f10,%r15(%r10)++
	fst.l	%f11,%r16(%r11)++
	fst.l	%f12,%r17(%r12)++
	fst.l	%f13,%r28(%r13)++
	fst.l	%f14,%r31(%r14)++

