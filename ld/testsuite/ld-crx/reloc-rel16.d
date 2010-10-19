#source: reloc-rel16.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL16

.*:     file format elf32-crx

Disassembly of section .text_16:

00001010 <_start>:
    1010:	7e 30 02 08 	bal	r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
