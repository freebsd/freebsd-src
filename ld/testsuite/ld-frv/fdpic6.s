	.text
	.weak _start
_start:
	.weak WF0
	.weak WF1
	.weak WF2
	.weak WF3
	.weak WF4
	.weak WF5
	.weak WF6
	.weak WF7
	.weak WF8
	.weak WF9
	.weak WFb
	.weak WD0
	.weak WD1
	.weak WD2
	.global F6
	.type F6,@function
F6:
	call	WF0
	addi	gr15, #got12(WF1), gr0
	
	setlos	#gotlo(WF2), gr0
	
	setlo	#gotlo(WF3), gr0
	sethi	#gothi(WF3), gr0

	addi	gr15, #gotfuncdesc12(WF4), gr0

	setlos	#gotfuncdesclo(WF5), gr0

	setlo	#gotfuncdesclo(WF6), gr0
	sethi	#gotfuncdeschi(WF6), gr0

	addi	gr15, #gotofffuncdesc12(WF7), gr0

	setlos	#gotofffuncdesclo(WF8), gr0

	setlo	#gotofffuncdesclo(WF9), gr0
	sethi	#gotofffuncdeschi(WF9), gr0

	setlo	#gotofflo(WD1), gr0
	sethi	#gotoffhi(WD1), gr0

	setlo	#gotlo(WD2), gr0
	sethi	#gothi(WD2), gr0

	.data
	.global D6
D6:
	.word	WD0
	
	.picptr funcdesc(WFb)
	.word	WFb
