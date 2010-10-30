        .text
        .global main
main:
        ###########
	# ANDB imm4/imm16, reg
        ###########
	andb    $0xf,r1
	andb    $0xff,r2
	andb    $0xfff,r1
	andb    $0xffff,r2
	andb    $20,r1
	andb    $10,r2
        ###########
	# ANDB reg, reg
        ###########
	andb    r1,r2
	andb    r2,r3
	andb    r3,r4
	andb    r5,r6
	andb    r6,r7
	andb    r7,r8
        ###########
	# ANDW imm4/imm16, reg
        ###########
	andw    $0xf,r1
	andw    $0xff,r2
	andw    $0xfff,r1
	andw    $0xffff,r2
	andw    $20,r1
	andw    $10,r2
        ###########
	# ANDW reg, reg
        ###########
	andw    r1,r2
	andw    r2,r3
	andw    r3,r4
	andw    r5,r6
	andw    r6,r7
	andw    r7,r8
        ###########
	# ANDD imm4/imm16/imm32, regp
        ###########
	andd    $0xf,(r2,r1)
	andd    $0xff,(r2,r1)
	andd    $0xfff,(r2,r1)
	andd    $0xffff,(r2,r1)
	andd    $0xfffff,(r2,r1)
	andd    $0xfffffff,(r2,r1)
	andd    $0xffffffff,(r2,r1)
        ###########
	# ANDD regp, regp
        ###########
	andd    (r4,r3),(r2,r1)
	andd    (r4,r3),(r2,r1)
	andd    $10,(sp)
	andd    $14,(sp)
	andd    $8,(sp)
