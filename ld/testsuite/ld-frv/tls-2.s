        .section        .tbss,"awT",@nobits
        .align 4
        .type   l, @object
        .size   l, 4
l:
        .zero   4
        .align 4
	.globl i
        .type   i, @object
        .size   i, 4
i:
        .zero   4
	.text
	.globl	_start
_start:
	call	#gettlsoff(l+1)
	call	#gettlsoff(l+1+4096)
	call	#gettlsoff(l+1+65536)

	sethi.p	#gottlsdeschi(l+2), gr14
	setlo	#gottlsdesclo(l+2), gr14
	ldd	#tlsdesc(l+2)@(gr15, gr14), gr8
	calll	#gettlsoff(l+2)@(gr8, gr0)

	sethi.p	#gottlsdeschi(l+2+4096), gr14
	setlo	#gottlsdesclo(l+2+4096), gr14
	ldd	#tlsdesc(l+2+4096)@(gr15, gr14), gr8
	calll	#gettlsoff(l+2+4096)@(gr8, gr0)

	sethi.p	#gottlsdeschi(l+2+65536), gr14
	setlo	#gottlsdesclo(l+2+65536), gr14
	ldd	#tlsdesc(l+2+65536)@(gr15, gr14), gr8
	calll	#gettlsoff(l+2+65536)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(l+3)), gr8
	setlos	#gottlsdesclo(l+3), gr14
	calll	#gettlsoff(l+3)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(l+3+4096)), gr8
	setlos	#gottlsdesclo(l+3+4096), gr14
	calll	#gettlsoff(l+3+4096)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(l+3+65536)), gr8
	setlos	#gottlsdesclo(l+3+65536), gr14
	calll	#gettlsoff(l+3+65536)@(gr8, gr0)

	sethi	#tlsmoffhi(l+4), gr9
	setlo	#tlsmofflo(l+4), gr9

	sethi	#tlsmoffhi(l+4+4096), gr9
	setlo	#tlsmofflo(l+4+4096), gr9

	sethi	#tlsmoffhi(l+4+65536), gr9
	setlo	#tlsmofflo(l+4+65536), gr9

	call	#gettlsoff(i+1)
	call	#gettlsoff(i+1+4096)
	call	#gettlsoff(i+1+65536)

	sethi.p	#gottlsdeschi(i+2), gr14
	setlo	#gottlsdesclo(i+2), gr14
	ldd	#tlsdesc(i+2)@(gr15, gr14), gr8
	calll	#gettlsoff(i+2)@(gr8, gr0)

	sethi.p	#gottlsdeschi(i+2+4096), gr14
	setlo	#gottlsdesclo(i+2+4096), gr14
	ldd	#tlsdesc(i+2+4096)@(gr15, gr14), gr8
	calll	#gettlsoff(i+2+4096)@(gr8, gr0)

	sethi.p	#gottlsdeschi(i+2+65536), gr14
	setlo	#gottlsdesclo(i+2+65536), gr14
	ldd	#tlsdesc(i+2+65536)@(gr15, gr14), gr8
	calll	#gettlsoff(i+2+65536)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(i+3)), gr8
	setlos	#gottlsdesclo(i+3), gr14
	calll	#gettlsoff(i+3)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(i+3+4096)), gr8
	setlos	#gottlsdesclo(i+3+4096), gr14
	calll	#gettlsoff(i+3+4096)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(i+3+65536)), gr8
	setlos	#gottlsdesclo(i+3+65536), gr14
	calll	#gettlsoff(i+3+65536)@(gr8, gr0)

	sethi	#tlsmoffhi(i+4), gr9
	setlo	#tlsmofflo(i+4), gr9

	sethi	#tlsmoffhi(i+4+4096), gr9
	setlo	#tlsmofflo(i+4+4096), gr9

	sethi	#tlsmoffhi(i+4+65536), gr9
	setlo	#tlsmofflo(i+4+65536), gr9

	call	#gettlsoff(0+1)
	call	#gettlsoff(0+1+4096)
	call	#gettlsoff(0+1+65536)

	sethi.p	#gottlsdeschi(0+2), gr14
	setlo	#gottlsdesclo(0+2), gr14
	ldd	#tlsdesc(0+2)@(gr15, gr14), gr8
	calll	#gettlsoff(0+2)@(gr8, gr0)

	sethi.p	#gottlsdeschi(0+2+4096), gr14
	setlo	#gottlsdesclo(0+2+4096), gr14
	ldd	#tlsdesc(0+2+4096)@(gr15, gr14), gr8
	calll	#gettlsoff(0+2+4096)@(gr8, gr0)

	sethi.p	#gottlsdeschi(0+2+65536), gr14
	setlo	#gottlsdesclo(0+2+65536), gr14
	ldd	#tlsdesc(0+2+65536)@(gr15, gr14), gr8
	calll	#gettlsoff(0+2+65536)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(0+3)), gr8
	setlos	#gottlsdesclo(0+3), gr14
	calll	#gettlsoff(0+3)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(0+3+4096)), gr8
	setlos	#gottlsdesclo(0+3+4096), gr14
	calll	#gettlsoff(0+3+4096)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(0+3+65536)), gr8
	setlos	#gottlsdesclo(0+3+65536), gr14
	calll	#gettlsoff(0+3+65536)@(gr8, gr0)

	sethi	#tlsmoffhi(0+4), gr9
	setlo	#tlsmofflo(0+4), gr9

	sethi	#tlsmoffhi(0+4+4096), gr9
	setlo	#tlsmofflo(0+4+4096), gr9

	sethi	#tlsmoffhi(0+4+65536), gr9
	setlo	#tlsmofflo(0+4+65536), gr9

	call	#gettlsoff(x+1)
	call	#gettlsoff(x+1+4096)
	call	#gettlsoff(x+1+65536)

	sethi.p	#gottlsdeschi(x+2), gr14
	setlo	#gottlsdesclo(x+2), gr14
	ldd	#tlsdesc(x+2)@(gr15, gr14), gr8
	calll	#gettlsoff(x+2)@(gr8, gr0)

	sethi.p	#gottlsdeschi(x+2+4096), gr14
	setlo	#gottlsdesclo(x+2+4096), gr14
	ldd	#tlsdesc(x+2+4096)@(gr15, gr14), gr8
	calll	#gettlsoff(x+2+4096)@(gr8, gr0)

	sethi.p	#gottlsdeschi(x+2+65536), gr14
	setlo	#gottlsdesclo(x+2+65536), gr14
	ldd	#tlsdesc(x+2+65536)@(gr15, gr14), gr8
	calll	#gettlsoff(x+2+65536)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(x+3)), gr8
	setlos	#gottlsdesclo(x+3), gr14
	calll	#gettlsoff(x+3)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(x+3+4096)), gr8
	setlos	#gottlsdesclo(x+3+4096), gr14
	calll	#gettlsoff(x+3+4096)@(gr8, gr0)

	lddi.p	@(gr15, #gottlsdesc12(x+3+65536)), gr8
	setlos	#gottlsdesclo(x+3+65536), gr14
	calll	#gettlsoff(x+3+65536)@(gr8, gr0)

.ifdef	static_tls
	ldi	@(gr15, #gottlsoff12(l+1)), gr9
	ldi	@(gr15, #gottlsoff12(l+1+65536)), gr9
	ldi	@(gr15, #gottlsoff12(i+1)), gr9
	ldi	@(gr15, #gottlsoff12(i+1+65536)), gr9
	ldi	@(gr15, #gottlsoff12(0+1)), gr9
	ldi	@(gr15, #gottlsoff12(0+1+65536)), gr9
	ldi	@(gr15, #gottlsoff12(x+1)), gr9
	ldi	@(gr15, #gottlsoff12(x+1+65536)), gr9
	
	setlos	#gottlsofflo(l+1+4096), gr8
	ld	#tlsoff(l+1+4096)@(gr15, gr8), gr9
	
	sethi	#gottlsoffhi(i+1+4096), gr8
	setlo	#gottlsofflo(i+1+4096), gr8
	ld	#tlsoff(i+1+4096)@(gr15, gr8), gr9
.endif
