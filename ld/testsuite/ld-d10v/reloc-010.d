#source: reloc-009.s
#ld: -T $srcdir/$subdir/reloc-010.ld
#objdump: -D

# Test 10 bit pc rel reloc negative good boundary case

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <foo>:
	...

01014200 <_start>:
 1014200:	6f 00 4a 80 	nop		->	brf0f.s	1014000 <foo>
 1014204:	26 0d 5e 00 	jmp	r13	||	nop	
