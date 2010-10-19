	.text
	.weak	u
	.globl	_start
_start:
	call	#gettlsoff(u)

	sethi.p	#gottlsdeschi(u), gr14
	setlo	#gottlsdesclo(u), gr14
	ldd	#tlsdesc(u)@(gr15, gr14), gr8
	calll	#gettlsoff(u)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(u)), gr8
	setlos	#gottlsdesclo(u), gr14
	calll	#gettlsoff(u)@(gr8, gr0)

	ldi	@(gr15, #gottlsoff12(u)), gr9

	sethi.p	#gottlsoffhi(u), gr14
	setlo	#gottlsofflo(u), gr14
	ld	#tlsoff(u)@(gr15, gr14), gr9
