! Check that unaligned pseudos emit the expected relocs and contents
! whether aligned or not.

	.section .rodata,"a"
start:
	.uaquad 0x123456789abcdef
	.byte 42
	.uaword 0x4a21
	.ualong 0x43b1abcd
	.ualong externsym0 + 3
	.uaquad 0x12c456d89ab1d0f
	.uaquad externsym1 + 41
	.byte 2
	.uaquad 0x1a34b67c9ab0d4f
	.ualong externsym2 + 42
	.uaquad externsym3 + 43
