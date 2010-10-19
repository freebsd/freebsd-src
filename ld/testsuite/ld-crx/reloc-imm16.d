#source: reloc-imm16.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test immediate relocation R_CRX_IMM16

.*:     file format elf32-crx

Disassembly of section .text_16:

00001010 <_start>:
    1010:	ee 11 14 20 	addw	\$0x2014, r14
