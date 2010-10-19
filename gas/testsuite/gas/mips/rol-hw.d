#objdump: -dr --prefix-addresses
#name: MIPS hardware rol/ror
#source: rol.s
#as: -32

# Test the rol and ror macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> negu	at,a1
0+0004 <[^>]*> rorv	a0,a0,at
0+0008 <[^>]*> negu	a0,a2
0+000c <[^>]*> rorv	a0,a1,a0
0+0010 <[^>]*> ror	a0,a0,0x1f
0+0014 <[^>]*> ror	a0,a1,0x1f
0+0018 <[^>]*> ror	a0,a1,0x0
0+001c <[^>]*> rorv	a0,a0,a1
0+0020 <[^>]*> rorv	a0,a1,a2
0+0024 <[^>]*> ror	a0,a0,0x1
0+0028 <[^>]*> ror	a0,a1,0x1
0+002c <[^>]*> ror	a0,a1,0x0
0+0030 <[^>]*> ror	a0,a1,0x0
0+0034 <[^>]*> ror	a0,a1,0x1f
0+0038 <[^>]*> ror	a0,a1,0x1
0+003c <[^>]*> ror	a0,a1,0x0
0+0040 <[^>]*> ror	a0,a1,0x1
0+0044 <[^>]*> ror	a0,a1,0x1f
	...
