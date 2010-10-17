# D30V parallel optimization test
# assemble with "-O"
	
	.text
start:	
	abs	r1,r2
	abs	r3,r4

	notfg	f0,f4
	notfg	f1,f2
		
	abs	r1,r2
	notfg	f1,f2

# both change C flag
	add	r1,r2,r3
	notfg	C,f0

# one uses and one changes C flag	
	add	r1,r2,r3
	notfg	f0,C

	bra	.
	abs	r1,r2

	abs	r1,r2	
	bra	.

	bsr	.
	abs	r1,r2
	
	abs	r1,r2	
	abs	r1,r2	
	bsr	.
	
	ldb	r1,@(r2,r3)
	stb	r7,@(r8,r9)

	stb	r7,@(r8,r9)	
	ldb	r1,@(r2,r3)

	ldb	r7,@(r8,r9)	
	ldb	r1,@(r2,r3)

	stb	r7,@(r8,r9)	
	stb	r1,@(r2,r3)

	add     r3, r3, r6
	stw     r2, @(r3, 0)
			
# should be serial because of conditional execution
        cmple   f0,r4,r5
        jmp/tx  0x0

        cmple   f0,r4,r5
        jmp/fx  0x0

        cmple   f0,r4,r5
        jmp/xt  0x0

        cmple   f0,r4,r5
        jmp/xf  0x0

        cmple   f0,r4,r5
        jmp/tt  0x0

        cmple   f0,r4,r5
        jmp/tf  0x0
					
        cmple   f1,r4,r5
        jmp/tx  0x0

        cmple   f1,r4,r5
        jmp/xt  0x0

	# serial because of the r4 dependency
	add	r4, r0, 1
	cmple	f0, r4, r5

	# parallel
	add	r4, r0, 1
	cmple	f0, r3, r5

	# serial because ld2w loads r5
	ld2w	r4,@(r0,r6)
	adds	r5,r19,r20

	# serial because ld2w loads r5
	ld2w	r4,@(r0,r6)
	adds	r3,r5,r20

	# parallel even though ld2w uses r6 and adds changes it
	ld2w	r4,@(r0,r6)
	adds	r6,r19,r20

	# parallel
	ld2w	r4,@(r0,r6)
	adds	r7,r19,r20

	# parallel
	ld2w	r4,@(r0,r6)
	adds	r7,r0,r20

	# parallel even though st2w uses r5 and adds modifies it
	st2w	r4,@(r0,r6)
	adds	r5,r19,r20

	# parallel, both use but don't modify r5
	st2w	r4,@(r0,r6)
	adds	r3,r5,r20

	# parallel even though st2w uses r6 and adds changes it
	st2w	r4,@(r0,r6)
	adds	r6,r19,r20

	# parallel
	st2w	r4,@(r0,r6)
	adds	r7,r19,r20

	# parallel
	st2w	r4,@(r0,r6)
	adds	r7,r0,r20
						
# test memory dependencies

	# always serial because one could overwrite the other
	st2w	r10,@(r3,r4)
	st2w	r40,@(r43,r44)

	# always serial
	stw	r1,@(r2,r3)
	ldw	r41,@(r42,r43)			

	# reads can happen in parallel but the current architecture 
	# doesn't support it
	ldw	r1,@(r2,r3)
	ldb	r41,@(r42,r43)			

# test post increment and decrement dependencies

	# serial
	ldw	r4,@(r6+,r11)
	adds	r9,r6,2

	# parallel, modification to r6 happens last
	adds	r9,r6,2	
	ldw	r4,@(r6-,r11)

	# serial
	stw	r4,@(r6-,r11)
	adds	r9,r6,2
	
	# parallel
	ldw	r4,@(r6,r11)
	adds	r9,r6,2

	# parallel
	adds	r9,r6,2	
	ldw	r4,@(r6,r11)
	
# if the first instruction is a jmp, don't parallelize
	jmp	0
	abs	r1,r2	

	jsr	0
	abs	r1,r2	

	.align	3
	
	bra	0
	abs	r1,r2	
	
	bsr	0
	abs	r1,r2	

# Explicitly prohibited from parallel execution.
# The labels are here to prevent instruction pairs
#  from being merged with following pairs.
	
label1:	
	st2w     r2, @(r2, r3)
	addhlll  r4, r5, r6
label2:
	st4hb    r8, @(r8, r9)
	subhllh  r10, r11, r12
label3:
	ld2w     r14, @(r14, r15)
	mulhxhl  r16, r17, r18
label4:
	ldw      r19, @(r20, r21)
	mulx2h   r22, r23, r24
label5:
	ldh      r25, @(r26, r27)
	mul2h    r28, r29, r30

# Insertion of NOPs required to prevent pipeline clashes.

label6:
	mul r1,r2,r3
	mulhxll r4,r5,r6
        add r7, r8, r9
label7:
	
        mul  r2,r3,r4
        ldw  r5, @(r6,r0)
 
        ldw  r10, @(r11, r0) <- mul r7,r8,r9
	
        mul  r12,r13,r14 -> ldw r15, @(r16, r0)

        mac1 r2,r3,r4
        ldw  r5, @(r6,r0)
 
        ldw  r10, @(r11, r0) <- mac0 r7,r8,r9
        ldw  r10, @(r11, r0) 
	
