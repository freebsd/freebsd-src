#as: -march=r4000 -mtune=r4000
#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS div

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
0+002c <[^>]*> nop
0+0030 <[^>]*> bnez	a2,0+003c <foo\+0x3c>
0+0034 <[^>]*> div	zero,a1,a2
0+0038 <[^>]*> break	(0x0,0x7|0x7)
0+003c <[^>]*> li	at,-1
0+0040 <[^>]*> bne	a2,at,0+0054 <foo\+0x54>
0+0044 <[^>]*> lui	at,0x8000
0+0048 <[^>]*> bne	a1,at,0+0054 <foo\+0x54>
0+004c <[^>]*> nop
0+0050 <[^>]*> break	(0x0,0x6|0x6)
0+0054 <[^>]*> mflo	a0
0+0058 <[^>]*> move	a0,a0
0+005c <[^>]*> move	a0,a1
0+0060 <[^>]*> neg	a0,a0
0+0064 <[^>]*> neg	a0,a1
0+0068 <[^>]*> li	at,2
0+006c <[^>]*> div	zero,a0,at
0+0070 <[^>]*> mflo	a0
0+0074 <[^>]*> li	at,2
0+0078 <[^>]*> nop
0+007c <[^>]*> div	zero,a1,at
0+0080 <[^>]*> mflo	a0
0+0084 <[^>]*> li	at,0x8000
0+0088 <[^>]*> nop
0+008c <[^>]*> div	zero,a0,at
0+0090 <[^>]*> mflo	a0
0+0094 <[^>]*> li	at,0x8000
0+0098 <[^>]*> nop
0+009c <[^>]*> div	zero,a1,at
0+00a0 <[^>]*> mflo	a0
0+00a4 <[^>]*> li	at,-32768
0+00a8 <[^>]*> nop
0+00ac <[^>]*> div	zero,a0,at
0+00b0 <[^>]*> mflo	a0
0+00b4 <[^>]*> li	at,-32768
0+00b8 <[^>]*> nop
0+00bc <[^>]*> div	zero,a1,at
0+00c0 <[^>]*> mflo	a0
0+00c4 <[^>]*> lui	at,0x1
0+00c8 <[^>]*> nop
0+00cc <[^>]*> div	zero,a0,at
0+00d0 <[^>]*> mflo	a0
0+00d4 <[^>]*> lui	at,0x1
0+00d8 <[^>]*> nop
0+00dc <[^>]*> div	zero,a1,at
0+00e0 <[^>]*> mflo	a0
0+00e4 <[^>]*> lui	at,0x1
0+00e8 <[^>]*> ori	at,at,0xa5a5
0+00ec <[^>]*> div	zero,a0,at
0+00f0 <[^>]*> mflo	a0
0+00f4 <[^>]*> lui	at,0x1
0+00f8 <[^>]*> ori	at,at,0xa5a5
0+00fc <[^>]*> div	zero,a1,at
0+0100 <[^>]*> mflo	a0
	...
0+010c <[^>]*> divu	zero,a0,a1
0+0110 <[^>]*> bnez	a1,0+011c <foo\+0x11c>
0+0114 <[^>]*> divu	zero,a0,a1
0+0118 <[^>]*> break	(0x0,0x7|0x7)
0+011c <[^>]*> mflo	a0
0+0120 <[^>]*> nop
0+0124 <[^>]*> bnez	a2,0+0130 <foo\+0x130>
0+0128 <[^>]*> divu	zero,a1,a2
0+012c <[^>]*> break	(0x0,0x7|0x7)
0+0130 <[^>]*> mflo	a0
0+0134 <[^>]*> move	a0,a0
0+0138 <[^>]*> bnez	a2,0+0144 <foo\+0x144>
0+013c <[^>]*> div	zero,a1,a2
0+0140 <[^>]*> break	(0x0,0x7|0x7)
0+0144 <[^>]*> li	at,-1
0+0148 <[^>]*> bne	a2,at,0+015c <foo\+0x15c>
0+014c <[^>]*> lui	at,0x8000
0+0150 <[^>]*> bne	a1,at,0+015c <foo\+0x15c>
0+0154 <[^>]*> nop
0+0158 <[^>]*> break	(0x0,0x6|0x6)
0+015c <[^>]*> mfhi	a0
0+0160 <[^>]*> li	at,2
0+0164 <[^>]*> nop
0+0168 <[^>]*> divu	zero,a1,at
0+016c <[^>]*> mfhi	a0
0+0170 <[^>]*> nop
0+0174 <[^>]*> bnez	a2,0+0180 <foo\+0x180>
0+0178 <[^>]*> ddiv	zero,a1,a2
0+017c <[^>]*> break	(0x0,0x7|0x7)
0+0180 <[^>]*> (daddiu	at,zero,-1|li	at,-1)
0+0184 <[^>]*> bne	a2,at,0+019c <foo\+0x19c>
0+0188 <[^>]*> (daddiu	at,zero,1|li	at,1)
0+018c <[^>]*> dsll32	at,at,0x1f
0+0190 <[^>]*> bne	a1,at,0+019c <foo\+0x19c>
0+0194 <[^>]*> nop
0+0198 <[^>]*> break	(0x0,0x6|0x6)
0+019c <[^>]*> mflo	a0
0+01a0 <[^>]*> li	at,2
0+01a4 <[^>]*> nop
0+01a8 <[^>]*> ddivu	zero,a1,at
0+01ac <[^>]*> mflo	a0
0+01b0 <[^>]*> li	at,0x8000
0+01b4 <[^>]*> nop
0+01b8 <[^>]*> ddiv	zero,a1,at
0+01bc <[^>]*> mfhi	a0
0+01c0 <[^>]*> li	at,-32768
0+01c4 <[^>]*> nop
0+01c8 <[^>]*> ddivu	zero,a1,at
0+01cc <[^>]*> mfhi	a0
	...
