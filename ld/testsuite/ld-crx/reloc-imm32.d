#source: reloc-imm32.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test immediate relocation R_CRX_IMM32

.*:     file format elf32-crx

Disassembly of section .text_32:

11014000 <_start>:
11014000:	f6 21 01 22 	addd	\$0x22014006, r6
11014004:	06 40 
