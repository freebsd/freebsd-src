#source: reloc-005.s
#ld: -T $srcdir/$subdir/reloc-006.ld
#objdump: -D

# Test 18 bit pc rel reloc good boundary

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <_start>:
 1014000:	e4 00 7f ff 	bra.l	1033ffc <foo>
 1014004:	26 0d 5e 00 	jmp	r13	||	nop	
