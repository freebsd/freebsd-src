#source: reloc-001.s
#ld: -T $srcdir/$subdir/reloc-002.ld
#objdump: -D

# Test 10 bit pc rel reloc good boundary.

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <_start>:
 1014000:	65 3f cc 1a 	brf0f.s	10141fc <foo>	->	jmp	r13
