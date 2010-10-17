#; $ as -o test.o -m31 -march=g5 gas-cfi-test.s && gcc -m32 -nostdlib -o test test.o

	.file	"a.c"
	.text
	.align	4
	.globl foo
	.type	foo, @function
foo:
	.cfi_startproc
	stm	%r8,%r15,32(%r15)
	.cfi_offset %r15,-36
	.cfi_offset %r14,-40
	.cfi_offset %r13,-44
	.cfi_offset %r12,-48
	.cfi_offset %r11,-52
	.cfi_offset %r10,-56
	.cfi_offset %r9,-60
	.cfi_offset %r8,-64
	bras	%r13,.L3
.L2:
	.align	4
.LC0:
	.long	bar1
.LC1:
	.long	syscall
.LC2:
	.long	bar2
	.align	2
.L3:
	lr	%r14,%r15
	ahi	%r15,-96
	.cfi_adjust_cfa_offset 96
	lr	%r12,%r2
	l	%r2,.LC0-.L2(%r13)
	lr	%r10,%r3
	lr	%r9,%r4
	st	%r14,0(%r15)
	basr	%r14,%r2
	l	%r1,.LC1-.L2(%r13)
	lr	%r4,%r9
	lr	%r8,%r2
	lr	%r3,%r10
	lr	%r2,%r12
	basr	%r14,%r1
	l	%r1,.LC2-.L2(%r13)
	lr	%r12,%r2
	lr	%r2,%r8
	basr	%r14,%r1
	lr	%r2,%r12
	l	%r4,152(%r15)
	lm	%r8,%r15,128(%r15)
	br	%r4
	.cfi_endproc
	.size	foo, .-foo
