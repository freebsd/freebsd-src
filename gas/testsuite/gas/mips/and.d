#objdump: -dr --prefix-addresses
#name: MIPS and
#as: -32

# Test the and macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> andi	a0,a0,0x0
0+0004 <[^>]*> andi	a0,a0,0x1
0+0008 <[^>]*> andi	a0,a0,0x8000
0+000c <[^>]*> li	at,-32768
0+0010 <[^>]*> and	a0,a0,at
0+0014 <[^>]*> lui	at,0x1
0+0018 <[^>]*> and	a0,a0,at
0+001c <[^>]*> lui	at,0x1
0+0020 <[^>]*> ori	at,at,0xa5a5
0+0024 <[^>]*> and	a0,a0,at
0+0028 <[^>]*> ori	a0,a1,0x0
0+002c <[^>]*> nor	a0,a0,zero
0+0030 <[^>]*> ori	a0,a1,0x1
0+0034 <[^>]*> nor	a0,a0,zero
0+0038 <[^>]*> ori	a0,a1,0x8000
0+003c <[^>]*> nor	a0,a0,zero
0+0040 <[^>]*> li	at,-32768
0+0044 <[^>]*> nor	a0,a1,at
0+0048 <[^>]*> lui	at,0x1
0+004c <[^>]*> nor	a0,a1,at
0+0050 <[^>]*> lui	at,0x1
0+0054 <[^>]*> ori	at,at,0xa5a5
0+0058 <[^>]*> nor	a0,a1,at
0+005c <[^>]*> ori	a0,a1,0x0
0+0060 <[^>]*> xori	a0,a1,0x0
	...
