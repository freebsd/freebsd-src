	.text
	.arch armv5t
	.global arm
	.global t1
	.global t2
	.global t5
arm:
	bx lr
	.thumb
	.thumb_func
t1:
	bx lr
	.thumb_func
t2:
	bl t3
	bl t4
	.thumb_func
t5:
	bl local_thumb
	nop
local_thumb:
	blx t3
	bl _start
	blx _start
