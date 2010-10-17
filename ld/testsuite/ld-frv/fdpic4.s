	.text
	.weak _start
_start:
	.global F4
	.type F4,@function
F4:
	call	PF0
	.global PF0
	.protected PF0
	.global PF1
	.protected PF1
	.global PF2
	.protected PF2
	.global PF3
	.protected PF3
	.global PF4
	.protected PF4
	.global PF5
	.protected PF5
	.global PF6
	.protected PF6
	.global PF7
	.protected PF7
	.global PF8
	.protected PF8
	.global PF9
	.protected PF9
	.global PFa
	.protected PFa
	.global PFb
	.protected PFb
	.global PFc
	.protected PFc
PF0:
PF1:
PF2:
PF3:
PF4:
PF5:
PF6:
PF7:
PF8:
PF9:
PFa:
PFb:
PFc:
	addi	gr15, #got12(PF1), gr0
	
	setlos	#gotlo(PF2), gr0
	
	setlo	#gotlo(PF3), gr0
	sethi	#gothi(PF3), gr0

	addi	gr15, #gotfuncdesc12(PF4), gr0

	setlos	#gotfuncdesclo(PF5), gr0

	setlo	#gotfuncdesclo(PF6), gr0
	sethi	#gotfuncdeschi(PF6), gr0

	addi	gr15, #gotofffuncdesc12(PF7), gr0

	setlos	#gotofffuncdesclo(PF8), gr0

	setlo	#gotofffuncdesclo(PF9), gr0
	sethi	#gotofffuncdeschi(PF9), gr0

	addi	gr15, #gotoff12(PD1), gr0
	
	setlos	#gotofflo(PD2), gr0

	setlo	#gotofflo(PD3), gr0
	sethi	#gotoffhi(PD3), gr0

	setlo	#gotlo(PD4), gr0
	sethi	#gothi(PD4), gr0

	.data
	.global D4
D4:
	.word	PD0
	
	.global PD0
	.protected PD0
	.global PD1
	.protected PD1
	.global PD2
	.protected PD2
	.global PD3
	.protected PD3
	.global PD4
	.protected PD4
PD0:
PD1:
PD2:
PD3:
PD4:
	.picptr funcdesc(PFb)
	.word	PFb
