	.text
	.global foo
foo:
	subi gr15, #gprel12(bar), gr16

	addi gr15, #got12(foo), gr4
	ldi @(gr15, #got12(foo)), gr5

	setlo #gotlo(foo), gr6
	sethi #gothi(foo), gr6

	addi gr15, #gotfuncdesc12(foo), gr7
	ldi @(gr15, #gotfuncdesc12(foo)), gr8

	setlo #gotfuncdesclo(foo), gr9
	sethi #gotfuncdeschi(foo), gr9

	addi gr15, #gotoff12(baz), gr16

	addi gr15, #gotoff12(foo), gr4
	ldi @(gr15, #gotoff12(foo)), gr5

	setlo #gotofflo(foo), gr6
	sethi #gotoffhi(foo), gr6

	addi gr15, #gotofffuncdesc12(foo), gr7
	ldi @(gr15, #gotofffuncdesc12(foo)), gr8

	setlo #gotofffuncdesclo(foo), gr9
	sethi #gotofffuncdeschi(foo), gr9

	.section .rodata
bar:

	.section .sdata,"aw",@progbits
	.p2align 2
baz:
	.picptr	funcdesc(foo)
	.word	foo
