#as: -32 -march=r3900
#objdump: -dr --prefix-addresses -mmips:3900
#name: MIPS 20-bit break

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> break
0+0004 <[^>]*> break
0+0008 <[^>]*> break	0x14
0+000c <[^>]*> break	0x14,0x28
0+0010 <[^>]*> break	0x3ff,0x3ff
0+0014 <[^>]*> sdbbp
0+0018 <[^>]*> sdbbp
0+001c <[^>]*> sdbbp	0x14
0+0020 <[^>]*> sdbbp	0x14,0x28
0+0024 <[^>]*> sdbbp	0x3ff,0x3ff
	...
