#source: reloc-009.s
#ld: -T $srcdir/$subdir/reloc-009.ld
#objdump: -D

# Test 10 bit pc rel reloc negative normal case

.*:     file format elf32-d10v
Disassembly of section .text:

01014000 <foo>:
	...

01014100 <_start>:
 1014100:	6f 00 4a c0 	nop		->	brf0f.s	1014000 <foo>
 1014104:	26 0d 5e 00 	jmp	r13	||	nop	
