#name: mode5
#objdump: -d
#as: 

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
   0:	2213           	movel %a3@,%d1
   2:	2882           	movel %d2,%a4@
   4:	2295           	movel %a5@,%a1@
	...
