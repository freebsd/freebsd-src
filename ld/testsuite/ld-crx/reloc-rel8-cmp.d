#source: reloc-rel8-cmp.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL8_CMP

.*:     file format elf32-crx

Disassembly of section .text_8:

000000e0 <_start>:
  e0:	81 30 0a 20 	cmpbeqb	r1, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
