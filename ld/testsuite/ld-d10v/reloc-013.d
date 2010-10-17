#source: reloc-005.s
#ld: -T $srcdir/$subdir/reloc-013.ld
#objdump: -D

# Test 18 bit pc rel reloc negative normal case

.*:     file format elf32-d10v

Disassembly of section .text:

01014000 <foo>:
	...

01014400 <_start>:
 1014400:	e4 00 ff 00 	bra.l	1014000 <foo>
 1014404:	26 0d 5e 00 	jmp	r13	||	nop	
