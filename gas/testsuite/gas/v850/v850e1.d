#objdump: -dr --prefix-addresses --show-raw-insn
#name: V850E1 instruction tests
#as: -mv850e1

# Test the new instructions in the V850E1 processor

.*: +file format .*v850.*

Disassembly of section .text:
0x0+00 e0 0f 42 13 [ 	]*bsh	r1, r2
0x0+04 e0 1f 40 23 [ 	]*bsw	sp, gp
0x0+08 05 02  [ 	]*callt	5
0x0+0a e8 3f e4 00 [ 	]*clr1	r7, r8
0x0+0e f6 17 14 1b [ 	]*cmov	nz, -10, r2, sp
0x0+12 e1 17 34 1b [ 	]*cmov	nz, r1, r2, sp
0x0+16 e0 07 44 01 [ 	]*ctret	
0x0+1a e0 07 46 01 [ 	]*dbret	
0x0+1e 40 f8  [ 	]*dbtrap	
0x0+20 4e 06 00 80 [ 	]*dispose	7, {r24}, r0
0x0+24 4e 06 05 70 [ 	]*dispose	7, {r25 - r27}, r5
0x0+28 e1 17 c0 1a [ 	]*div	r1, r2, sp
0x0+2c e4 2f 80 32 [ 	]*divh	gp, r5, r6
0x0+30 e7 47 82 4a [ 	]*divhu	r7, r8, r9
0x0+34 ea 5f c2 62 [ 	]*divu	r10, r11, r12
0x0+38 e0 6f 44 73 [ 	]*hsw	r13, r14
0x0+3c a1 17 0d 00 [ 	]*ld.bu	13\[r1\],r2
0x0+40 e3 27 11 00 [ 	]*ld.hu	16\[sp\],gp
0x0+44 21 06 78 56 34 12 [ 	]*mov	0x12345678, r1
0x0+4a e5 17 40 1a [ 	]*mul	5, r2, sp
0x0+4e e1 17 20 1a [ 	]*mul	r1, r2, sp
0x0+52 e4 2f 22 32 [ 	]*mulu	gp, r5, r6
0x0+56 e3 2f 46 32 [ 	]*mulu	35, r5, r6
0x0+5a ea 4f e2 00 [ 	]*not1	r9, r10
0x0+5e a8 07 01 80 [ 	]*prepare	{r24}, 20
0x0+62 a8 07 03 70 [ 	]*prepare	{r25 - r27}, 20, sp
0x0+66 e1 4f e0 00 [ 	]*set1	r9, r1
0x0+6a ea 47 00 02 [ 	]*sasf	nz, r8
0x0+6e 60 20  [ 	]*sld.bu	0\[ep\],gp
0x0+70 77 28  [ 	]*sld.hu	14\[ep\],r5
0x0+72 a1 00  [ 	]*sxb	r1
0x0+74 e2 00  [ 	]*sxh	r2
0x0+76 ff 07 e6 00 [ 	]*tst1	r0, lp
0x0+7a 83 00  [ 	]*zxb	sp
0x0+7c c4 00  [ 	]*zxh	gp
