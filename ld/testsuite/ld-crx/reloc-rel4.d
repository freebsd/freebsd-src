#source: reloc-rel4.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL4

.*:     file format elf32-crx

Disassembly of section .text_4:

0000000a <_start>:
   a:	3a b0       	beq0b	r10, 0x8 [-_<>+0-9a-z]*
