	.text
	.weak _start
_start:
	.global F1
	.type F1,@function
F1:
	call	.F0

.F0:
.F1:
.F2:
.F3:
.F4:
.F5:
.F6:
.F7:
.F8:
.F9:
.Fa:
.Fb:
.Fc:
	addi	gr15, #got12(.F1), gr0
	
	setlos	#gotlo(.F2), gr0
	
	setlo	#gotlo(.F3), gr0
	sethi	#gothi(.F3), gr0

	addi	gr15, #gotfuncdesc12(.F4), gr0

	setlos	#gotfuncdesclo(.F5), gr0

	setlo	#gotfuncdesclo(.F6), gr0
	sethi	#gotfuncdeschi(.F6), gr0

	addi	gr15, #gotofffuncdesc12(.F7), gr0

	setlos	#gotofffuncdesclo(.F8), gr0

	setlo	#gotofffuncdesclo(.F9), gr0
	sethi	#gotofffuncdeschi(.F9), gr0

	addi	gr15, #gotoff12(.D1), gr0
	
	setlos	#gotofflo(.D2), gr0

	setlo	#gotofflo(.D3), gr0
	sethi	#gotoffhi(.D3), gr0

	setlo	#gotlo(.D4), gr0
	sethi	#gothi(.D4), gr0

	.data
	.global D1
D1:
	.word	.D0
	.section .data.rel.local
.D0:
.D1:
.D2:
.D3:
.D4:
	.picptr funcdesc(.Fb)
	.word	.Fb
