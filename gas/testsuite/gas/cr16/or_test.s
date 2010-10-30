        .text
        .global main
main:
        ###########
	# ORB imm4/imm16, reg
        ###########
	orb    $0xf,r1
	orb    $0xff,r2
	orb    $0xfff,r1
	orb    $0xffff,r2
	orb    $20,r1
	orb    $10,r2
        ###########
	# ORB reg, reg
        ###########
	orb    r1,r2
	orb    r2,r3
	orb    r3,r4
	orb    r5,r6
	orb    r6,r7
	orb    r7,r8
        ###########
	# ORW imm4/imm16, reg
        ###########
	orw    $0xf,r1
	orw    $0xff,r2
	orw    $0xfff,r1
	orw    $0xffff,r2
	orw    $20,r1
	orw    $10,r2
        ###########
	# ORW reg, reg
        ###########
	orw    r1,r2
	orw    r2,r3
	orw    r3,r4
	orw    r5,r6
	orw    r6,r7
	orw    r7,r8
        ###########
	# ORD imm32, regp
        ###########
	ord    $0xf,(r2,r1)
	ord    $0xff,(r2,r1)
	ord    $0xfff,(r2,r1)
	ord    $0xffff,(r2,r1)
	ord    $0xfffff,(r2,r1)
	ord    $0xfffffff,(r2,r1)
	ord    $0xffffffff,(r2,r1)
        ###########
	# ORD regp, regp
        ###########
	ord    (r4,r3),(r2,r1)
	ord    (r4,r3),(r2,r1)
	#ord    $10,(sp)
	#ord    $14,(sp)
	#ord    $8,(sp)
