#source: reloc-regrel28.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test register relative relocation R_CRX_REGREL28

.*:     file format elf32-crx

Disassembly of section .text_28:

06201400 <_start>:
 6201400:	7f 3b 30 99 	cbitd	\$0x1f, 0x9301406\(r9\)
 6201404:	06 14 
