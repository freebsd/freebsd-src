#as: -march=r4000
#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS 20-bit trap

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> teq	zero,v1
0+0004 <[^>]*> teq	zero,v1,0x1
0+0008 <[^>]*> tge	zero,v1
0+000c <[^>]*> tge	zero,v1,0x3
0+0010 <[^>]*> tgeu	zero,v1
0+0014 <[^>]*> tgeu	zero,v1,0x7
0+0018 <[^>]*> tlt	zero,v1
0+001c <[^>]*> tlt	zero,v1,0x1f
0+0020 <[^>]*> tltu	zero,v1
0+0024 <[^>]*> tltu	zero,v1,0xff
0+0028 <[^>]*> tne	zero,v1
0+002c <[^>]*> tne	zero,v1,0x3ff
	...
