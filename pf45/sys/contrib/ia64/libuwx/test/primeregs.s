	.text
	.proc	prime_registers
	.global prime_registers

prime_registers:

	.prologue

	.save ar.pfs, r32
	alloc	r32 = ar.pfs, 0, 3, 0, 0
	.save rp, r33
	mov	r33 = b0
	.save ar.unat, r34
	mov	r34 = ar.unat
	add	r14 = -56, sp
	add	r15 = -48, sp
	.fframe 80
	add	sp = -80, sp
	mov	r16 = b1
	;;

	.save.g 0x1
	st8.spill [r14] = r4, 16
	.save.g 0x2
	st8.spill [r15] = r5, 16
	mov	r17 = b2
	;;
	.save.g 0x4
	st8.spill [r14] = r6, 16
	.save.g 0x8
	st8.spill [r15] = r7, 16
	mov	r18 = b3
	;;
	.save.b 0x1
	st8	[r14] = r16, 16
	.save.b 0x2
	st8	[r15] = r17, 16
	mov	r19 = b4
	;;
	.save.b 0x4
	st8	[r14] = r18, 16
	.save.b 0x8
	st8	[r15] = r19
	mov	r20 = b5
	;;
	.save.b 0x10
	st8	[r14] = r20

	.body

	dep.z	r4 = -0x34, 16, 32
	;;
	add	r5 = 1, r4
	add	r6 = 2, r4
	;;
	add	r7 = 3, r4
	;;

	.global func1
	.type	func1, @function
	br.call.sptk b0 = func1
	;;

	add	r14 = 80, sp
	add	r15 = 88, sp
	;;
	ld8	r20 = [r15], -16
	;;
	ld8	r19 = [r14], -16
	ld8	r18 = [r15], -16
	mov	b5 = r20
	;;
	ld8	r17 = [r14], -16
	ld8	r16 = [r15], -16
	mov	b4 = r19
	;;
	ld8.fill r7 = [r14], -16
	ld8.fill r6 = [r15], -16
	mov	b3 = r18
	;;
	ld8.fill r5 = [r14]
	ld8.fill r4 = [r15]
	mov	b2 = r17
	mov	b1 = r16

	.restore sp
	mov	ar.pfs = r32
	;;
	add	sp = 80, sp
	mov	ar.unat = r34
	mov	b0 = r33
	br.ret.sptk	b0
	.endp
