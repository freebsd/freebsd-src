	.text
	.weak _start
_start:
	.global F5
	.type F5,@function
F5:
	call	UF0
	addi	gr15, #got12(UF1), gr0
	
	setlos	#gotlo(UF2), gr0
	
	setlo	#gotlo(UF3), gr0
	sethi	#gothi(UF3), gr0

	addi	gr15, #gotfuncdesc12(UF4), gr0

	setlos	#gotfuncdesclo(UF5), gr0

	setlo	#gotfuncdesclo(UF6), gr0
	sethi	#gotfuncdeschi(UF6), gr0

	addi	gr15, #gotofffuncdesc12(UF7), gr0

	setlos	#gotofffuncdesclo(UF8), gr0

	setlo	#gotofffuncdesclo(UF9), gr0
	sethi	#gotofffuncdeschi(UF9), gr0

	setlo	#gotlo(UD1), gr0
	sethi	#gothi(UD1), gr0

	.data
	.global D5
D5:
	.word	UD0
	
	.picptr funcdesc(UFb)
	.word	UFb
