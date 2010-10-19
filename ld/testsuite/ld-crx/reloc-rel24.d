#source: reloc-rel24.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL24

.*:     file format elf32-crx

Disassembly of section .text_24:

00f01400 <_start>:
  f01400:	81 31 70 20 	cmpbeqb	r1, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
  f01404:	03 00 
