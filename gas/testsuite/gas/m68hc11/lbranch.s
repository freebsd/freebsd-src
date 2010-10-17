# Test for the 68HC11 long branch switch
	.text
	.globl	_rcall
	.globl _start
_start:
_rcall:
	ldaa	#0x10		;86 10
	jbra	Lend		; Must be switched to a jmp
	jbsr	toto		; -> to a jsr
	jbne	toto		; -> to a beq+jmp
	jbeq	toto		; -> to a bne+jmp
	jbcs	toto		; -> to a bcc+jmp
	jbcc	toto		; -> to a bcs+jmp
	xgdx
	xgdx
	beq	bidule		; -> to a bne+jmp
	bcs	bidule		; -> to a bcc+jmp
	bcc	bidule		; -> to a bcs+jmp
	xgdx
	jbra	200
	jbsr	1923
	bne	Lend		; -> to a beq+jmp
	jbsr	toto
	jbeq	toto
	.skip 200
	ldaa	*dir		;96 33
Lend:
	bhi	external_op
	bls	external_op
	bsr	out
	ldx	#12
toto:	
	rts
	.skip 100
bidule:
	rts
	.sect ".page0"
dir:
	.long 0
	
	; END
