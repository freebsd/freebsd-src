#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS trunc
#as: -32 -mips1 -mtune=r3000

# Test the trunc macros.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> cfc1	a0,\$31
0+0004 <[^>]*> cfc1	a0,\$31
0+0008 <[^>]*> nop
0+000c <[^>]*> ori	at,a0,0x3
0+0010 <[^>]*> xori	at,at,0x2
0+0014 <[^>]*> ctc1	at,\$31
0+0018 <[^>]*> nop
0+001c <[^>]*> cvt.w.d	\$f4,\$f6
0+0020 <[^>]*> ctc1	a0,\$31
0+0024 <[^>]*> nop
0+0028 <[^>]*> cfc1	a0,\$31
0+002c <[^>]*> cfc1	a0,\$31
0+0030 <[^>]*> nop
0+0034 <[^>]*> ori	at,a0,0x3
0+0038 <[^>]*> xori	at,at,0x2
0+003c <[^>]*> ctc1	at,\$31
0+0040 <[^>]*> nop
0+0044 <[^>]*> cvt.w.s	\$f4,\$f6
0+0048 <[^>]*> ctc1	a0,\$31
0+004c <[^>]*> nop
