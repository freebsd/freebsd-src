        .text
        .global main
main:
        ###########
	# ADDB imm4/imm16, reg
        ###########
	addb    $0xf,r1
	addb    $0xff,r2
	addb    $0xfff,r1
	#addb    $0xffff,r2 // CHECK WITH CRASM 4.1
	addb    $20,r1
	addb    $10,r2
	addb    $11,r2
        ###########
	# ADDB reg, reg
        ###########
	addb    r1,r2
	addb    r2,r3
	addb    r3,r4
	addb    r5,r6
	addb    r6,r7
	addb    r7,r8
        ###########
	# ADDCB imm4/imm16, reg
        ###########
	addcb    $0xf,r1
	addcb    $0xff,r2
	addcb    $0xfff,r1
	#addcb    $0xffff,r2 // CHECK WITH CRASM 4.1
	addcb    $20,r1
	addcb    $10,r2
	addcb    $11,r2
        ###########
	# ADDCB reg, reg
        ###########
	addcb    r1,r2
	addcb    r2,r3
	addcb    r3,r4
	addcb    r5,r6
	addcb    r6,r7
	addcb    r7,r8
        ###########
	# ADDCW imm4/imm16, reg
        ###########
	addcw    $0xf,r1
	addcw    $0xff,r2
	addcw    $0xfff,r1
	#addcw    $0xffff,r2 # check with CRASM 4.1
	addcw    $20,r1
	addcw    $10,r2
	addcw    $11,r2
        ###########
	# ADDCW reg, reg
        ###########
	addcw    r1,r2
	addcw    r2,r3
	addcw    r3,r4
	addcw    r5,r6
	addcw    r6,r7
	addcw    r7,r8
        ###########
	# ADDW imm4/imm16, reg
        ###########
	addw    $0xf,r1
	addw    $0xff,r2
	addw    $0xfff,r1
	#addw    $0xffff,r2 // CHECK WITH CRASM 4.1
	addw    $20,r1
	addw    $10,r2
        ###########
	# ADDW reg, reg
        ###########
	addw    r1,r2
	addw    r2,r3
	addw    r3,r4
	addw    r5,r6
	addw    r6,r7
	addw    r7,r8
        ###########
	# ADDD imm4/imm16/imm20/imm32, regp
        ###########
	addd    $0xf,(r2,r1)
	addd    $0xB,(r2,r1)
	addd    $0xff,(r2,r1)
	addd    $0xfff,(r2,r1)
	addd    $0xffff,(r2,r1)
	addd    $0xfffff,(r2,r1)
	addd    $0xfffffff,(r2,r1)
	addd    $0xffffffff,(r2,r1)
        ###########
	# ADDD regp, regp
        ###########
	addd    (r4,r3),(r2,r1)
	addd    (r4,r3),(r2,r1)
	addd    $10,(sp)
	addd    $14,(sp)
	addd    $11,(sp)
	addd    $8,(sp)
