	.text
	.align	0

loadhalfwords:
	ldrh	r0, [r1]
	ldrh	r0, [r1]!
	ldrh	r0, [r1, r2]
	ldrh	r0, [r1, r2]!
	ldrh	r0, [r1,#0x0C]
	ldrh	r0, [r1,#0x0C]!
	ldrh	r0, [r1,#-0x0C]
	ldrh	r0, [r1], r2
	ldrh	r0, =0xFF00
	ldrh	r0, =0xC0DE
	ldrh	r0, .L2

storehalfwords:
	strh	r0, [r1]
	strh	r0, [r1]!
	strh	r0, [r1, r2]
	strh	r0, [r1, r2]!
	strh	r0, [r1,#0x0C]
	strh	r0, [r1,#0x0C]!
	strh	r0, [r1,#-0x0C]
	strh	r0, [r1], r2
	strh	r0, .L2

loadsignedbytes:
	ldrsb	r0, [r1]
	ldrsb	r0, [r1]!
	ldrsb	r0, [r1, r2]
	ldrsb	r0, [r1, r2]!
	ldrsb	r0, [r1,#0x0C]
	ldrsb	r0, [r1,#0x0C]!
	ldrsb	r0, [r1,#-0x0C]
	ldrsb	r0, [r1], r2
	ldrsb	r0, =0xDE
	ldrsb	r0, .L2

loadsignedhalfwords:
	ldrsh	r0, [r1]
	ldrsh	r0, [r1]!
	ldrsh	r0, [r1, r2]
	ldrsh	r0, [r1, r2]!
	ldrsh	r0, [r1, #0x0C]
	ldrsh	r0, [r1, #0x0C]!
	ldrsh	r0, [r1, #-0x0C]
	ldrsh	r0, [r1], r2
	ldrsh	r0, =0xFF00
	ldrsh	r0, =0xC0DE
	ldrsh	r0, .L2
	
misc:
	ldralh	r0, [r1, r2]
	ldrneh	r0, [r1, r2]
	ldrhih	r0, [r1, r2]
	ldrlth	r0, [r1, r2]

	ldralsh	r0, [r1, r2]
	ldrnesh	r0, [r1, r2]
	ldrhish	r0, [r1, r2]
	ldrltsh	r0, [r1, r2]

	ldralsb	r0, [r1, r2]
	ldrnesb	r0, [r1, r2]
	ldrhisb	r0, [r1, r2]
	ldrltsb	r0, [r1, r2]

	ldrsh	r0, =0xC0DE
	ldrsh	r0, =0xDEAD

	.align
.L2:
	.word	fred
	
	.ltorg

	# Add two nop instructions to ensure that the
	# output is 32-byte aligned as required for arm-aout.
	nop
	nop
