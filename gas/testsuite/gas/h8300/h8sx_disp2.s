	.h8300sx
	add.b	#2,@(1,er0)	; L_2
	add.b	#2,@(2,er0)	; L_2
	add.b	#2,@(3,er0)	; L_2
	add.b	#2,@(4,er0)	; L_16
	add.b	#2,@(foo+1,er0)	; L_32

	add.w	#2,@(1,er0)	; L_16
	add.w	#2,@(2,er0)	; L_2
	add.w	#2,@(4,er0)	; L_2
	add.w	#2,@(6,er0)	; L_2
	add.w	#2,@(8,er0)	; L_16
	add.w	#2,@(foo+2,er0)	; L_32

	add.l	#2,@(1,er0)	; L_16
	add.l	#2,@(2,er0)	; L_16
	add.l	#2,@(4,er0)	; L_2
	add.l	#2,@(8,er0)	; L_2
	add.l	#2,@(12,er0)	; L_2
	add.l	#2,@(16,er0)	; L_16
	add.l	#2,@(foo+4,er0)	; L_32
