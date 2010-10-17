#; $ as -o test.o -m64 -march=z900 gas-cfi-test.s && gcc -m64 -nostdlib -o test test.o

	.file	"a.c"
	.text
	.align	4
	.globl foo
	.type	foo, @function
foo:
	.cfi_startproc
	stmg	%r8,%r15,64(%r15)
	.cfi_offset %r15,-40
	.cfi_offset %r14,-48
	.cfi_offset %r13,-56
	.cfi_offset %r12,-64
	.cfi_offset %r11,-72
	.cfi_offset %r10,-80
	.cfi_offset %r9,-88
	.cfi_offset %r8,-96
	lgr	%r14,%r15
	aghi	%r15,-160
	.cfi_adjust_cfa_offset 160
	lgr	%r12,%r3
	lgr	%r10,%r4
	lgr	%r9,%r2
	lgfr	%r9,%r9
	stg	%r14,0(%r15)
	brasl	%r14,bar1
	lgfr	%r12,%r12
	lgfr	%r10,%r10
	lgr	%r3,%r12
	lgr	%r4,%r10
	lgr	%r8,%r2
	lgr	%r2,%r9
	brasl	%r14,syscall
	lgfr	%r8,%r8
	lgr	%r12,%r2
	lgr	%r2,%r8
	brasl	%r14,bar2
	lgfr	%r12,%r12
	lgr	%r2,%r12
	lg	%r4,272(%r15)
	lmg	%r8,%r15,224(%r15)
	br	%r4
	.cfi_endproc
	.size	foo, .-foo
