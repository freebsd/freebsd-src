#objdump: -dr --prefix-addresses -mmips:4650
#name: MIPS 4650
#as: -32 -march=4650 -mtune=4650


.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <stuff> mad	a0,a1
	...
0+000c <stuff\+0xc> madu	a1,a2
	...
0+0018 <stuff\+0x18> mul	a2,a3,t0
0+001c <stuff\+0x1c> nop
