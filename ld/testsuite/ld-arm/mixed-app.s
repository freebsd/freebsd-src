	.text
	.p2align 4
	.globl _start
_start:
	mov	ip, sp
	stmdb	sp!, {r11, ip, lr, pc}
	bl	app_func
	ldmia	sp, {r11, sp, lr}
	bx lr

	.p2align 4
	.globl app_func
	.type app_func,%function
app_func:
	mov	ip, sp
	stmdb	sp!, {r11, ip, lr, pc}
	bl	lib_func1
	ldmia	sp, {r11, sp, lr}
	bx lr

	.p2align 4
	.globl app_func2
	.type app_func2,%function
app_func2:
	bx	lr

	.p2align 4
	.globl app_tfunc
	.type app_tfunc,%function
	.thumb_func
	.code 16
app_tfunc:
	push	{lr}
	bl	lib_func2
	pop	{pc}
	bx	lr

	.data
	.long data_obj
