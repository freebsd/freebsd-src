        .text
        .global main
main:
        ###########
	# CMPB imm4/imm16, reg
        ###########
	cmpb    $0xf,r1
	cmpb    $0xff,r2
	cmpb    $0xfff,r1
	#cmpb    $0xffff,r2 // CHCEFK WITH CRASM 4.1
	cmpb    $20,r1
	cmpb    $10,r2
	cmpb    $11,r2
        ###########
	# CMPB reg, reg
        ###########
	cmpb    r1,r2
	cmpb    r2,r3
	cmpb    r3,r4
	cmpb    r5,r6
	cmpb    r6,r7
	cmpb    r7,r8
        ###########
	# CMPW imm4/imm16, reg
        ###########
	cmpw    $0xf,r1
	cmpw    $0xB,r1
	cmpw    $0xff,r2
	cmpw    $0xfff,r1
	#cmpw    $0xffff,r2 // CHECK WITH CRASM 4.1
	cmpw    $20,r1
	cmpw    $10,r2
	cmpw    $11,r2
        ###########
	# CMPW reg, reg
        ###########
	cmpw    r1,r2
	cmpw    r2,r3
	cmpw    r3,r4
	cmpw    r5,r6
	cmpw    r6,r7
	cmpw    r7,r8
        ###########
	# CMPD imm4/imm16/imm32, regp
        ###########
	cmpd    $0xf,(r2,r1)
	cmpd    $0xB,(r2,r1)
	cmpd    $0xff,(r2,r1)
	cmpd    $0xfff,(r2,r1)
	cmpd    $0xffff,(r2,r1)
	cmpd    $0xfffff,(r2,r1)
	cmpd    $0xfffffff,(r2,r1)
	cmpd    $0xffffffff,(r2,r1)
        ###########
	# CMPD regp, regp
        ###########
	cmpd    (r4,r3),(r2,r1)
	cmpd    (r4,r3),(r2,r1)
	cmpd    $10,(sp)
	cmpd    $14,(sp)
	cmpd    $11,(sp)
	cmpd    $8,(sp)
