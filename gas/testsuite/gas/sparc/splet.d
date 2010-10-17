#as: -Asparclet
#objdump: -dr
#name: sparclet extensions

.*: +file format .*

Disassembly of section .text:

0+ <start>:
   0:	a1 40 00 00 	rd  %y, %l0
   4:	a1 40 40 00 	rd  %asr1, %l0
   8:	a1 43 c0 00 	rd  %asr15, %l0
   c:	a1 44 40 00 	rd  %asr17, %l0
  10:	a1 44 80 00 	rd  %asr18, %l0
  14:	a1 44 c0 00 	rd  %asr19, %l0
  18:	a1 45 00 00 	rd  %asr20, %l0
  1c:	a1 45 40 00 	rd  %asr21, %l0
  20:	a1 45 80 00 	rd  %asr22, %l0
  24:	81 84 20 00 	mov  %l0, %y
  28:	83 84 20 00 	mov  %l0, %asr1
  2c:	9f 84 20 00 	mov  %l0, %asr15
  30:	a3 84 20 00 	mov  %l0, %asr17
  34:	a5 84 20 00 	mov  %l0, %asr18
  38:	a7 84 20 00 	mov  %l0, %asr19
  3c:	a9 84 20 00 	mov  %l0, %asr20
  40:	ab 84 20 00 	mov  %l0, %asr21
  44:	ad 84 20 00 	mov  %l0, %asr22

0+48 <test_umul>:
  48:	86 50 40 02 	umul  %g1, %g2, %g3
  4c:	86 50 40 02 	umul  %g1, %g2, %g3

0+50 <test_smul>:
  50:	86 58 40 02 	smul  %g1, %g2, %g3
  54:	86 58 40 02 	smul  %g1, %g2, %g3

0+58 <test_stbar>:
  58:	81 43 c0 00 	stbar 
  5c:	81 43 c0 00 	stbar 
  60:	00 00 00 01 	unimp  0x1
  64:	81 dc 40 00 	flush  %l1

0+68 <test_scan>:
  68:	a7 64 7f ff 	scan  %l1, -1, %l3
  6c:	a7 64 60 00 	scan  %l1, 0, %l3
  70:	a7 64 40 11 	scan  %l1, %l1, %l3

0+74 <test_shuffle>:
  74:	a3 6c 20 01 	shuffle  %l0, 1, %l1
  78:	a3 6c 20 02 	shuffle  %l0, 2, %l1
  7c:	a3 6c 20 04 	shuffle  %l0, 4, %l1
  80:	a3 6c 20 08 	shuffle  %l0, 8, %l1
  84:	a3 6c 20 10 	shuffle  %l0, 0x10, %l1
  88:	a3 6c 20 18 	shuffle  %l0, 0x18, %l1

0+8c <test_umac>:
  8c:	a1 f4 40 12 	umac  %l1, %l2, %l0
  90:	a1 f4 60 02 	umac  %l1, 2, %l0
  94:	a1 f4 60 02 	umac  %l1, 2, %l0

0+98 <test_umacd>:
  98:	a1 74 80 14 	umacd  %l2, %l4, %l0
  9c:	a1 74 a0 03 	umacd  %l2, 3, %l0
  a0:	a1 74 a0 03 	umacd  %l2, 3, %l0

0+a4 <test_smac>:
  a4:	a1 fc 40 12 	smac  %l1, %l2, %l0
  a8:	a1 fc 7f d6 	smac  %l1, -42, %l0
  ac:	a1 fc 7f d6 	smac  %l1, -42, %l0

0+b0 <test_smacd>:
  b0:	a1 7c 80 14 	smacd  %l2, %l4, %l0
  b4:	a1 7c a0 7b 	smacd  %l2, 0x7b, %l0
  b8:	a1 7c a0 7b 	smacd  %l2, 0x7b, %l0

0+bc <test_umuld>:
  bc:	90 4a 80 0c 	umuld  %o2, %o4, %o0
  c0:	90 4a a2 34 	umuld  %o2, 0x234, %o0
  c4:	90 4a a5 67 	umuld  %o2, 0x567, %o0

