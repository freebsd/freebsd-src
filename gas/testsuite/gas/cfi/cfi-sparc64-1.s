#; $ as -o test.o -64 gas-cfi-test.s && gcc -m64 -nostdlib -o test test.o

	.file	"a.c"
	.text
	.align	4
	.globl foo
	.type	foo, @function
foo:
	.cfi_startproc
	save	%sp, -192, %sp
	.cfi_def_cfa_register	%fp
	.cfi_window_save
	.cfi_register	%o7, %i7
	add	%i0, 1, %o0
	add	%i0, 2, %i0
	call	bar, 0
	sra	%o0, 0, %o0
	sra	%i0, 0, %i0
	call	bar, 0
	mov	%i0, %o0
	add	%o0, 3, %o0
	sra	%o0, 0, %o0
	ret
	restore	%g0, %o0, %o0
	.cfi_endproc
	.size	foo, .-foo
