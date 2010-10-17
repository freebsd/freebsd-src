	.text

	.globl lib_func1
	.type lib_func1, %function
lib_func1:
	mov	ip, sp
	stmdb	sp!, {r11, ip, lr, pc}
	bl	app_func2
	ldmia	sp, {r11, sp, lr}
	bx lr
	.size lib_func1, . - lib_func1

	.globl lib_func2
	.type lib_func2, %function
lib_func2:
	bx lr
	.size lib_func2, . - lib_func2

	.data
	.globl data_obj
	.type data_obj, %object
data_obj:
	.long 0
	.size data_obj, . - data_obj
