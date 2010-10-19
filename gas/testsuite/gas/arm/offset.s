	@ test that an OFFSET_IMM reloc against a global symbol is
	@ still resolved by the assembler, as long as the symbol is in
	@ the same section as the reference
	.text
	.globl l
	.globl foo
l:
	ldr r0, foo
foo:
	nop

	@ pad section for a.out's benefit
	nop
	nop
