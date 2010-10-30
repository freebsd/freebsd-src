        .text
        .global main
main:
	################
	# JAL regp regp
	################
	jal (r2,r1)
	jal (r6,r5),(r2,r1)
	jal (r3,r2),(r4,r3)
	jal (r1,r0), (r4,r3)
	jal (r3,r2), (r8,r7)
