#; $ as -o test.o -a32 gas-cfi-test.s && gcc -nostdlib -o test test.o

	.file   "a.c"
	.text
	.align 2
	.globl foo
	.type   foo, @function
foo:
	.cfi_startproc
	stwu 1,-48(1)
	.cfi_adjust_cfa_offset 48
	mflr 0
	stw 0,52(1)
	stw 26,24(1)
	stw 27,28(1)
	.cfi_offset 27,-20
	.cfi_offset %r26,-24
	.cfi_offset lr,4
	mr 27,5
	stw 28,32(1)
	.cfi_offset %r.28,-16
	mr 28,4
	stw 29,36(1)
	.cfi_offset 29,-12
	mr 29,3
	bl bar1
	mr 5,27
	mr 26,3
	mr 4,28
	mr 3,29
	bl syscall
	mr 29,3
	mr 3,26
	bl bar2
	lwz 28,32(1)
	lwz 0,52(1)
	mr 3,29
	lwz 26,24(1)
	lwz 27,28(1)
	mtlr 0
	lwz 29,36(1)
	addi 1,1,48
	blr
	.cfi_endproc
	.size   foo, .-foo
