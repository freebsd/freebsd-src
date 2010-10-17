	.file	1 "z.c"
	.set noat
	.set noreorder
.text
	.align 4
	.globl f
	.ent f
$f..ng:
f:
	.frame $15,32,$26,0
	.mask 0x4008200,-32
	.fmask 0x4,-8
	lda $30,-32($30)
	stq $26,0($30)
	stq $9,8($30)
	stq $15,16($30)
	stt $f2,24($30)
	mov $30,$15
	.prologue 0
	mov $15,$30
	ldq $26,0($30)
	ldq $9,8($30)
	ldt $f2,24($30)
	ldq $15,16($30)
	lda $30,32($30)
	ret $31,($26),1
	.end f
	.ident	"GCC: (GNU) 2.96 20000731 (Red Hat Linux 7.2 2.96-112.7.1)"
