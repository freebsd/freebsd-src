#objdump: -dz --prefix-addresses -m mips:4120
#as: -32 -march=vr4120 -mfix-vr4120
#name: MIPS vr4120 workarounds

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> macc	a0,a1,a2
0+0004 <[^>]*> nop
0+0008 <[^>]*> div	zero,a3,t0
0+000c <[^>]*> or	a0,a0,a1
0+0010 <[^>]*> dmacc	a0,a1,a2
0+0014 <[^>]*> nop
0+0018 <[^>]*> div	zero,a3,t0
0+001c <[^>]*> or	a0,a0,a1
0+0020 <[^>]*> macc	a0,a1,a2
0+0024 <[^>]*> nop
0+0028 <[^>]*> divu	zero,a3,t0
0+002c <[^>]*> or	a0,a0,a1
0+0030 <[^>]*> dmacc	a0,a1,a2
0+0034 <[^>]*> nop
0+0038 <[^>]*> divu	zero,a3,t0
0+003c <[^>]*> or	a0,a0,a1
0+0040 <[^>]*> macc	a0,a1,a2
0+0044 <[^>]*> nop
0+0048 <[^>]*> ddiv	zero,a3,t0
0+004c <[^>]*> or	a0,a0,a1
0+0050 <[^>]*> dmacc	a0,a1,a2
0+0054 <[^>]*> nop
0+0058 <[^>]*> ddiv	zero,a3,t0
0+005c <[^>]*> or	a0,a0,a1
0+0060 <[^>]*> macc	a0,a1,a2
0+0064 <[^>]*> nop
0+0068 <[^>]*> ddivu	zero,a3,t0
0+006c <[^>]*> or	a0,a0,a1
0+0070 <[^>]*> dmacc	a0,a1,a2
0+0074 <[^>]*> nop
0+0078 <[^>]*> ddivu	zero,a3,t0
0+007c <[^>]*> or	a0,a0,a1
0+0080 <[^>]*> dmult	a0,a1
0+0084 <[^>]*> nop
0+0088 <[^>]*> dmult	a2,a3
0+008c <[^>]*> or	a0,a0,a1
0+0090 <[^>]*> dmultu	a0,a1
0+0094 <[^>]*> nop
0+0098 <[^>]*> dmultu	a2,a3
0+009c <[^>]*> or	a0,a0,a1
0+00a0 <[^>]*> dmacc	a0,a1,a2
0+00a4 <[^>]*> nop
0+00a8 <[^>]*> dmacc	a2,a3,t0
0+00ac <[^>]*> or	a0,a0,a1
0+00b0 <[^>]*> dmult	a0,a1
0+00b4 <[^>]*> nop
0+00b8 <[^>]*> dmacc	a2,a3,t0
0+00bc <[^>]*> or	a0,a0,a1
0+00c0 <[^>]*> macc	a0,a1,a2
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> mtlo	a3
0+00cc <[^>]*> dmacc	a0,a1,a2
0+00d0 <[^>]*> nop
0+00d4 <[^>]*> mtlo	a3
0+00d8 <[^>]*> macc	a0,a1,a2
0+00dc <[^>]*> nop
0+00e0 <[^>]*> mthi	a3
0+00e4 <[^>]*> dmacc	a0,a1,a2
0+00e8 <[^>]*> nop
0+00ec <[^>]*> mthi	a3
#...
