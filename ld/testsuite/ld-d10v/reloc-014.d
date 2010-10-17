#source: reloc-005.s
#ld: -T $srcdir/$subdir/reloc-014.ld
#objdump: -D

# Test 18 bit pc rel reloc negative good boundary case

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <foo>:
	...

01034000 <_start>:
 1034000:	e4 00 80 00 	bra.l	1014000 <foo>
 1034004:	26 0d 5e 00 	jmp	r13	||	nop	
