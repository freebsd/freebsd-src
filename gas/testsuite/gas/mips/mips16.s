# Test the mips16 instruction set.

	.set	mips16

	.macro	ldst op, reg, base
	\op	\reg,0(\base)
	\op	\reg,1(\base)
	\op	\reg,2(\base)
	\op	\reg,3(\base)
	\op	\reg,4(\base)
	\op	\reg,8(\base)
	\op	\reg,16(\base)
	\op	\reg,32(\base)
	\op	\reg,64(\base)
	\op	\reg,128(\base)
	\op	\reg,256(\base)
	\op	\reg,512(\base)
	\op	\reg,1024(\base)
	\op	\reg,2048(\base)
	\op	\reg,-1(\base)
	\op	\reg,-2(\base)
	\op	\reg,-3(\base)
	\op	\reg,-4(\base)
	\op	\reg,-8(\base)
	\op	\reg,-16(\base)
	\op	\reg,-32(\base)
	\op	\reg,-64(\base)
	\op	\reg,-128(\base)
	\op	\reg,-256(\base)
	\op	\reg,-512(\base)
	\op	\reg,-1024(\base)
	\op	\reg,-2048(\base)
	.endm

	.p2align 3
data1:
	.word	0
insns1:
	ldst	ld, $2, $3
	ld	$2,data1
	ld	$2,data2
	ld	$2,bar
	ld	$2,quux
	ldst	ld, $2, $sp
	ldst	lwu, $2, $3
	ldst	lw, $2, $3
	lw	$2,data1
	lw	$2,data2
	lw	$2,bar
	lw	$2,quux
	ldst	lw, $2, $sp
	ldst	lh, $2, $3
	ldst	lhu, $2, $3
	ldst	lb, $2, $3
	ldst	lbu, $2, $3
	ldst	sd, $2, $3
	ldst	sd, $2, $sp
	ldst	sd, $31, $sp
	ldst	sw, $2, $3
	ldst	sw, $2, $sp
	ldst	sw, $31, $sp
	ldst	sh, $2, $3
	ldst	sb, $2, $3

	li	$2,0
	li	$2,1
	li	$2,256

	move	$2,$30
	move	$20,$2

	daddu	$2,$3,0
	daddu	$2,$3,1
	daddu	$2,$3,-1
	daddu	$2,$3,16
	daddu	$2,$3,-16
	daddu	$2,$3,$4
	daddu	$2,0
	daddu	$2,1
	daddu	$2,-1
	daddu	$2,32
	daddu	$2,-32
	daddu	$2,128
	daddu	$2,-128
	dla	$2,data1
	dla	$2,data2
	dla	$2,bar
	dla	$2,quux
	daddu	$sp,0
	daddu	$sp,1
	daddu	$sp,-1
	daddu	$sp,256
	daddu	$sp,-256
	daddu	$2,$sp,0
	daddu	$2,$sp,1
	daddu	$2,$sp,-1
	daddu	$2,$sp,32
	daddu	$2,$sp,-32
	daddu	$2,$sp,128
	daddu	$2,$sp,-128

	addu	$2,$3,0
	addu	$2,$3,1
	addu	$2,$3,-1
	addu	$2,$3,16
	addu	$2,$3,-16
	addu	$2,$3,$4
	addu	$2,0
	addu	$2,1
	addu	$2,-1
	addu	$2,32
	addu	$2,-32
	addu	$2,128
	addu	$2,-128
	la	$2,data1
	la	$2,data2
	la	$2,bar
	la	$2,quux
	addu	$sp,0
	addu	$sp,1
	addu	$sp,-1
	addu	$sp,256
	addu	$sp,-256
	addu	$2,$sp,0
	addu	$2,$sp,1
	addu	$2,$sp,-1
	addu	$2,$sp,32
	addu	$2,$sp,-32
	addu	$2,$sp,128
	addu	$2,$sp,-128

data2:
	.word	0
insns2:	
	dsubu	$2,$3,$4
	subu	$2,$3,$4
	neg	$2,$3

	and	$2,$3
	or	$2,$3
	xor	$2,$3
	not	$2,$3

	slt	$2,0
	slt	$2,1
	slt	$2,-1
	slt	$2,255
	slt	$2,256
	slt	$2,$3
	sltu	$2,0
	sltu	$2,1
	sltu	$2,-1
	sltu	$2,255
	sltu	$2,256
	sltu	$2,$3
	cmp	$2,0
	cmp	$2,1
	cmp	$2,255
	cmp	$2,256
	cmp	$2,$3

	dsll	$2,$3,0
	dsll	$2,$3,1
	dsll	$2,$3,8
	dsll	$2,$3,9
	dsll	$2,$3,63
	dsll	$2,$3
	dsrl	$2,0
	dsrl	$2,1
	dsrl	$2,8
	dsrl	$2,9
	dsrl	$2,63
	dsrl	$2,$3
	dsra	$2,0
	dsra	$2,1
	dsra	$2,8
	dsra	$2,9
	dsra	$2,63
	dsra	$2,$3

	mflo	$2
	mfhi	$3

	sll	$2,$3,0
	sll	$2,$3,1
	sll	$2,$3,8
	sll	$2,$3,9
	sll	$2,$3,31
	sll	$2,$3
	srl	$2,$3,0
	srl	$2,$3,1
	srl	$2,$3,8
	srl	$2,$3,9
	srl	$2,$3,31
	srl	$2,$3
	sra	$2,$3,0
	sra	$2,$3,1
	sra	$2,$3,8
	sra	$2,$3,9
	sra	$2,$3,31
	sra	$2,$3

	dmult	$2,$3
	dmultu	$2,$3
	ddiv	$2,$3
	ddivu	$2,$3

	mult	$2,$3
	multu	$2,$3
	div	$2,$3
	divu	$2,$3

	jr	$2
	jr	$31
	jalr	$31,$2

	beqz	$2,insns1
	beqz	$2,insns2
	beqz	$2,bar
	beqz	$2,quux
	bnez	$2,insns1
	bnez	$2,insns2
	bnez	$2,bar
	bnez	$2,quux
	bteqz	insns1
	bteqz	insns2
	bteqz	bar
	bteqz	quux
	btnez	insns1
	btnez	insns2
	btnez	bar
	btnez	quux
	b	insns1
	b	insns2
	b	bar
	b	quux

	break	0
	break	1
	break	63

	jal	extern

	entry
	entry	$4
	entry	$4-$6,$16
	entry	$16-$17,$31
	entry	$31
	exit
	exit	$16
	exit	$16-$17,$31
	exit	$31

	.p2align 3
bar:	

	.skip	200
quux:	
