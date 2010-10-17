@       Test file for ARM/GAS -- ldr reg, =... expressions.

.text
.align
foo:
	ldr	r0, =0
	ldr	r0, =0xff000000
	ldr	r0, =-1
	ldr	r0, =0x0fff0000
	.pool

	ldr	r14, =0
	ldr	r14, =0x00ff0000
	ldr	r14, =0xff00ffff
	ldr	r14, =0x00fff000
	.pool

	ldreq	r0, =0
	ldreq	r0, =0x0000ff00
	ldreq	r0, =0xffff00ff
	ldreq	r0, =0x000fff00
	.pool

	ldrmi	r11, =0
	ldrmi	r11, =0x000000ff
	ldrmi	r11, =0xffffff00
	ldrmi	r11, =0x0000fff0
	.pool
