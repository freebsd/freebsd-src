	.text
	.weak _start
_start:
	.global F3
	.type F3,@function
F3:
	call	HF0
	.global HF0
	.hidden HF0
	.global HF1
	.hidden HF1
	.global HF2
	.hidden HF2
	.global HF3
	.hidden HF3
	.global HF4
	.hidden HF4
	.global HF5
	.hidden HF5
	.global HF6
	.hidden HF6
	.global HF7
	.hidden HF7
	.global HF8
	.hidden HF8
	.global HF9
	.hidden HF9
	.global HFa
	.hidden HFa
	.global HFb
	.hidden HFb
	.global HFc
	.hidden HFc
HF0:
HF1:
HF2:
HF3:
HF4:
HF5:
HF6:
HF7:
HF8:
HF9:
HFa:
HFb:
HFc:
	addi	gr15, #got12(HF1), gr0
	
	setlos	#gotlo(HF2), gr0
	
	setlo	#gotlo(HF3), gr0
	sethi	#gothi(HF3), gr0

	addi	gr15, #gotfuncdesc12(HF4), gr0

	setlos	#gotfuncdesclo(HF5), gr0

	setlo	#gotfuncdesclo(HF6), gr0
	sethi	#gotfuncdeschi(HF6), gr0

	addi	gr15, #gotofffuncdesc12(HF7), gr0

	setlos	#gotofffuncdesclo(HF8), gr0

	setlo	#gotofffuncdesclo(HF9), gr0
	sethi	#gotofffuncdeschi(HF9), gr0

	addi	gr15, #gotoff12(HD1), gr0
	
	setlos	#gotofflo(HD2), gr0

	setlo	#gotofflo(HD3), gr0
	sethi	#gotoffhi(HD3), gr0

	setlo	#gotlo(HD4), gr0
	sethi	#gothi(HD4), gr0

	.data
	.global D3
D3:
	.word	HD0
	
	.global HD0
	.hidden HD0
	.global HD1
	.hidden HD1
	.global HD2
	.hidden HD2
	.global HD3
	.hidden HD3
	.global HD4
	.hidden HD4
HD0:
HD1:
HD2:
HD3:
HD4:
	.picptr funcdesc(HFb)
	.word	HFb
