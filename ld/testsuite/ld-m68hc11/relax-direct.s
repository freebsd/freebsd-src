;;; Test 68HC11 linker relaxation from extended addressing to direct
;;; addressing modes
;;; 
	.sect .text
	.globl _start
_start:
start:	
	lds	stack
	ldd	_bar
	beq	F1
	beq	F2
	std	_bar
	jsr	_bar
F1:
	addd	_toto
	bne	start
	;; All the following instructions will be relaxed and win 1 byte
	;; for each.
	addd	_toto+200
	addd	stack+256-20
	adca	_table+2
	adcb	_table+3
	adda	_table+4
	addb	_table+5
	addd	_table+6
	anda	_table+7
	andb	_table+8
	cmpa	_table+9
	cmpb	_table+10
	cpd	_table+11
	cpx	_table+12
	cpy	_table+13
	eora	_table+14
	eorb	_table+15
	jsr	_table+16
	ldaa	_table+17
	ldab	_table+18
	ldd	_table+19
	lds	_table+20
	ldx	_table+21
	ldy	_table+22
	oraa	_table+23
	orab	_table+24
	sbcb	_table+25
	sbca	_table+26
	staa	_table+27
	stab	_table+28
	std	_table+29
	sts	_table+30
	stx	_table+31
	sty	_table+32
	suba	_table+33
	subb	_table+34
	subd	_table+35
	;; 'bne' is assembled as far branch and must relax to 
	;; a relative 8-bit branch.
	bne	_start
	;; Likewise for next branch
	bra	F1
	rts

;;; The following instructions will not be relaxed
no_relax:
	addd	_stack_top+60
	std	_stack_top+40
	;; 'tst' does not support direct addressing mode.
	tst	_toto+1
	bne	no_relax
	.skip	200
F2:
	bra	_start

	.sect	.page0
_bar:
	.long	0
_toto:
	.long	0
	.skip	32
stack:
	.skip	10
_table:
	.skip	200
_stack_top:

