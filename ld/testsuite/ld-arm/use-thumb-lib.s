	.cpu arm10tdmi
	.fpu softvfp
	.eabi_attribute 18, 4
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.file	"use_thumb_lib.c"
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	bl	lib_func2
	ldmfd	sp, {fp, sp, pc}
	.size	foo, .-foo
	.ident	"GCC: (GNU) 4.1.0 (CodeSourcery ARM 2006q1-7)"
