#objdump: -dr --prefix-addresses --show-raw-insn
#name: SH DSP basic instructions
#as: -dsp
# Test the SH DSP instructions:

.*: +file format .*sh.*

Disassembly of section .text:
0+000 <[^>]*> f6 80 [ 	]*movs.w	@-r2,x0
0+002 <[^>]*> f7 94 [ 	]*movs.w	@r3,x1
0+004 <[^>]*> f4 a8 [ 	]*movs.w	@r4\+,y0
0+006 <[^>]*> f5 bc [ 	]*movs.w	@r5\+r8,y1
0+008 <[^>]*> f5 c1 [ 	]*movs.w	m0,@-r5
0+00a <[^>]*> f4 e5 [ 	]*movs.w	m1,@r4
0+00c <[^>]*> f7 79 [ 	]*movs.w	a0,@r3\+
0+00e <[^>]*> f6 5d [ 	]*movs.w	a1,@r2\+r8
0+010 <[^>]*> f6 f2 [ 	]*movs.l	@-r2,a0g
0+012 <[^>]*> f7 d6 [ 	]*movs.l	@r3,a1g
0+014 <[^>]*> f4 8a [ 	]*movs.l	@r4\+,x0
0+016 <[^>]*> f5 9e [ 	]*movs.l	@r5\+r8,x1
0+018 <[^>]*> f5 a3 [ 	]*movs.l	y0,@-r5
0+01a <[^>]*> f4 b7 [ 	]*movs.l	y1,@r4
0+01c <[^>]*> f7 cb [ 	]*movs.l	m0,@r3\+
0+01e <[^>]*> f6 ef [ 	]*movs.l	m1,@r2\+r8
0+020 <[^>]*> f8 00 b1 07 [ 	]padd	x0,y0,a0
0+024 <[^>]*> f8 00 ed 07 [ 	]plds	a0,mach
0+028 <[^>]*> f8 00 b1 07 [ 	]padd	x0,y0,a0
