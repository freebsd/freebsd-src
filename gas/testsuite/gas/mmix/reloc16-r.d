# objdump: -dr
# as: -linkrelax
# source: reloc16.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	e3040000 	setl \$4,0x0
			2: R_MMIX_16	foo
   4:	f82d0000 	pop 45,0
			6: R_MMIX_16	bar\+0x2a
   8:	fd2a0000 	swym 42,0,0
			a: R_MMIX_16	baz\+0xfffffffffffff6d7
