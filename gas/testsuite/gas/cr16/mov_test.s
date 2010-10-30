        .text
        .global main
main:
        ###########
	# MOVB imm4/imm16, reg
        ###########
	movb    $0xf,r1
	movb    $0xff,r2
	movb    $0xfff,r1
	#movb    $0xffff,r2 // CHECK WITH CRASM 4.1
	movb    $20,r1
	movb    $10,r2
	movb    $11,r2
        ###########
	# MOVB reg, reg
        ###########
	movb    r1,r2
	movb    r2,r3
	movb    r3,r4
	movb    r5,r6
	movb    r6,r7
	movb    r7,r8
        ###########
	# MOVW imm4/imm16, reg
        ###########
	movw    $0xf,r1
	movw    $0xB,r1
	movw    $0xff,r2
	movw    $0xfff,r1
	#movw    $0xffff,r2 // CHECK WITH CRASM 4.1
	movw    $20,r1
	movw    $10,r2
	movw    $11,r2
        ###########
	# MOVW reg, reg
        ###########
	movw    r1,r2
	movw    r2,r3
	movw    r3,r4
	movw    r5,r6
	movw    r6,r7
	movw    r7,r8
        ###########
	# MOVD imm4/imm16/imm20/imm32, regp
        ###########
	movd    $0xf,(r2,r1)
	movd    $0xB,(r2,r1)
	movd    $0xff,(r2,r1)
	movd    $0xfff,(r2,r1)
	movd    $0xffff,(r2,r1)
	movd    $0xfffff,(r2,r1)
	movd    $0xfffffff,(r2,r1)
	movd    $0xffffffff,(r2,r1)
        ###########
	# MOVD regp, regp
        ###########
	movd    (r4,r3),(r2,r1)
	movd    (r4,r3),(r2,r1)
	movd    $10,(sp)
	movd    $14,(sp)
	movd    $11,(sp)
	movd    $8,(sp)
        ###########
	# MOVXB reg, reg
        ###########
	movxb	r1,r2
	movxb	r3,r4
	movxb	r5,r6
	movxb	r7,r8
	movxb	r9,r10
        ###########
	# MOVXW reg, regp
        ###########
	movxw	r1,(r3,r2)
	movxw	r3,(r4,r3)
	movxw	r5,(r6,r5)
	movxw	r7,(r8,r7)
	movxw	r9,(r9,r8)
        ###########
	# MOVZB reg, reg
        ###########
	movzb	r1,r2
	movzb	r3,r4
	movzb	r5,r6
	movzb	r7,r8
	movzb	r9,r10
        ###########
	# MOVZW reg, regp
        ###########
	movzw	r1,(r3,r2)
	movzw	r3,(r4,r3)
	movzw	r5,(r6,r5)
	movzw	r7,(r8,r7)
	movzw	r9,(r9,r8)
