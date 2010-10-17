#objdump: -Dr
#source: instruction_packing-004.s
#as: -W

.*:     file format elf32-d10v

Disassembly of section s1:

00000000 <foo>:
   0:	01 1b 5e 00 	add	r1, r11	||	nop	

00000004 <bar>:
   4:	01 2b 02 76 	add	r2, r11	||	add	r3, r11
