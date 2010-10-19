#source: reloc-regrel32.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test register relative relocation R_CRX_REGREL32

.*:     file format elf32-crx

Disassembly of section .text_32:

11014000 <_start>:
11014000:	f5 87 01 22 	loadb	0x22014006\(r5\), r7
11014004:	06 40 
