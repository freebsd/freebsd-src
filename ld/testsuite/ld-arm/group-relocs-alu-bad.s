@ Test intended to fail for ALU group relocations.
@
@ Beware when editing this file: it is carefully crafted so that
@ a specific PC-relative offset arises.

@ We will place .text at 0x8000.

	.text
	.globl _start

_start:
	add r0, r0, #:pc_g0:(bar)

@ We will place the section foo at 0x9004.

	.section foo

bar:
	mov r0, #0

