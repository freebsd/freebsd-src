	.text
	.weak _start
_start:
	.global F7
	.type F7,@function
F7:
	call	.F0+4

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
	addi	gr15, #got12(.F1+4), gr0
	
	setlos	#gotlo(.F2+4), gr0
	
	setlo	#gotlo(.F3+4), gr0
	sethi	#gothi(.F3+4), gr0

	addi	gr15, #gotfuncdesc12(.F4+4), gr0

	setlos	#gotfuncdesclo(.F5+4), gr0

	setlo	#gotfuncdesclo(.F6+4), gr0
	sethi	#gotfuncdeschi(.F6+4), gr0

	addi	gr15, #gotofffuncdesc12(.F7+4), gr0

	setlos	#gotofffuncdesclo(.F8+4), gr0

	setlo	#gotofffuncdesclo(.F9+4), gr0
	sethi	#gotofffuncdeschi(.F9+4), gr0

	addi	gr15, #gotoff12(.D1+4), gr0
	
	setlos	#gotofflo(.D2+4), gr0

	setlo	#gotofflo(.D3+4), gr0
	sethi	#gotoffhi(.D3+4), gr0

	setlo	#gotlo(.D4+4), gr0
	sethi	#gothi(.D4+4), gr0

	.data
	.global D7
D7:
	.word	.D0+4
.D0:
.D1:
.D2:
.D3:
.D4:
	.picptr funcdesc(.Fb+4)
	.word	.Fb+4
