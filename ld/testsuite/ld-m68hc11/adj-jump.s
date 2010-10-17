;;; Test 68HC11 linker relaxation and fixup of bcc/bra branches
;;; 
	.sect .text
	.globl _start
_start:
	;; Next 'bra' is assembled as a 'jmp'.  It is relaxed to 'bra L3'
	;; during a second pass of relax.
	bra	L3
	.skip	20
	;; Next 'jmp' must be relaxed to a 'bra' during the first pass.
	;; The branch offset must then be adjusted by consecutive relax.
	jmp	L3
L1:
	addd	0,x
	bne	L1		; Branch not adjusted
	addd	_toto
	beq	L1		; Backward branch, adjust -1
	addd	_toto+1
	jbne	L1		; Backward branch, adjust -2
	bgt	L1		; All possible backward branchs, adjust -2
	bge	L1
	beq	L1
	ble	L1
	blt	L1
	bhi	L1
	bhs	L1
	beq	L1
	bls	L1
	blo	L1
	bcs	L1
	bmi	L1
	bvs	L1
	bcc	L1
	bpl	L1
	bvc	L1
	bne	L1
	brn	L1
	bra	L1
	;; Relax several insn to reduce block by 15
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
	addd	_toto
L2:
	jmp	_start		; -> relax to bra _start
	bne	L2		; Backward branch, adjust -1
	beq	L3		; Forward branch, adjust -2
	addd	_toto
	beq	L3		; Forward branch, adjust -1
	addd	_toto
L3:
	addd	_toto
	rts

	.sect	.page0
_bar:
	.long	0
_toto:
	.long	0
	.skip	32
stack:
	.skip	10
_table:
