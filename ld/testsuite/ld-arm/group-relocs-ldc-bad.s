@ Test intended to fail for LDC group relocations.

@ We will place .text at 0x8000.

	.text
	.globl _start

_start:
	add	r0, r0, #:pc_g0_nc:(bar)
	ldc	0, c0, [r0, #:pc_g1:(bar + 4)]

@ We will place the section foo at 0x118400.
@ (The relocations above would be OK if it were at 0x118200, for example.)

	.section foo

bar:
	mov r0, #0

