	.text
	.align 16
	.global foo#
	.proc foo#
foo:
	.prologue 2, 2
	.vframe r2
	mov r2 = r12
	.body
	.restore sp
	mov r12 = r2
	br.ret.sptk.many b0
	.endp foo#
