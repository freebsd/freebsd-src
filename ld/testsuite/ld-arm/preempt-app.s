	@ Preempt an ARM shared library function with a Thumb function
	@ in the application.
	.text
	.p2align 4
	.globl _start
_start:
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
	.globl lib_func1
	.type lib_func1,%function
	.thumb_func
lib_func1:
	bx lr

	.data
	.long data_obj
