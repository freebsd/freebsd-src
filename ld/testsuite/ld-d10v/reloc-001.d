#source: reloc-001.s
#ld: -T $srcdir/$subdir/reloc-001.ld
#objdump: -D

# Test 10 bit pc rel reloc normal case

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <_start>:
 1014000:	65 20 cc 1a 	brf0f.s	1014104 <foo>	->	jmp	r13
