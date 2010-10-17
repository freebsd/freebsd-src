	.file	"test.c"
	.data

! Hitachi SH cc1 (cygnus-2.7.1-950728) arguments: -O -fpeephole
! -ffunction-cse -freg-struct-return -fdelayed-branch -fcommon -fgnu-linker

gcc2_compiled.:
___gnu_compiled_c:
	.text
	.align 2
	.global	_foo
_foo:
	fmov.s	@r0,fr0
	fmov.s	fr0,@r0
	fmov.s	@r0+,fr0
	fmov.s	fr0,@-r0
	fmov.s	@(r0,r0),fr0
	fmov.s	fr0,@(r0,r0)
	fmov	fr0,fr1
	fldi0	fr0
	fldi1	fr0
	fadd	fr0,fr1
	fsub	fr0,fr1
	fmul	fr0,fr1
	fdiv	fr0,fr1
	fmac	fr0,fr0,fr1
	fcmp/eq	fr0,fr1
	fcmp/gt	fr0,fr1
	fneg	fr0
	fabs	fr0
	fsqrt	fr0
	float	fpul,fr0
	ftrc	fr0,fpul
	fsts	fpul,fr0
	flds	fr0,fpul
	lds	r3,fpul
	lds.l	@r3+,fpul
	lds	r3,fpscr
	lds.l	@r3+,fpscr
	sts	fpul,r3
	sts.l	fpul,@-r3
	sts	fpscr,r3
	sts.l	fpscr,@-r3

