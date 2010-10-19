@ Test file for ARM ELF PIC

.text
.align 0
	bl	foo
	bl	foo(PLT)
	.word	sym
	.word	sym(GOT)
	.word	sym(GOTOFF)
1:
	.word	_GLOBAL_OFFSET_TABLE_ - 1b
	.word foo2(TARGET1)
	.word foo3(SBREL)
	.word foo4(TARGET2)
