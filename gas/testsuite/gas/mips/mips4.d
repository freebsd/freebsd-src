#objdump: -dr --prefix-addresses
#name: MIPS mips4

# Test the mips4 macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> bc1f	00000000+ <text_label>
0+0004 <[^>]*> nop
0+0008 <[^>]*> bc1f	\$fcc1,00000000+ <text_label>
0+000c <[^>]*> nop
0+0010 <[^>]*> bc1fl	\$fcc1,00000000+ <text_label>
0+0014 <[^>]*> nop
0+0018 <[^>]*> bc1t	\$fcc1,00000000+ <text_label>
0+001c <[^>]*> nop
0+0020 <[^>]*> bc1tl	\$fcc2,00000000+ <text_label>
0+0024 <[^>]*> nop
0+0028 <[^>]*> c.f.d	\$f4,\$f6
0+002c <[^>]*> c.f.d	\$fcc1,\$f4,\$f6
0+0030 <[^>]*> ldxc1	\$f2,a0\(a1\)
0+0034 <[^>]*> lwxc1	\$f2,a0\(a1\)
0+0038 <[^>]*> madd.d	\$f0,\$f2,\$f4,\$f6
0+003c <[^>]*> madd.s	\$f0,\$f2,\$f4,\$f6
0+0040 <[^>]*> movf	a0,a1,\$fcc4
0+0044 <[^>]*> movf.d	\$f4,\$f6,\$fcc0
0+0048 <[^>]*> movf.s	\$f4,\$f6,\$fcc0
0+004c <[^>]*> movn	a0,a2,a2
0+0050 <[^>]*> movn.d	\$f4,\$f6,a2
0+0054 <[^>]*> movn.s	\$f4,\$f6,a2
0+0058 <[^>]*> movt	a0,a1,\$fcc4
0+005c <[^>]*> movt.d	\$f4,\$f6,\$fcc0
0+0060 <[^>]*> movt.s	\$f4,\$f6,\$fcc0
0+0064 <[^>]*> movz	a0,a2,a2
0+0068 <[^>]*> movz.d	\$f4,\$f6,a2
0+006c <[^>]*> movz.s	\$f4,\$f6,a2
0+0070 <[^>]*> msub.d	\$f0,\$f2,\$f4,\$f6
0+0074 <[^>]*> msub.s	\$f0,\$f2,\$f4,\$f6
0+0078 <[^>]*> nmadd.d	\$f0,\$f2,\$f4,\$f6
0+007c <[^>]*> nmadd.s	\$f0,\$f2,\$f4,\$f6
0+0080 <[^>]*> nmsub.d	\$f0,\$f2,\$f4,\$f6
0+0084 <[^>]*> nmsub.s	\$f0,\$f2,\$f4,\$f6
0+0088 <[^>]*> prefx	0x4,a0\(a1\)
0+008c <[^>]*> recip.d	\$f4,\$f6
0+0090 <[^>]*> recip.s	\$f4,\$f6
0+0094 <[^>]*> rsqrt.d	\$f4,\$f6
0+0098 <[^>]*> rsqrt.s	\$f4,\$f6
0+009c <[^>]*> sdxc1	\$f4,a0\(a1\)
0+00a0 <[^>]*> swxc1	\$f4,a0\(a1\)
	...
