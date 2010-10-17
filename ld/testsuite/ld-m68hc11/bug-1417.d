#source: bug-1417.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32-m68hc11

Disassembly of section .text:
0+8000 <_start> tst	0+ <__bss_size>
0+8003 <_start\+0x3> bne	0+8007 <L1>
0+8005 <_start\+0x5> bsr	0+800b <foo>
0+8007 <L1> bset	\*0+ <__bss_size> \#\$04
0+800a <L2> rts
0+800b <foo> rts
