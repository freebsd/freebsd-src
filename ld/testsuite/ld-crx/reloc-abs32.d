#source: reloc-abs32.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test absolute relocation R_CRX_ABS32

.*:     file format elf32-crx

Disassembly of section .text_32:

11014000 <_start>:
11014000:	01 33 01 22 	loadb	0x22014006 [-_<>+0-9a-z]*, r1
11014004:	06 40 
