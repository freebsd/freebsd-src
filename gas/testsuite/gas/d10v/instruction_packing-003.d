#objdump: -Dr
#source: instruction_packing-003.s
#as: -W

.*:     file format elf32-d10v

Disassembly of section s1:

00000000 <foo>:
   0:	01 1b 5e 00 	add	r1, r11	||	nop	
   4:	01 2b 02 76 	add	r2, r11	||	add	r3, r11
Disassembly of section s2:

00000000 <bar>:
   0:	01 4b 02 b6 	add	r4, r11	||	add	r5, r11
