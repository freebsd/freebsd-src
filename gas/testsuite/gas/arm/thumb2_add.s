	.syntax unified
	.text
	.align	2
	.global	thumb2_add
	.thumb
	.thumb_func
thumb2_add:
	add r0, pc, #0x800
	add r9, pc, #0
	add r9, pc, #0x400
	add r8, r9, #0x400
	add r8, r9, #0x101
	add r3, r1, #0x101
	sub r0, pc, #0x800
	sub r9, pc, #0
	sub r9, pc, #0x400
	sub r8, r9, #0x400
	sub r8, r9, #0x101
	sub r3, r1, #0x101
	add r3, #1
	sub r3, #1
	sub sp, sp, #0x100
	sub sp, sp, #0x200
	sub sp, sp, #0x101
	add sp, sp, #0x100
	add sp, sp, #0x200
	add sp, sp, #0x101
	add r0, sp, #0x100
	add r5, sp, #0x400
	add r9, sp, #0x101
	rsbs r1, r6, #0
