# objdump: -dr
# as: -linkrelax
# source: reloc8.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	b9002dea 	syncd 0,\$45,234
			1: R_MMIX_8	foo
   4:	372f002a 	negu \$47,0,42
			6: R_MMIX_8	bar\+0x30
   8:	fd00b26e 	swym 0,178,110
			9: R_MMIX_8	baz\+0xfffffffffffffffe
   c:	ff000000 	trip 0,0,0
			d: R_MMIX_8	fee\+0xffffffffffffffff
			e: R_MMIX_8	fie\+0x1
			f: R_MMIX_8	foe\+0x3
  10:	f9000000 	resume 0
			13: R_MMIX_8	foobar\+0x8
