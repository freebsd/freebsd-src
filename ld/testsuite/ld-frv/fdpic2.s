	.text
	.weak _start
_start:
	.global F2
	.type F2,@function
F2:
	call	GF0
	.global GF0
	.global GF1
	.global GF2
	.global GF3
	.global GF4
	.global GF5
	.global GF6
	.global GF7
	.global GF8
	.global GF9
	.global GFa
	.global GFb
	.global GFc
GF0:
GF1:
GF2:
GF3:
GF4:
GF5:
GF6:
GF7:
GF8:
GF9:
GFa:
GFb:
GFc:
	addi	gr15, #got12(GF1), gr0
	
	setlos	#gotlo(GF2), gr0
	
	setlo	#gotlo(GF3), gr0
	sethi	#gothi(GF3), gr0

	addi	gr15, #gotfuncdesc12(GF4), gr0

	setlos	#gotfuncdesclo(GF5), gr0

	setlo	#gotfuncdesclo(GF6), gr0
	sethi	#gotfuncdeschi(GF6), gr0

	addi	gr15, #gotofffuncdesc12(GF7), gr0

	setlos	#gotofffuncdesclo(GF8), gr0

	setlo	#gotofffuncdesclo(GF9), gr0
	sethi	#gotofffuncdeschi(GF9), gr0

	addi	gr15, #gotoff12(GD1), gr0
	
	setlos	#gotofflo(GD2), gr0

	setlo	#gotofflo(GD3), gr0
	sethi	#gotoffhi(GD3), gr0

	setlo	#gotlo(GD4), gr0
	sethi	#gothi(GD4), gr0

	.data
	.global D2
D2:
	.word	GD0
	
	.global GD0
	.global GD1
	.global GD2
	.global GD3
	.global GD4
GD0:
GD1:
GD2:
GD3:
GD4:
	.picptr funcdesc(GFb)
	.word	GFb
