        .text
        .global main
main:
        ###########
	# MULB imm4/imm16, reg
        ###########
	mulb    $0xf,r1
	mulb    $0xff,r2
	mulb    $0xfff,r1
	#mulb    $0xffff,r2 // CHCEK WITH CRASM 4.1
	mulb    $20,r1
	mulb    $10,r2
        ###########
	# MULB reg, reg
        ###########
	mulb    r1,r2
	mulb    r2,r3
	mulb    r3,r4
	mulb    r5,r6
	mulb    r6,r7
	mulb    r7,r8
        ###########
	# MULW imm4/imm16, reg
        ###########
	mulw    $0xf,r1
	mulw    $0xff,r2
	mulw    $0xfff,r1
	#mulw    $0xffff,r2 // CHCEK WITH CRASM 4.1
	mulw    $20,r1
	mulw    $10,r2
        ###########
	# MULW reg, reg
        ###########
	mulw    r1,r2
	mulw    r2,r3
	mulw    r3,r4
	mulw    r5,r6
	mulw    r6,r7
	mulw    r7,r8
        ###########
	# MULSB reg, reg
        ###########
	mulsb	r1,r2
	mulsb	r3,r4
	mulsb	r5,r6
	mulsb	r7,r8
	mulsb	r9,r10
        ###########
	# MULSW reg, regp
        ###########
	mulsw	r1,(r3,r2)
	mulsw	r3,(r4,r3)
	mulsw	r5,(r6,r5)
	mulsw	r7,(r8,r7)
	mulsw	r9,(r9,r8)
        #############################
	# MUC[q/u/s/]w reg, reg, regp
        #############################
	macqw   r1,r2,(r3,r2)
	macqw   r4,r5,(r5,r4)
	macuw   r1,r2,(r3,r2)
	macuw   r4,r5,(r8,r7)
	macsw   r1,r2,(r3,r2)
	macsw   r4,r5,(r7,r6)
