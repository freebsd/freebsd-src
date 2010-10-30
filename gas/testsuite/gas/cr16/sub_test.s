        .text
        .global main
main:
        ###########
	# SUBB imm4/imm16, reg
        ###########
	subb    $0xf,r1
	subb    $0xff,r2
	subb    $0xfff,r1
	#subb    $0xffff,r2  // CHECK WITH CRASM 4.1
	subb    $20,r1
	subb    $10,r2
        ###########
	# SUBB reg, reg
        ###########
	subb    r1,r2
	subb    r2,r3
	subb    r3,r4
	subb    r5,r6
	subb    r6,r7
	subb    r7,r8
        ###########
	# SUBCB imm4/imm16, reg
        ###########
	subcb    $0xf,r1
	subcb    $0xff,r2
	subcb    $0xfff,r1
	#subcb    $0xffff,r2   // CHECK WITH CRASM 4.1
	subcb    $20,r1
	subcb    $10,r2
        ###########
	# SUBCB reg, reg
        ###########
	subcb    r1,r2
	subcb    r2,r3
	subcb    r3,r4
	subcb    r5,r6
	subcb    r6,r7
	subcb    r7,r8
        ###########
	# SUBCW imm4/imm16, reg
        ###########
	subcw    $0xf,r1
	subcw    $0xff,r2
	subcw    $0xfff,r1
	#subcw    $0xffff,r2  // CHECK WITH CRASM 4.1
	subcw    $20,r1
	subcw    $10,r2
        ###########
	# SUBCW reg, reg
        ###########
	subcw    r1,r2
	subcw    r2,r3
	subcw    r3,r4
	subcw    r5,r6
	subcw    r6,r7
	subcw    r7,r8
        ###########
	# SUBW imm4/imm16, reg
        ###########
	subw    $0xf,r1
	subw    $0xff,r2
	subw    $0xfff,r1
	#subw    $0xffff,r2  // CHECK WITH CRASM 4.1
	subw    $20,r1
	subw    $10,r2
        ###########
	# SUBW reg, reg
        ###########
	subw    r1,r2
	subw    r2,r3
	subw    r3,r4
	subw    r5,r6
	subw    r6,r7
	subw    r7,r8
        ###########
	# SUBD imm4/imm16/imm32, regp
        ###########
	subd    $0xf,(r2,r1)
	subd    $0xff,(r2,r1)
	subd    $0xfff,(r2,r1)
	subd    $0xffff,(r2,r1)
	subd    $0xfffff,(r2,r1)
	subd    $0xfffffff,(r2,r1)
	subd    $0xffffffff,(r2,r1)
        ###########
	# SUBD regp, regp
        ###########
	subd    (r4,r3),(r2,r1)
	subd    (r4,r3),(r2,r1)
	#subd    $10,(sp)
	#subd    $14,(sp)
	#subd    $8,(sp)
