# test all instructions

start:
	abs	r21,r42

	add	r1,r2,r3
	add	r50,r51,0x1a
	add	r50,r51,0xdeadbeef

	add2h	r1,r2,r3
	add2h	r50,r51,0x1a
	add2h	r50,r51,0xdeadbeef

	addc	r1,r2,r3
	addc	r50,r51,0x1a
	addc	r50,r51,0xdeadbeef

	addhlll	r1,r2,r3
	addhlll	r50,r51,0x1a
	addhlll	r50,r51,0xdeadbeef

	addhllh	r1,r2,r3
	addhllh	r50,r51,0x1a
	addhllh	r50,r51,0xdeadbeef

	addhlhl	r1,r2,r3
	addhlhl	r50,r51,0x1a
	addhlhl	r50,r51,0xdeadbeef

	addhlhh	r1,r2,r3
	addhlhh	r50,r51,0x1a
	addhlhh	r50,r51,0xdeadbeef
		
	addhhll	r1,r2,r3
	addhhll	r50,r51,0x1a
	addhhll	r50,r51,0xdeadbeef

	addhhlh	r1,r2,r3
	addhhlh	r50,r51,0x1a
	addhhlh	r50,r51,0xdeadbeef

	addhhhl	r1,r2,r3
	addhhhl	r50,r51,0x1a
	addhhhl	r50,r51,0xdeadbeef

	addhhhh	r1,r2,r3
	addhhhh	r50,r51,0x1a
	addhhhh	r50,r51,0xdeadbeef
	
	adds	r1,r2,r3
	adds	r50,r51,0x1a
	adds	r50,r51,0xdeadbeef
		
	adds2h	r1,r2,r3
	adds2h	r50,r51,0x1a
	adds2h	r50,r51,0xdeadbeef
		
	and	r1,r2,r3
	and	r50,r51,0x1a
	and	r50,r51,0xdeadbeef

	andfg	f0,f1,f2
	andfg	f3,f4,5
	
	avg	r1,r2,r3
	avg	r4,r5,6
	avg	r50,r51,0xdeadbeef

	avg2h	r1,r2,r3
	avg2h	r4,r5,6
	avg2h	r50,r51,0xdeadbeef

	bclr	r1,r2,r3
	bclr	r4,r5,6
	
	bnot	r1,r2,r3
	bnot	r5,r51,6
			
	bra	r41
	bra	0x40
	bra	0xf00d

	bratnz	r41,r42
	bratnz	r1,0xf00d
	bratnz	r1,0xdeadf00d

	bratzr	r41,r42
	bratzr	r1,0xf00d
	bratzr	r1,0xdeadf00d

	bset	r1,r2,r3
	bset	r5,r51,6

	bsr	r41
	bsr	0xf00d
	bsr	0xdeadf00d

	bsrtnz	r41,r42
	bsrtnz	r1,0xf00d
	bsrtnz	r1,0xdeadf00d

	bsrtzr	r41,r42
	bsrtzr	r1,0xf00d
	bsrtzr	r1,0xdeadf00d

	btst	f1,r2,r3
	btst	f5,r51,6
						
	cmpeq	f0,r3,r1
	cmpne	f1,r20,r21
	cmpgt	f2,r31,r32
	cmpge	f3,r3,r4
	cmplt	f4,r3,r4
	cmple	f5,r3,r4
	cmpps	f6,r3,r4
	cmpng	f7,r3,r4				

	cmpugt	f2,r31,r32
	cmpuge	f3,r3,r4
	cmpult	f4,r3,r4
	cmpule	f5,r3,r4
		
	dbra	r1,r8 
	dbra	r1,0x100
	dbra	r1,0xdeadf00d

	dbrai	0x10,r31
	dbrai	0x10,0x100
	dbrai	0x10,0xdeadf00d

	dbsr	r1,r8 || nop
	dbsr	r1,0x100 || nop
	dbsr	r1,0xdeadf00d

	dbsri	0x20,r31 || nop
	dbsri	0x20,0x100 || nop
	dbsri	0x20,0xdeadf00d

	djmp	r1,r32
	djmp	r1,0xf00d
	djmp	r1,0xdeadf00d

	djmpi	0x30,r32
	djmpi	0x30,0xf00d
	djmpi	0x30,0xdeadf00d

	djsr	r1,r32
	djsr	r1,0xf00d
	djsr	r1,0xdeadf00d

	djsri	0x10,r32
	djsri	0x20,0xf00d
	djsri	0x40,0xdeadf00d
	
	jmp	r41
	jmp	0xf00d
	jmp	0xdeadf00d

	jmptnz	r41,r42
	jmptnz	r1,0xf00d
	jmptnz	r1,0xdeadf00d

	jmptzr	r41,r42
	jmptzr	r1,0xf00d
	jmptzr	r1,0xdeadf00d

	joinll	r1,r2,r4
	joinll	r1,r2,0xf
	joinll	r1,r2,0xdeadf00d	

	joinlh	r1,r2,r4
	joinlh	r1,r2,0xf
	joinlh	r1,r2,0xdeadf00d	

	joinhl	r1,r2,r4
	joinhl	r1,r2,0xf
	joinhl	r1,r2,0xdeadf00d	

	joinhh	r1,r2,r4
	joinhh	r1,r2,0xf
	joinhh	r1,r2,0xdeadf00d	

	jsr	r41
	jsr	0xf00d
	jsr	0xdeadf00d

	jsrtnz	r41,r42
	jsrtnz	r1,0xf00d
	jsrtnz	r1,0xdeadf00d

	jsrtzr	r41,r42
	jsrtzr	r1,0xf00d
	jsrtzr	r1,0xdeadf00d
					
	ld2h	r6,@(r7,r8)
	ld2h	r6,@(r7+,r8)
	ld2h	r6,@(r7-,r8)
	ld2h	r6,@(r7,0x1a)
	ld2h	r6,@(r7,0x1234)
	
	ld2w	r6,@(r7,r8)
	ld2w	r6,@(r7+,r8)
	ld2w	r6,@(r7-,r8)
	ld2w	r6,@(r7,0x1a)
	ld2w	r6,@(r7,0x1234)
	
	ld4bh	r6,@(r7,r8)
	ld4bh	r6,@(r7+,r8)
	ld4bh	r6,@(r7-,r8)
	ld4bh	r6,@(r7,0x1a)
	ld4bh	r6,@(r7,0x1234)

	ld4bhu	r6,@(r7,r8)
	ld4bhu	r6,@(r7+,r8)
	ld4bhu	r6,@(r7-,r8)
	ld4bhu	r6,@(r7,0x1a)
	ld4bhu	r6,@(r7,0x1234)

	ldb	r6,@(r7,r8)
	ldb	r6,@(r7+,r8)
	ldb	r6,@(r7-,r8)
	ldb	r6,@(r7,0x1a)
	ldb	r6,@(r7,0x1234)

	ldbu	r6,@(r7,r8)
	ldbu	r6,@(r7+,r8)
	ldbu	r6,@(r7-,r8)
	ldbu	r6,@(r7,0x1a)
	ldbu	r6,@(r7,0x1234)

	ldh	r6,@(r7,r8)
	ldh	r6,@(r7+,r8)
	ldh	r6,@(r7-,r8)
	ldh	r6,@(r7,0x1a)
	ldh	r6,@(r7,0x1234)

	ldhh	r6,@(r7,r8)
	ldhh	r6,@(r7+,r8)
	ldhh	r6,@(r7-,r8)
	ldhh	r6,@(r7,0x1a)
	ldhh	r6,@(r7,0x1234)

	ldhu	r6,@(r7,r8)
	ldhu	r6,@(r7+,r8)
	ldhu	r6,@(r7-,r8)
	ldhu	r6,@(r7,0x1a)
	ldhu	r6,@(r7,0x1234)

	ldw	r6,@(r7,r8)
	ldw	r6,@(r7+,r8)
	ldw	r6,@(r7-,r8)
	ldw	r6,@(r7,0x1a)
	ldw	r6,@(r7,0x1234)
							
	mac0	r1,r2,r4
	mac0	r1,r2,0x1f
	mac1	r1,r2,r4
	mac1	r1,r2,0x1f

	macs0	r1,r2,r4
	macs0	r1,r2,0x1f
	macs1	r1,r2,r4
	macs1	r1,r2,0x1f

	moddec	r1,0xa

	modinc	r1,0xa

	msub0	r1,r2,r4
	msub0	r1,r2,0x1f
	msub1	r1,r2,r4
	msub1	r1,r2,0x1f

	mul	r1,r2,r4
	mul	r1,r2,0xa

	msubs0	r1,r2,r4
	msubs0	r1,r2,0x1f
	msubs1	r1,r2,r4
	msubs1	r1,r2,0x1f

	mul2h	r1,r2,r4
	mul2h	r1,r2,0xa
	
	mulhxll	r1,r2,r4
	mulhxll	r1,r2,0xa

	mulhxlh	r1,r2,r4
	mulhxlh	r1,r2,0xa

	mulhxhl	r1,r2,r4
	mulhxhl	r1,r2,0xa

	mulhxhh	r1,r2,r4
	mulhxhh	r1,r2,0xa

	mulx2h	r8,r2,r4
	mulxs	a0,r1,r4
	
	mulx	a0,r1,r4
	mulx	a1,r2,0xa

	mvfacc	r1,a0,r4
	mvfacc	r2,a1,0xa

	mulx2h	r8,r2,0xa
	mulxs	a1,r2,0xa
		
	mvfsys	r10,pc
	mvfsys	r10,rpt_c
	mvfsys	r10,psw
	mvfsys	r10,pswh
	mvfsys	r10,pswl
	mvfsys	r10,f0
	mvfsys	r10,S

	mvtacc	a1,r2,r4

	mvtsys	rpt_c, r10
	mvtsys	psw, r10
	mvtsys	pswh, r10
	mvtsys	pswl, r10
	mvtsys	f0, r10
	mvtsys	f3, r10
	mvtsys	S, r10
	mvtsys	V, r10
	mvtsys	VA, r10
	mvtsys	C, r10

	nop

	not	r1,r2

	notfg	f1,f2

	or	r1,r2,r4
	or	r1,r2,0x1a
	or	r1,r2,0xdeadf00d

	orfg	f1,f2,f4
	orfg	f4,f2,0x1
	
	reit

	repeat	r1,r2
	repeat	r4,0xdead
	repeat	r4,0xdeadf00d

	repeati	0xa,r1
	repeati	0xa,0x1001

	nop || nop
	
	rot	r1,r2,r4
	rot	r1,r2,0xa

	rot2h	r1,r2,r4
	rot2h	r1,r2,0xa

	sat	r1,r2,r4
	sat	r1,r2,0xa

	sat2h	r1,r2,r4
	sat2h	r1,r2,0xa

	sathl	r1,r2,r4
	sathl	r1,r2,0xa

	sathh	r1,r2,r4
	sathh	r1,r2,0xa
		
	satz	r1,r2,r4
	satz	r1,r2,0xa

	satz2h	r1,r2,r4
	satz2h	r1,r2,0xa
			
	sra	r1,r2,r4
	sra	r1,r2,0xa

	sra2h	r1,r2,r4
	sra2h	r1,r2,0xa

	src	r1,r2,r4
	src	r1,r2,0xa
	
	srl	r1,r2,r4
	srl	r1,r2,0xa

	srl2h	r1,r2,r4
	srl2h	r1,r2,0xa

	
	st2h	r6,@(r7,r8)
	st2h	r6,@(r7+,r8)
	st2h	r6,@(r7-,r8)
	st2h	r6,@(r7,0x1a)
	st2h	r6,@(r7,0x1234)
		
	st2w	r6,@(r7,r8)
	st2w	r6,@(r7+,r8)
	st2w	r6,@(r7-,r8)
	st2w	r6,@(r7,0x1a)
	st2w	r6,@(r7,0x1234)

	st4hb	r6,@(r7,r8)
	st4hb	r6,@(r7+,r8)
	st4hb	r6,@(r7-,r8)
	st4hb	r6,@(r7,0x1a)
	st4hb	r6,@(r7,0x1234)

	stb	r6,@(r7,r8)
	stb	r6,@(r7+,r8)
	stb	r6,@(r7-,r8)
	stb	r6,@(r7,0x1a)
	stb	r6,@(r7,0x1234)

	sth	r6,@(r7,r8)
	sth	r6,@(r7+,r8)
	sth	r6,@(r7-,r8)
	sth	r6,@(r7,0x1a)
	sth	r6,@(r7,0x1234)

	sthh	r6,@(r7,r8)
	sthh	r6,@(r7+,r8)
	sthh	r6,@(r7-,r8)
	sthh	r6,@(r7,0x1a)
	sthh	r6,@(r7,0x1234)

	stw	r6,@(r7,r8)
	stw	r6,@(r7+,r8)
	stw	r6,@(r7-,r8)
	stw	r6,@(r7,0x1a)
	stw	r6,@(r7,0x1234)
								
	sub	r1,r2,r3
	sub	r50,r51,0x1a
	sub	r50,r51,0xdeadbeef

	sub2h	r1,r2,r3
	sub2h	r50,r51,0x1a
	sub2h	r50,r51,0xdeadbeef

	subb	r1,r2,r3
	subb	r50,r51,0x1a
	subb	r50,r51,0xdeadbeef

	subhlll	r1,r2,r3
	subhlll	r50,r51,0x1a
	subhlll	r50,r51,0xdeadbeef

	subhllh	r1,r2,r3
	subhllh	r50,r51,0x1a
	subhllh	r50,r51,0xdeadbeef

	subhlhl	r1,r2,r3
	subhlhl	r50,r51,0x1a
	subhlhl	r50,r51,0xdeadbeef

	subhlhh	r1,r2,r3
	subhlhh	r50,r51,0x1a
	subhlhh	r50,r51,0xdeadbeef
		
	subhhll	r1,r2,r3
	subhhll	r50,r51,0x1a
	subhhll	r50,r51,0xdeadbeef

	subhhlh	r1,r2,r3
	subhhlh	r50,r51,0x1a
	subhhlh	r50,r51,0xdeadbeef

	subhhhl	r1,r2,r3
	subhhhl	r50,r51,0x1a
	subhhhl	r50,r51,0xdeadbeef

	subhhhh	r1,r2,r3
	subhhhh	r50,r51,0x1a
	subhhhh	r50,r51,0xdeadbeef
	
	trap	r1
	trap	0xa

	xor	r1,r2,r4
	xor	r1,r2,0xa
	xor	r1,r2,0xdeadf00d

	xorfg	f1,f2,f4
	xorfg	f1,f4,0xa

# VLIW syntax test
	nop
	nop
	nop	->	nop
	nop	||	nop
	nop	<-	nop

# try changing sections
	not	r1,r2
	.section .foo
	add	r10,r12,6
	.text
	not	r2,r3
	nop
	