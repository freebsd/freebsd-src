	.file	1 "z.c"
	.set noat
	.set noreorder
.text
	.align 4
	.globl f
	.type f,@function
	.usepv f,no
	.cfi_startproc
f:
	lda $30,-32($30)
	.cfi_adjust_cfa_offset 32
	stq $26,0($30)
	.cfi_offset $26, -32
	stq $9,8($30)
	.cfi_offset $9, -24
	stq $15,16($30)
	.cfi_offset $15, -16
	stt $f2,24($30)
	.cfi_offset $f2, -8
	mov $30,$15
	.cfi_def_cfa_register $15

	nop
	nop
	nop

	mov $15,$30
	ldq $26,0($30)
	ldq $9,8($30)
	ldt $f2,24($30)
	ldq $15,16($30)
	lda $30,32($30)
	.cfi_def_cfa $30, 0
	ret $31,($26),1
	.size f, .-f
	.cfi_endproc
