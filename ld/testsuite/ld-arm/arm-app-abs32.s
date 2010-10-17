	.text
	.globl _start
_start:
	mov	ip, sp
	stmdb	sp!, {r11, ip, lr, pc}
	ldr	a1, .Lval
	ldmia	sp, {r11, sp, lr}
	bx	lr

.Lval:
	.long	lib_func1

	.globl app_func2
app_func2:
	bx	lr

