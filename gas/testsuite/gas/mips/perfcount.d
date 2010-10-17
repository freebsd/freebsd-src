#objdump: -dr --prefix-addresses -mmips:10000
#name: MIPS R1[20]000 performance counters
#as: -mips4 -march=r10000

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> mtps	a0,0
0+0004 <[^>]*> mfps	a0,1
0+0008 <[^>]*> mtpc	a0,1
0+000c <[^>]*> mfpc	a0,0
