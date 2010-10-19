# Test R_ARM_CALL and R_ARM_JUMP24 relocations and interworking
	.text
	.arch armv5t
	.global _start
_start:
	bl arm
	bl t1
	bl t2
	bl t5
	blx t1
	blx t2
	b t1
	b t2
	blne t1
	blne t2
	blne arm
	blx arm
	blx thumblocal
	.thumb
thumblocal:
	bx lr
	.global t3
	.thumb_func
t3:
	bx lr
	.global t4
	.thumb_func
t4:
	bx lr
	nop
