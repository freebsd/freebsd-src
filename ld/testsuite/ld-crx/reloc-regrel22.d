#source: reloc-regrel22.s
#ld: -T $srcdir/$subdir/crx.ld
#objdump: -D

# Test register relative relocation R_CRX_REGREL22

.*:     file format elf32-crx

Disassembly of section .text_22:

00201400 <_start>:
  201400:	cd 33 70 9c 	loadb	0x301406\(r9,r12,2\), r13
  201404:	06 14 
