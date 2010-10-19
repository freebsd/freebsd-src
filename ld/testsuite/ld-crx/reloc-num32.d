#source: reloc-num32.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test relocation on data R_CRX_NUM32

.*:     file format elf32-crx

Disassembly of section .text:

.* <_start>:
.*:	78 56       	orw	r7, r8
.*:	34 12       	addcw	\$0x3, r4
