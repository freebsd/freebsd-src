#objdump: -dr --prefix-addresses
#name: MIPS hardware drol/dror
#source: rol64.s
#stderr: rol64-hw.l

# Test the drol and dror macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> dnegu	at,a1
0+0004 <[^>]*> drorv	a0,a0,at
0+0008 <[^>]*> dnegu	a0,a2
0+000c <[^>]*> drorv	a0,a1,a0
0+0010 <[^>]*> dror32	a0,a0,0x1f
0+0014 <[^>]*> dror	a0,a1,0x0
0+0018 <[^>]*> dror32	a0,a1,0x1f
0+001c <[^>]*> dror32	a0,a1,0x1
0+0020 <[^>]*> dror32	a0,a1,0x0
0+0024 <[^>]*> dror	a0,a1,0x1f
0+0028 <[^>]*> dror	a0,a1,0x1
0+002c <[^>]*> dror	a0,a1,0x0
0+0030 <[^>]*> drorv	a0,a0,a1
0+0034 <[^>]*> drorv	a0,a1,a2
0+0038 <[^>]*> dror	a0,a0,0x1
0+003c <[^>]*> dror	a0,a1,0x0
0+0040 <[^>]*> dror	a0,a1,0x1
0+0044 <[^>]*> dror	a0,a1,0x1f
0+0048 <[^>]*> dror32	a0,a1,0x0
0+004c <[^>]*> dror32	a0,a1,0x1
0+0050 <[^>]*> dror32	a0,a1,0x1f
0+0054 <[^>]*> dror	a0,a1,0x0
0+0058 <[^>]*> dror32	a0,a1,0x1f
0+005c <[^>]*> dror32	a0,a1,0x1
0+0060 <[^>]*> dror32	a0,a1,0x0
0+0064 <[^>]*> dror	a0,a1,0x1f
0+0068 <[^>]*> dror	a0,a1,0x1
0+006c <[^>]*> dror	a0,a1,0x1
0+0070 <[^>]*> dror	a0,a1,0x1f
0+0074 <[^>]*> dror32	a0,a1,0x0
0+0078 <[^>]*> dror32	a0,a1,0x1
0+007c <[^>]*> dror32	a0,a1,0x1f
	...
