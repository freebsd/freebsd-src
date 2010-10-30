        .text
        .global main
main:
        ###########
	# XORB imm4/imm16, reg
        ###########
	xorb    $0xf,r1
	xorb    $0xff,r2
	xorb    $0xfff,r1
	xorb    $0xffff,r2
	xorb    $20,r1
	xorb    $10,r2
        ###########
	# XORB reg, reg
        ###########
	xorb    r1,r2
	xorb    r2,r3
	xorb    r3,r4
	xorb    r5,r6
	xorb    r6,r7
	xorb    r7,r8
        ###########
	# XORW imm4/imm16, reg
        ###########
	xorw    $0xf,r1
	xorw    $0xff,r2
	xorw    $0xfff,r1
	xorw    $0xffff,r2
	xorw    $20,r1
	xorw    $10,r2
        ###########
	# XORW reg, reg
        ###########
	xorw    r1,r2
	xorw    r2,r3
	xorw    r3,r4
	xorw    r5,r6
	xorw    r6,r7
	xorw    r7,r8
        ###########
	# XORD imm32, regp
        ###########
	xord    $0xf,(r2,r1)
	xord    $0xff,(r2,r1)
	xord    $0xfff,(r2,r1)
	xord    $0xffff,(r2,r1)
	xord    $0xfffff,(r2,r1)
	xord    $0xfffffff,(r2,r1)
	xord    $0xffffffff,(r2,r1)
        ###########
	# XORD regp, regp
        ###########
	xord    (r4,r3),(r2,r1)
	xord    (r4,r3),(r2,r1)
	#xord    $10,(sp)
	#xord    $14,(sp)
	#xord    $8,(sp)
