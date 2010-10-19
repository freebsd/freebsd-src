#objdump: -dr --prefix-addresses
#name: MIPS macro rol/ror
#as: -32

# Test the rol and ror macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> negu	at,a1
0+0004 <[^>]*> srlv	at,a0,at
0+0008 <[^>]*> sllv	a0,a0,a1
0+000c <[^>]*> or	a0,a0,at
0+0010 <[^>]*> negu	at,a2
0+0014 <[^>]*> srlv	at,a1,at
0+0018 <[^>]*> sllv	a0,a1,a2
0+001c <[^>]*> or	a0,a0,at
0+0020 <[^>]*> sll	at,a0,0x1
0+0024 <[^>]*> srl	a0,a0,0x1f
0+0028 <[^>]*> or	a0,a0,at
0+002c <[^>]*> sll	at,a1,0x1
0+0030 <[^>]*> srl	a0,a1,0x1f
0+0034 <[^>]*> or	a0,a0,at
0+0038 <[^>]*> srl	a0,a1,0x0
0+003c <[^>]*> negu	at,a1
0+0040 <[^>]*> sllv	at,a0,at
0+0044 <[^>]*> srlv	a0,a0,a1
0+0048 <[^>]*> or	a0,a0,at
0+004c <[^>]*> negu	at,a2
0+0050 <[^>]*> sllv	at,a1,at
0+0054 <[^>]*> srlv	a0,a1,a2
0+0058 <[^>]*> or	a0,a0,at
0+005c <[^>]*> srl	at,a0,0x1
0+0060 <[^>]*> sll	a0,a0,0x1f
0+0064 <[^>]*> or	a0,a0,at
0+0068 <[^>]*> srl	at,a1,0x1
0+006c <[^>]*> sll	a0,a1,0x1f
0+0070 <[^>]*> or	a0,a0,at
0+0074 <[^>]*> srl	a0,a1,0x0
0+0078 <[^>]*> srl	a0,a1,0x0
0+007c <[^>]*> sll	at,a1,0x1
0+0080 <[^>]*> srl	a0,a1,0x1f
0+0084 <[^>]*> or	a0,a0,at
0+0088 <[^>]*> sll	at,a1,0x1f
0+008c <[^>]*> srl	a0,a1,0x1
0+0090 <[^>]*> or	a0,a0,at
0+0094 <[^>]*> srl	a0,a1,0x0
0+0098 <[^>]*> srl	at,a1,0x1
0+009c <[^>]*> sll	a0,a1,0x1f
0+00a0 <[^>]*> or	a0,a0,at
0+00a4 <[^>]*> srl	at,a1,0x1f
0+00a8 <[^>]*> sll	a0,a1,0x1
0+00ac <[^>]*> or	a0,a0,at
	...
