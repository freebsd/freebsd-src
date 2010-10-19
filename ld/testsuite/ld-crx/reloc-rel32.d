#source: reloc-rel32.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test pc relative relocation R_CRX_REL32

.*:     file format elf32-crx

Disassembly of section .text_32:

11014000 <_start>:
11014000:	7f 7e 80 08 	br	0x[0-9a-f]* [-_<>+0-9a-z]*
11014004:	03 00 
