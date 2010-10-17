#objdump: -ldr
#name: D30V debug (-g) test
#as: -gstabs

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <.text>:
.*:[0-9]+
   0:	08001083 00f00000 	add.s	r1, r2, r3	||	nop	
.*:[0-9]+
   8:	08001083 00f00000 	add.s	r1, r2, r3	||	nop	
.*:[0-9]+
  10:	18001083 00f00000 	add.s/tx	r1, r2, r3	||	nop	
.*:[0-9]+
  18:	28001083 00f00000 	add.s/fx	r1, r2, r3	||	nop	
.*:[0-9]+
  20:	38001083 00f00000 	add.s/xt	r1, r2, r3	||	nop	
.*:[0-9]+
  28:	48001083 00f00000 	add.s/xf	r1, r2, r3	||	nop	
.*:[0-9]+
  30:	58001083 00f00000 	add.s/tt	r1, r2, r3	||	nop	
.*:[0-9]+
  38:	68001083 00f00000 	add.s/tf	r1, r2, r3	||	nop	
