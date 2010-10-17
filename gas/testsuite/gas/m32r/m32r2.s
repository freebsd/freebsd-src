# Test new instructions
	
	.text
	.global setpsw
setpsw:
	setpsw 0xc1
	setpsw 0xff

	.text
	.global clrpsw
clrpsw:
	clrpsw 0xc1
	clrpsw 0xff

	.text
	.global bset
bset:
	bset #0,@(4,r1)
	bset #1,@(4,r1)
	bset #7,@(4,r1)

	.text
	.global bclr
bclr:
	bclr #0,@(4,r1)
	bclr #1,@(4,r1)
	bclr #7,@(4,r1)

	.text
	.global btst
btst:
	btst #0,fp
	btst #1,fp
	btst #7,fp
	btst #1,fp || mv r0,r2
	mv r0,r2 || btst #1,fp

	.text
	.global divuh
divuh:
	divuh fp,fp

	.text
	.global divb
divb:
	divb fp,fp
	
	.text
	.global divub
divub:
	divub fp,fp
	
	.text
	.global remh
remh:
	remh fp,fp
	
	.text
	.global remuh
remuh:
	remuh fp,fp
	
	.text
	.global remb
remb:
	remb fp,fp
	
	.text
	.global remub
remub:
	remub fp,fp
	
	.text
	.global sll
sll:
	sll r0,r1 || sll r2,r3
	mul r0,r1 || sll r2,r3
	sll r0,r1 || mul r2,r3
	ldi r0,#1 || sll r2,r3
	sll r0,r1 || ldi r2,#1

	.text
	.global slli
slli:
	slli r0,#1 || slli r2,#31
	mul r0,r1 || slli r2,#31
	slli r0,#1 || mul r2,r3
	ldi r0,#1 || slli r2,#31
	slli r0,#1 || ldi r2,#1

	.text
	.global sra
sra:
	sra r0,r1 || sra r2,r3
	mul r0,r1 || sra r2,r3
	sra r0,r1 || mul r2,r3
	ldi r0,#1 || sra r2,r3
	sra r0,r1 || ldi r2,#1

	.text
	.global srai
srai:
	srai r0,#1 || srai r2,#31
	mul r0,r1 || srai r2,#31
	srai r0,#1 || mul r2,r3
	ldi r0,#1 || srai r2,#31
	srai r0,#1 || ldi r2,#1

	.text
	.global sra
srl:
	srl r0,r1 || srl r2,r3
	mul r0,r1 || srl r2,r3
	srl r0,r1 || mul r2,r3
	ldi r0,#1 || srl r2,r3
	srl r0,r1 || ldi r2,#1

	.text
	.global srai
srli:
	srli r0,#1 || srli r2,#31
	mul r0,r1 || srli r2,#31
	srli r0,#1 || mul r2,r3
	ldi r0,#1 || srli r2,#31
	srli r0,#1 || ldi r2,#1

