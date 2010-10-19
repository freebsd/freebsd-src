#source: reloc-rel8.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL8

.*:     file format elf32-crx

Disassembly of section .text_8:

000000e0 <_start>:
  e0:	09 70       	beq	0x[0-9a-f]* [-_<>+0-9a-z]*
