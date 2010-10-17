#objdump: -dr --prefix-addresses
#name: MIPS mul-ilocks
#as: -march=r4000 -mtune=r4000
#source: mul.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> multu	a0,a1
0+0004 <[^>]*> mflo	a0
0+0008 <[^>]*> multu	a1,a2
0+000c <[^>]*> mflo	a0
0+0010 <[^>]*> li	at,0
0+0014 <[^>]*> mult	a1,at
0+0018 <[^>]*> mflo	a0
0+001c <[^>]*> li	at,1
0+0020 <[^>]*> mult	a1,at
0+0024 <[^>]*> mflo	a0
0+0028 <[^>]*> li	at,0x8000
0+002c <[^>]*> mult	a1,at
0+0030 <[^>]*> mflo	a0
0+0034 <[^>]*> li	at,-32768
0+0038 <[^>]*> mult	a1,at
0+003c <[^>]*> mflo	a0
0+0040 <[^>]*> lui	at,0x1
0+0044 <[^>]*> mult	a1,at
0+0048 <[^>]*> mflo	a0
0+004c <[^>]*> lui	at,0x1
0+0050 <[^>]*> ori	at,at,0xa5a5
0+0054 <[^>]*> mult	a1,at
0+0058 <[^>]*> mflo	a0
0+005c <[^>]*> mult	a0,a1
0+0060 <[^>]*> mflo	a0
0+0064 <[^>]*> sra	a0,a0,0x1f
0+0068 <[^>]*> mfhi	at
0+006c <[^>]*> beq	a0,at,0+78 <foo\+(0x|)78>
0+0070 <[^>]*> nop
0+0074 <[^>]*> break	(0x0,0x6|0x6)
0+0078 <[^>]*> mflo	a0
0+007c <[^>]*> mult	a1,a2
0+0080 <[^>]*> mflo	a0
0+0084 <[^>]*> sra	a0,a0,0x1f
0+0088 <[^>]*> mfhi	at
0+008c <[^>]*> beq	a0,at,0+98 <foo\+(0x|)98>
0+0090 <[^>]*> nop
0+0094 <[^>]*> break	(0x0,0x6|0x6)
0+0098 <[^>]*> mflo	a0
0+009c <[^>]*> multu	a0,a1
0+00a0 <[^>]*> mfhi	at
0+00a4 <[^>]*> mflo	a0
0+00a8 <[^>]*> beqz	at,0+b4 <foo\+(0x|)b4>
0+00ac <[^>]*> nop
0+00b0 <[^>]*> break	(0x0,0x6|0x6)
0+00b4 <[^>]*> multu	a1,a2
0+00b8 <[^>]*> mfhi	at
0+00bc <[^>]*> mflo	a0
0+00c0 <[^>]*> beqz	at,0+cc <foo\+(0x|)cc>
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> break	(0x0,0x6|0x6)
0+00cc <[^>]*> dmultu	a1,a2
0+00d0 <[^>]*> mflo	a0
0+00d4 <[^>]*> li	at,1
0+00d8 <[^>]*> dmult	a1,at
0+00dc <[^>]*> mflo	a0
0+00e0 <[^>]*> dmult	a1,a2
0+00e4 <[^>]*> mflo	a0
0+00e8 <[^>]*> dsra32	a0,a0,0x1f
0+00ec <[^>]*> mfhi	at
0+00f0 <[^>]*> beq	a0,at,0+fc <foo\+(0x|)fc>
0+00f4 <[^>]*> nop
0+00f8 <[^>]*> break	(0x0,0x6|0x6)
0+00fc <[^>]*> mflo	a0
0+0100 <[^>]*> dmultu	a1,a2
0+0104 <[^>]*> mfhi	at
0+0108 <[^>]*> mflo	a0
0+010c <[^>]*> beqz	at,0+118 <foo\+(0x|)118>
0+0110 <[^>]*> nop
0+0114 <[^>]*> break	(0x0,0x6|0x6)
	...
