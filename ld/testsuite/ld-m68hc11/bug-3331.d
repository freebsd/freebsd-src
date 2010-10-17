#source: bug-3331.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32-m68hc11

Disassembly of section .text:
0+8000 <_start> ldx	#0+1100 <__data_section_start>
0+8003 <_start\+0x3> bset	0,x \#\$04
0+8006 <L1> ldd	\#0+2 <__bss_size\+0x2>
0+8009 <L1\+0x3> std	\*0+ <__bss_size>
0+800b <L1\+0x5> rts
