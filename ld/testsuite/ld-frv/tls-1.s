        .section        .tbss,"awT",@nobits
        .align 4
	.globl i
        .type   i, @object
        .size   i, 4
i:
        .zero   4
        .align 4
        .type   l, @object
        .size   l, 4
l:
        .zero   4
	.text
	.globl	_start
_start:
	call	#gettlsoff(x)

	sethi.p	#gottlsdeschi(x), gr14
	setlo	#gottlsdesclo(x), gr14
	ldd	#tlsdesc(x)@(gr15, gr14), gr8
	calll	#gettlsoff(x)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(x)), gr8
	setlos	#gottlsdesclo(x), gr14
	calll	#gettlsoff(x)@(gr8, gr0)

	call	#gettlsoff(i)

	sethi.p	#gottlsdeschi(i), gr14
	setlo	#gottlsdesclo(i), gr14
	ldd	#tlsdesc(i)@(gr15, gr14), gr8
	calll	#gettlsoff(i)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(i)), gr8
	setlos	#gottlsdesclo(i), gr14
	calll	#gettlsoff(i)@(gr8, gr0)

	call	#gettlsoff(l)

	sethi.p	#gottlsdeschi(l), gr14
	setlo	#gottlsdesclo(l), gr14
	ldd	#tlsdesc(l)@(gr15, gr14), gr8
	calll	#gettlsoff(l)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(l)), gr8
	setlos	#gottlsdesclo(l), gr14
	calll	#gettlsoff(l)@(gr8, gr0)

	call	#gettlsoff(0)

	sethi.p	#gottlsdeschi(0), gr14
	setlo	#gottlsdesclo(0), gr14
	ldd	#tlsdesc(0)@(gr15, gr14), gr8
	calll	#gettlsoff(0)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(0)), gr8
	setlos	#gottlsdesclo(0), gr14
	calll	#gettlsoff(0)@(gr8, gr0)

	sethi.p	#tlsmoffhi(l), gr8
	setlo	#tlsmofflo(l), gr8

	sethi.p	#tlsmoffhi(i), gr9
	setlo	#tlsmofflo(i), gr9

	ldi	@(gr15, #gottlsoff12(x)), gr9
	ldi	@(gr15, #gottlsoff12(i)), gr9
	ldi	@(gr15, #gottlsoff12(l)), gr9
	ldi	@(gr15, #gottlsoff12(0)), gr9

	sethi.p	#gottlsoffhi(x), gr14
	setlo	#gottlsofflo(x), gr14
	ld	#tlsoff(x)@(gr15, gr14), gr9

	sethi.p	#gottlsoffhi(i), gr14
	setlo	#gottlsofflo(i), gr14
	ld	#tlsoff(i)@(gr15, gr14), gr9
	
	sethi.p	#gottlsoffhi(l), gr14
	setlo	#gottlsofflo(l), gr14
	ld	#tlsoff(l)@(gr15, gr14), gr9

	sethi.p	#gottlsoffhi(0), gr14
	setlo	#gottlsofflo(0), gr14
	ld	#tlsoff(0)@(gr15, gr14), gr9
