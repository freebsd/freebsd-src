#
# Try to verify all branchs for 68HC12
# Ensures that PC-relative relocations are correct.
#
	sect .text
	globl start

start:
L0:
	;; Branchs to defined symbols, positive offset < 128
	bgt	L1
	bge	L1
	ble	L1
	blt	L1
	bhi	L1
	bhs	L1
	bcc	L1
	beq	L1
	bls	L1
	blo	L1
	bcs	L1
	bmi	L1
	bvs	L1
	bra	L1
	bvc	L1
	bne	L1
	bpl	L1
	brn	L1

	;; Branchs to defined symbols, negative offset > -128
	bgt	L0
	bge	L0
	ble	L0
	blt	L0
	bhi	L0
	bhs	L0
	bcc	L0
	beq	L0
	bls	L0
	blo	L0
	bcs	L0
	bmi	L0
	bvs	L0
	bra	L0
	bvc	L0
	bne	L0
	bpl	L0
	brn	L0
L1:
	;; Branchs to defined symbols, positive offset > -128
	lbgt	L2
	lbge	L2
	lble	L2
	lblt	L2
	lbhi	L2
	lbhs	L2
	lbcc	L2
	lbeq	L2
	lbls	L2
	lblo	L2
	lbcs	L2
	lbmi	L2
	lbvs	L2
	lbra	L2
	lbvc	L2
	lbne	L2
	lbpl	L2
	lbrn	L2

	;; Branchs to undefined symbols, translated into lbcc
	bgt	undefined
	bge	undefined
	ble	undefined
	blt	undefined
	bhi	undefined
	bhs	undefined
	bcc	undefined
	beq	undefined
	bls	undefined
	blo	undefined
	bcs	undefined
	bmi	undefined
	bvs	undefined
	bra	undefined
	bvc	undefined
	bne	undefined
	bpl	undefined
	brn	undefined

	;; Far branchs to undefined symbols
	lbgt	undefined+16
	lbge	undefined+16
	lble	undefined+16
	lblt	undefined+16
	lbhi	undefined+16
	lbhs	undefined+16
	lbcc	undefined+16
	lbeq	undefined+16
	lbls	undefined+16
	lblo	undefined+16
	lbcs	undefined+16
	lbmi	undefined+16
	lbvs	undefined+16
	lbra	undefined+16
	lbvc	undefined+16
	lbne	undefined+16
	lbpl	undefined+16
	lbrn	undefined+16
	.skip	200
L2:
	rts
