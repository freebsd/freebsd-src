#as: -64 -Av9
#objdump: -dr
#name: sparc64 window

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	81 88 00 00 	saved 
   4:	83 88 00 00 	restored 
   8:	85 88 00 00 	allclean 
   c:	87 88 00 00 	otherw 
  10:	89 88 00 00 	normalw 
  14:	8b 88 00 00 	invalw 
