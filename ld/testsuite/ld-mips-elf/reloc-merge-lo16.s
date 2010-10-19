	.section .rodata.str1.4,"aMS", @progbits, 1
	.macro	fillstr char
	.rept	0x3fff - \char
	.byte	\char
	.endr
	.byte	0
	.endm
	fillstr	'a'
	fillstr	'h'
	fillstr	'c'
	fillstr	'd'
	fillstr	'g'
	fillstr	'f'
g:
	fillstr	'g'
	fillstr	'h'

	.text
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
	lui	$2, %hi(g)
	addiu	$3, $2, %lo(g)
	addiu	$2, $2, %lo(g)
	.end	__start

	.space	16
