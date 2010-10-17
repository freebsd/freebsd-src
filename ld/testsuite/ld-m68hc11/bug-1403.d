#source: bug-1403.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32-m68hc11

Disassembly of section .text:
0+8000 <_start> bset	\*0+ <__bss_size> \#\$04
0+8003 <L1> bra	0+8005 <toto>
0+8005 <toto> rts
