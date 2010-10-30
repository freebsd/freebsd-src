        .text
        .global main
main:
	###################
	# beq0b reg, dispu5
	###################
	beq0b	r1,*+16
	beq0b	r1,*+24
	beq0b	r1,*+30
	###################
	# beq0w reg, dispu5
	###################
	beq0w	r1,*+16
	beq0w	r1,*+24
	beq0w	r1,*+30
