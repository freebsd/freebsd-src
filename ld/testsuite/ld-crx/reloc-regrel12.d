#source: reloc-regrel12.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test register relative relocation R_CRX_REGREL12

.*:     file format elf32-crx

Disassembly of section .text_12:

00000101 <_start>:
 101:	85 32 05 78 	loadb	0x805\(r7\)\+, r5
