#as:
#objdump: -dr
#name: store

.*: +file format .*

Disassembly of section .text:

00000000 <l_sw>:
   0:	d7 e1 0f fc 	l.sw -4\(r1\),r1

00000004 <l_lw>:
   4:	80 21 ff 9c 	l.lw r1,-100\(r1\)
