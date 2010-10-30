        .text
        .global main
main:
	##########
	# JCond regp
	##########
	jeq	(r1,r0)
	jne	(r2,r1)
	jcc	(r3,r2)
	jcc	(r4,r3)
	jhi	(r5,r4)
	jlt	(r6,r5)
	jgt	(r7,r6)
	jfs	(r8,r7)
	jfc	(r9,r8)
	jlo	(r10,r9)
	jhi	(r11,r10)
	jlt	(r1,r0)
	jge	(r3,r2)
	jump	(r6,r5)
	jusr	(r6,r5)
