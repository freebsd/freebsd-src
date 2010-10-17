#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS div
#source: div.s

# Test the div macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> div	zero,a0,a1
0+0004 <[^>]*> bnez	a1,0+0010 <foo\+0x10>
0+0008 <[^>]*> div	zero,a0,a1
0+000c <[^>]*> break	(0x0,0x7|0x7)
0+0010 <[^>]*> li	at,-1
0+0014 <[^>]*> bne	a1,at,0+0028 <foo\+0x28>
0+0018 <[^>]*> lui	at,0x8000
0+001c <[^>]*> bne	a0,at,0+0028 <foo\+0x28>
0+0020 <[^>]*> nop
0+0024 <[^>]*> break	(0x0,0x6|0x6)
0+0028 <[^>]*> mflo	a0
0+002c <[^>]*> bnez	a2,0+0038 <foo\+0x38>
0+0030 <[^>]*> div	zero,a1,a2
0+0034 <[^>]*> break	(0x0,0x7|0x7)
0+0038 <[^>]*> li	at,-1
0+003c <[^>]*> bne	a2,at,0+0050 <foo\+0x50>
0+0040 <[^>]*> lui	at,0x8000
0+0044 <[^>]*> bne	a1,at,0+0050 <foo\+0x50>
0+0048 <[^>]*> nop
0+004c <[^>]*> break	(0x0,0x6|0x6)
0+0050 <[^>]*> mflo	a0
0+0054 <[^>]*> move	a0,a0
0+0058 <[^>]*> move	a0,a1
0+005c <[^>]*> neg	a0,a0
0+0060 <[^>]*> neg	a0,a1
0+0064 <[^>]*> li	at,2
0+0068 <[^>]*> div	zero,a0,at
0+006c <[^>]*> mflo	a0
0+0070 <[^>]*> li	at,2
0+0074 <[^>]*> div	zero,a1,at
0+0078 <[^>]*> mflo	a0
0+007c <[^>]*> li	at,0x8000
0+0080 <[^>]*> div	zero,a0,at
0+0084 <[^>]*> mflo	a0
0+0088 <[^>]*> li	at,0x8000
0+008c <[^>]*> div	zero,a1,at
0+0090 <[^>]*> mflo	a0
0+0094 <[^>]*> li	at,-32768
0+0098 <[^>]*> div	zero,a0,at
0+009c <[^>]*> mflo	a0
0+00a0 <[^>]*> li	at,-32768
0+00a4 <[^>]*> div	zero,a1,at
0+00a8 <[^>]*> mflo	a0
0+00ac <[^>]*> lui	at,0x1
0+00b0 <[^>]*> div	zero,a0,at
0+00b4 <[^>]*> mflo	a0
0+00b8 <[^>]*> lui	at,0x1
0+00bc <[^>]*> div	zero,a1,at
0+00c0 <[^>]*> mflo	a0
0+00c4 <[^>]*> lui	at,0x1
0+00c8 <[^>]*> ori	at,at,0xa5a5
0+00cc <[^>]*> div	zero,a0,at
0+00d0 <[^>]*> mflo	a0
0+00d4 <[^>]*> lui	at,0x1
0+00d8 <[^>]*> ori	at,at,0xa5a5
0+00dc <[^>]*> div	zero,a1,at
0+00e0 <[^>]*> mflo	a0
0+00e4 <[^>]*> divu	zero,a0,a1
0+00e8 <[^>]*> bnez	a1,0+0f4 <foo\+0xf4>
0+00ec <[^>]*> divu	zero,a0,a1
0+00f0 <[^>]*> break	(0x0,0x7|0x7)
0+00f4 <[^>]*> mflo	a0
0+00f8 <[^>]*> bnez	a2,0+0104 <foo\+0x104>
0+00fc <[^>]*> divu	zero,a1,a2
0+0100 <[^>]*> break	(0x0,0x7|0x7)
0+0104 <[^>]*> mflo	a0
0+0108 <[^>]*> move	a0,a0
0+010c <[^>]*> bnez	a2,0+0118 <foo\+0x118>
0+0110 <[^>]*> div	zero,a1,a2
0+0114 <[^>]*> break	(0x0,0x7|0x7)
0+0118 <[^>]*> li	at,-1
0+011c <[^>]*> bne	a2,at,0+0130 <foo\+0x130>
0+0120 <[^>]*> lui	at,0x8000
0+0124 <[^>]*> bne	a1,at,0+0130 <foo\+0x130>
0+0128 <[^>]*> nop
0+012c <[^>]*> break	(0x0,0x6|0x6)
0+0130 <[^>]*> mfhi	a0
0+0134 <[^>]*> li	at,2
0+0138 <[^>]*> divu	zero,a1,at
0+013c <[^>]*> mfhi	a0
0+0140 <[^>]*> bnez	a2,0+014c <foo\+0x14c>
0+0144 <[^>]*> ddiv	zero,a1,a2
0+0148 <[^>]*> break	(0x0,0x7|0x7)
0+014c <[^>]*> daddiu	at,zero,-1
0+0150 <[^>]*> bne	a2,at,0+0168 <foo\+0x168>
0+0154 <[^>]*> daddiu	at,zero,1
0+0158 <[^>]*> dsll32	at,at,0x1f
0+015c <[^>]*> bne	a1,at,0+0168 <foo\+0x168>
0+0160 <[^>]*> nop
0+0164 <[^>]*> break	(0x0,0x6|0x6)
0+0168 <[^>]*> mflo	a0
0+016c <[^>]*> li	at,2
0+0170 <[^>]*> ddivu	zero,a1,at
0+0174 <[^>]*> mflo	a0
0+0178 <[^>]*> li	at,0x8000
0+017c <[^>]*> ddiv	zero,a1,at
0+0180 <[^>]*> mfhi	a0
0+0184 <[^>]*> li	at,-32768
0+0188 <[^>]*> ddivu	zero,a1,at
0+018c <[^>]*> mfhi	a0
	...
