	.text
	.arch armv7
	.thumb
	.syntax unified
	.thumb_func
thumb2_bcond:
	it ne
	bne thumb2_bcond
	it cc
	bcc.w thumb2_bcond
	it cs
	blcs thumb2_bcond
	it lt
	blxlt r5
	it eq
	bxeq r8
	it gt
	tbbgt [r4, r1]
	it lt
	svclt 0
	itt le
	bkpt #0
	nople
	nop
	nop