0+c8 <test_smuld>:
  c8:	b0 6e 80 1c 	smuld  %i2, %i4, %i0
  cc:	b0 6e b0 00 	smuld  %i2, -4096, %i0
  d0:	b0 6f 2f ff 	smuld  %i4, 0xfff, %i0

0+d4 <test_coprocessor>:
  d4:	81 b4 00 11 	cpush  %l0, %l1
  d8:	81 b4 20 01 	cpush  %l0, 1
  dc:	81 b4 00 51 	cpusha  %l0, %l1
  e0:	81 b4 20 41 	cpusha  %l0, 1
  e4:	a1 b0 00 80 	cpull  %l0
  e8:	a1 b0 01 00 	crdcxt  %ccsr, %l0
  ec:	a1 b0 41 00 	crdcxt  %ccfr, %l0
  f0:	a1 b0 c1 00 	crdcxt  %ccpr, %l0
  f4:	a1 b0 81 00 	crdcxt  %cccrcr, %l0
  f8:	81 b4 00 c0 	cwrcxt  %l0, %ccsr
  fc:	83 b4 00 c0 	cwrcxt  %l0, %ccfr
 100:	87 b4 00 c0 	cwrcxt  %l0, %ccpr
 104:	85 b4 00 c0 	cwrcxt  %l0, %cccrcr
 108:	01 c0 00 01 	cbn  10c <test_coprocessor\+(0x|)38>
			108: WDISP22	stop\+0xfffffef8
 10c:	01 00 00 00 	nop 
 110:	21 c0 00 01 	cbn,a   114 <test_coprocessor\+(0x|)40>
			110: WDISP22	stop\+0xfffffef0
 114:	01 00 00 00 	nop 
 118:	03 c0 00 01 	cbe  11c <test_coprocessor\+(0x|)48>
			118: WDISP22	stop\+0xfffffee8
 11c:	01 00 00 00 	nop 
 120:	23 c0 00 01 	cbe,a   124 <test_coprocessor\+(0x|)50>
			120: WDISP22	stop\+0xfffffee0
 124:	01 00 00 00 	nop 
 128:	05 c0 00 01 	cbf  12c <test_coprocessor\+(0x|)58>
			128: WDISP22	stop\+0xfffffed8
 12c:	01 00 00 00 	nop 
 130:	25 c0 00 01 	cbf,a   134 <test_coprocessor\+(0x|)60>
			130: WDISP22	stop\+0xfffffed0
 134:	01 00 00 00 	nop 
 138:	07 c0 00 01 	cbef  13c <test_coprocessor\+(0x|)68>
			138: WDISP22	stop\+0xfffffec8
 13c:	01 00 00 00 	nop 
 140:	27 c0 00 01 	cbef,a   144 <test_coprocessor\+(0x|)70>
			140: WDISP22	stop\+0xfffffec0
 144:	01 00 00 00 	nop 
 148:	09 c0 00 01 	cbr  14c <test_coprocessor\+(0x|)78>
			148: WDISP22	stop\+0xfffffeb8
 14c:	01 00 00 00 	nop 
 150:	29 c0 00 01 	cbr,a   154 <test_coprocessor\+(0x|)80>
			150: WDISP22	stop\+0xfffffeb0
 154:	01 00 00 00 	nop 
 158:	0b c0 00 01 	cber  15c <test_coprocessor\+(0x|)88>
			158: WDISP22	stop\+0xfffffea8
 15c:	01 00 00 00 	nop 
 160:	2b c0 00 01 	cber,a   164 <test_coprocessor\+(0x|)90>
			160: WDISP22	stop\+0xfffffea0
 164:	01 00 00 00 	nop 
 168:	0d c0 00 01 	cbfr  16c <test_coprocessor\+(0x|)98>
			168: WDISP22	stop\+0xfffffe98
 16c:	01 00 00 00 	nop 
 170:	2d c0 00 01 	cbfr,a   174 <test_coprocessor\+(0x|)a0>
			170: WDISP22	stop\+0xfffffe90
 174:	01 00 00 00 	nop 
 178:	0f c0 00 01 	cbefr  17c <test_coprocessor\+(0x|)a8>
			178: WDISP22	stop\+0xfffffe88
 17c:	01 00 00 00 	nop 
 180:	2f c0 00 01 	cbefr,a   184 <test_coprocessor\+(0x|)b0>
			180: WDISP22	stop\+0xfffffe80
 184:	01 00 00 00 	nop 
 188:	11 c0 00 01 	cba  18c <test_coprocessor\+(0x|)b8>
			188: WDISP22	stop\+0xfffffe78
 18c:	01 00 00 00 	nop 
 190:	31 c0 00 01 	cba,a   194 <test_coprocessor\+(0x|)c0>
			190: WDISP22	stop\+0xfffffe70
 194:	01 00 00 00 	nop 
 198:	13 c0 00 01 	cbne  19c <test_coprocessor\+(0x|)c8>
			198: WDISP22	stop\+0xfffffe68
 19c:	01 00 00 00 	nop 
 1a0:	33 c0 00 01 	cbne,a   1a4 <test_coprocessor\+(0x|)d0>
			1a0: WDISP22	stop\+0xfffffe60
 1a4:	01 00 00 00 	nop 
 1a8:	15 c0 00 01 	cbnf  1ac <test_coprocessor\+(0x|)d8>
			1a8: WDISP22	stop\+0xfffffe58
 1ac:	01 00 00 00 	nop 
 1b0:	35 c0 00 01 	cbnf,a   1b4 <test_coprocessor\+(0x|)e0>
			1b0: WDISP22	stop\+0xfffffe50
 1b4:	01 00 00 00 	nop 
 1b8:	17 c0 00 01 	cbnef  1bc <test_coprocessor\+(0x|)e8>
			1b8: WDISP22	stop\+0xfffffe48
 1bc:	01 00 00 00 	nop 
 1c0:	37 c0 00 01 	cbnef,a   1c4 <test_coprocessor\+(0x|)f0>
			1c0: WDISP22	stop\+0xfffffe40
 1c4:	01 00 00 00 	nop 
 1c8:	19 c0 00 01 	cbnr  1cc <test_coprocessor\+(0x|)f8>
			1c8: WDISP22	stop\+0xfffffe38
 1cc:	01 00 00 00 	nop 
 1d0:	39 c0 00 01 	cbnr,a   1d4 <test_coprocessor\+(0x|)100>
			1d0: WDISP22	stop\+0xfffffe30
 1d4:	01 00 00 00 	nop 
 1d8:	1b c0 00 01 	cbner  1dc <test_coprocessor\+(0x|)108>
			1d8: WDISP22	stop\+0xfffffe28
 1dc:	01 00 00 00 	nop 
 1e0:	3b c0 00 01 	cbner,a   1e4 <test_coprocessor\+(0x|)110>
			1e0: WDISP22	stop\+0xfffffe20
 1e4:	01 00 00 00 	nop 
 1e8:	1d c0 00 01 	cbnfr  1ec <test_coprocessor\+(0x|)118>
			1e8: WDISP22	stop\+0xfffffe18
 1ec:	01 00 00 00 	nop 
 1f0:	3d c0 00 01 	cbnfr,a   1f4 <test_coprocessor\+(0x|)120>
			1f0: WDISP22	stop\+0xfffffe10
 1f4:	01 00 00 00 	nop 
 1f8:	1f c0 00 01 	cbnefr  1fc <test_coprocessor\+(0x|)128>
			1f8: WDISP22	stop\+0xfffffe08
 1fc:	01 00 00 00 	nop 
 200:	3f c0 00 01 	cbnefr,a   204 <test_coprocessor\+(0x|)130>
			200: WDISP22	stop\+0xfffffe00
 204:	01 00 00 00 	nop 
