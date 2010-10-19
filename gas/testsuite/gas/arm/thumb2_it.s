	.text
	.thumb
	.syntax unified
	.thumb_func
foo:
	ittet eq
	addeq r0, r0, r2
	addeq r0, r0, r8
	addne r0, r1, r2
	addseq r0, r1, r2
	add r0, r0, r2
	add r0, r0, r8
	adds r0, r0, r2
	adds r0, r0, r8
	adds r0, r1, r2

	itet eq
	orreq r0, r0, r2
	orrne r0, r0, r8
	orrseq r0, r0, r2
	orr r0, r0, r2
	orr r0, r0, r8
	orrs r0, r0, r2

	itttt eq
	lsleq r0, r0, r2
	lsleq r0, r0, r8
	lsleq r0, r1, r2
	lslseq r0, r0, r2
	ittt eq
	lsleq r0, r1, #1
	lsleq r0, r8, #1
	lslseq r0, r0, #1
	lsl r0, r0, r2
	lsls r0, r0, r2
	lsl r0, r1, #1
	lsls r0, r1, #1

	itttt eq
	cmpeq r0, r1
	cmpeq r0, r8
	moveq r0, r1
	movseq r0, r1
	it eq
	moveq r0, r8
	mov r0, r1
	movs r0, r1
	movs r0, r8

	itttt eq
	mvneq r0, r1
	mvneq r0, r8
	mvnseq r0, r1
	cmneq r0, r1
	mvn r0, r1
	mvns r0, r1

	ittt eq
	negeq r0, r1
	negeq r0, r8
	negseq r0, r1
	neg r0, r1
	negs r0, r1

