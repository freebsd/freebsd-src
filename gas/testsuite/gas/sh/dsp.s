#	Test file for SH/GAS -- dsp instructions

	.text
	.align
	.globl dsp_tests
dsp_tests:
	movs.w @-r2, x0
	movs.w @r3,  x1
	movs.w @r4+, y0
	movs.w @r5+r8, y1
	movs.w m0, @-r5
	movs.w m1, @r4
	movs.w a0, @r3+
	movs.w a1, @r2+r8

	movs.l @-r2, a0g
	movs.l @r3,  a1g
	movs.l @r4+, x0
	movs.l @r5+r8, x1
	movs.l y0, @-r5
	movs.l y1, @r4
	movs.l m0, @r3+
	movs.l m1, @r2+r8

	padd x0,y0,a0
	plds a0,mach
	padd x0,y0,a0
