	.text
h8300_cbranch:
	bra h8300_cbranch
	bt h8300_cbranch
	brn h8300_cbranch
	bf h8300_cbranch
	bhi h8300_cbranch
	bls h8300_cbranch
	bcc h8300_cbranch
	bhs h8300_cbranch
	bcs h8300_cbranch
	blo h8300_cbranch
	bne h8300_cbranch
	beq h8300_cbranch
	bvc h8300_cbranch
	bvs h8300_cbranch
	bpl h8300_cbranch
	bmi h8300_cbranch
	bge h8300_cbranch
	blt h8300_cbranch
	bgt h8300_cbranch
	ble h8300_cbranch

