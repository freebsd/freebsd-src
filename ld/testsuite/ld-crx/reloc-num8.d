#source: reloc-num8.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test relocation on data R_CRX_NUM8

.*:     file format elf32-crx

Disassembly of section .text:

.* <_start>:
.*:	12 00       	addub	\$0x1, r2
