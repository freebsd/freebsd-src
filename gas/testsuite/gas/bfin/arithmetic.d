#objdump: -dr
#name: arithmetic
.*: +file format .*


Disassembly of section .text:

00000000 <abs>:
   0:	10 c4 [0-3][[:xdigit:]] 00 	A0= ABS A0;
   4:	10 c4 [0-3][[:xdigit:]] 40 	A0= ABS A1;
   8:	30 c4 [0-3][[:xdigit:]] 00 	A1= ABS A0;
   c:	30 c4 [0-3][[:xdigit:]] 40 	A1= ABS A1;
  10:	10 c4 [0-3][[:xdigit:]] c0 	A1= ABS A0,A0= ABS A0;
  14:	07 c4 10 80 	R0= ABS R2;

00000018 <add>:
  18:	86 5b       	SP=SP\+P0;
  1a:	96 5b       	SP=SP\+P2;
  1c:	f9 5b       	FP=P1\+FP;
  1e:	04 c4 3a 0e 	R7=R7\+R2 \(NS\);
  22:	04 c4 30 2c 	R6=R6\+R0 \(S\);
  26:	02 c4 10 a8 	R4.L=R2.H\+R0.L \(S\);
  2a:	22 c4 09 aa 	R5.H=R1.H\+R1.L \(S\);
  2e:	02 c4 35 0c 	R6.L=R6.L\+R5.L \(NS\);

00000032 <add_sub_prescale_down>:
  32:	05 c4 01 98 	R4.L=R0\+R1\(RND20\);
  36:	25 c4 28 96 	R3.H=R5\+R0\(RND20\);
  3a:	05 c4 3d d2 	R1.L=R7-R5\(RND20\);

0000003e <add_sub_prescale_up>:
  3e:	05 c4 01 04 	R2.L=R0\+R1\(RND12\);
  42:	25 c4 3e 0e 	R7.H=R7\+R6\(RND12\);
  46:	05 c4 1a 4a 	R5.L=R3-R2\(RND12\);
  4a:	25 c4 0a 44 	R2.H=R1-R2\(RND12\);

0000004e <add_immediate>:
  4e:	05 66       	R5\+=-64;
  50:	fa 65       	R2\+=0x3f;
  52:	60 9f       	I0\+=2;
  54:	63 9f       	I3\+=2;
  56:	6a 9f       	I2\+=4;
  58:	69 9f       	I1\+=4;
  5a:	20 6c       	P0\+=0x4;
  5c:	86 6c       	SP\+=0x10;
  5e:	07 6f       	FP\+=-32;

00000060 <divide_primitive>:
  60:	6b 42       	DIVS\(R3,R5\);
  62:	2b 42       	DIVQ\(R3,R5\);

00000064 <expadj>:
  64:	07 c6 25 0c 	R6.L=EXPADJ \(R5,R4.L\);
  68:	07 c6 08 ca 	R5.L=EXPADJ \(R0.H,R1.L\);
  6c:	07 c6 2b 48 	R4.L=EXPADJ \(R3,R5.L\) \(V\);

00000070 <max>:
  70:	07 c4 2a 0c 	R6=MAX\(R5,R2\);
  74:	07 c4 0b 00 	R0=MAX\(R1,R3\);

00000078 <min>:
  78:	07 c4 13 4a 	R5=MIN\(R2,R3\);
  7c:	07 c4 38 48 	R4=MIN\(R7,R0\);

00000080 <modify_decrement>:
  80:	0b c4 [0-3][[:xdigit:]] c0 	A0-=A1;
  84:	0b c4 [0-3][[:xdigit:]] e0 	A0-=A1\(W32\);
  88:	17 44       	FP-=P2;
  8a:	06 44       	SP-=P0;
  8c:	73 9e       	I3-=M0;
  8e:	75 9e       	I1-=M1;

00000090 <modify_increment>:
  90:	0b c4 [0-3][[:xdigit:]] 80 	A0\+=A1;
  94:	0b c4 [0-3][[:xdigit:]] a0 	A0\+=A1\(W32\);
  98:	4e 45       	SP\+=P1\(BREV\);
  9a:	7d 45       	P5\+=FP\(BREV\);
  9c:	6a 9e       	I2\+=M2;
  9e:	e0 9e       	I0\+=M0\(BREV\);
  a0:	0b c4 [0-3][[:xdigit:]] 0e 	R7=\(A0\+=A1\);
  a4:	0b c4 [0-3][[:xdigit:]] 4c 	R6.L=\(A0\+=A1\);
  a8:	2b c4 [0-3][[:xdigit:]] 40 	R0.H=\(A0\+=A1\);

000000ac <multiply16>:
  ac:	00 c2 0a 24 	R0 = R1.H \* R2.L;
  b0:	20 c2 68 26 	R1 = R5.H \* R0.H \(S2RND\);
  b4:	80 c2 db 23 	R7 = R3.L \* R3.H \(FU\);
  b8:	28 c3 15 27 	R4 = R2.H \* R5.H \(ISS2\);
  bc:	08 c3 0b 20 	R0 = R1.L \* R3.L \(IS\);
  c0:	08 c2 a8 25 	R6 = R5.H \* R0.L;
  c4:	94 c3 be 40 	R2.H = R7.L \* R6.H \(M, IU\);
  c8:	04 c2 e8 80 	R3.H = R5.H \* R0.L;
  cc:	14 c2 09 40 	R0.H = R1.L \* R1.H \(M\);
  d0:	1c c3 3e 80 	R1 = R7.H \* R6.L \(M, IS\);
  d4:	0c c2 02 41 	R5 = R0.L \* R2.H;
  d8:	1c c2 b0 c0 	R3 = R6.H \* R0.H \(M\);

000000dc <multiply32>:
  dc:	c4 40       	R4\*=R0;
  de:	d7 40       	R7\*=R2;

000000e0 <multiply_accumulate>:
  e0:	63 c0 2f 02 	a0 = R5.L \* R7.H \(W32\);
  e4:	03 c0 00 04 	a0 = R0.H \* R0.L;
  e8:	83 c0 13 0a 	a0 \+= R2.L \* R3.H \(FU\);
  ec:	03 c0 21 0c 	a0 \+= R4.H \* R1.L;
  f0:	03 c1 3e 12 	a0 -= R7.L \* R6.H \(IS\);
  f4:	03 c0 2a 16 	a0 -= R5.H \* R2.H;
  f8:	10 c0 08 58 	a1 = R1.L \* R0.H \(M\);
  fc:	00 c0 10 98 	a1 = R2.H \* R0.L;
 100:	70 c0 3e 98 	a1 = R7.H \* R6.L \(M, W32\);
 104:	81 c0 1a 18 	a1 \+= R3.L \* R2.L \(FU\);
 108:	01 c0 31 98 	a1 \+= R6.H \* R1.L;
 10c:	02 c1 03 58 	a1 -= R0.L \* R3.H \(IS\);
 110:	02 c0 17 58 	a1 -= R2.L \* R7.H;

00000114 <multiply_accumulate_half>:
 114:	03 c0 f5 25 	R7.L = \(a0 = R6.H \* R5.L\);
 118:	c3 c0 0a 24 	R0.L = \(a0 = R1.H \* R2.L\) \(TFU\);
 11c:	03 c0 ac 28 	R2.L = \(a0 \+= R5.L \* R4.L\);
 120:	43 c0 fe 2e 	R3.L = \(a0 \+= R7.H \* R6.H\) \(T\);
 124:	03 c0 1a 36 	R0.L = \(a0 -= R3.H \* R2.H\);
 128:	63 c1 6c 30 	R1.L = \(a0 -= R5.L \* R4.L\) \(IH\);
 12c:	04 c0 48 58 	R1.H = \(a1 = R1.L \* R0.H\);
 130:	34 c1 83 98 	R2.H = \(a1 = R0.H \* R3.L\) \(M, ISS2\);
 134:	05 c0 bf 59 	R6.H = \(a1 \+= R7.L \* R7.H\);
 138:	25 c0 d3 19 	R7.H = \(a1 \+= R2.L \* R3.L\) \(S2RND\);
 13c:	06 c0 a2 d9 	R6.H = \(a1 -= R4.H \* R2.H\);
 140:	d6 c0 5f 99 	R5.H = \(a1 -= R3.H \* R7.L\) \(M, TFU\);

00000144 <multiply_accumulate_data_reg>:
 144:	0b c0 0a 20 	R0 = \(a0 = R1.L \* R2.L\);
 148:	0b c1 8a 20 	R2 = \(a0 = R1.L \* R2.L\) \(IS\);
 14c:	0b c0 3e 2d 	R4 = \(a0 \+= R7.H \* R6.L\);
 150:	2b c0 ab 2b 	R6 = \(a0 \+= R5.L \* R3.H\) \(S2RND\);
 154:	0b c0 97 35 	R6 = \(a0 -= R2.H \* R7.L\);
 158:	8b c0 06 33 	R4 = \(a0 -= R0.L \* R6.H\) \(FU\);
 15c:	0c c0 81 99 	R7 = \(a1 = R0.H \* R1.L\);
 160:	9c c0 13 d9 	R5 = \(a1 = R2.H \* R3.H\) \(M, FU\);
 164:	0d c0 bd 18 	R3 = \(a1 \+= R7.L \* R5.L\);
 168:	2d c1 17 d8 	R1 = \(a1 \+= R2.H \* R7.H\) \(ISS2\);
 16c:	0e c0 80 58 	R3 = \(a1 -= R0.L \* R0.H\);
 170:	1e c1 17 59 	R5 = \(a1 -= R2.L \* R7.H\) \(M, IS\);

00000174 <negate>:
 174:	85 43       	R5=-R0;
 176:	07 c4 10 ee 	R7=-R2\(S\);
 17a:	07 c4 10 ce 	R7=-R2\(NS\);
 17e:	0e c4 [0-3][[:xdigit:]] 00 	A0=-A0;
 182:	0e c4 [0-3][[:xdigit:]] 40 	A0=-A1;
 186:	2e c4 [0-3][[:xdigit:]] 00 	A1=-A0;
 18a:	2e c4 [0-3][[:xdigit:]] 40 	A1=-A1;
 18e:	0e c4 [0-3][[:xdigit:]] c0 	A1=-A1,A0=-A0;

00000192 <round_half>:
 192:	0c c4 18 ca 	R5.L=R3\(RND\);
 196:	2c c4 00 cc 	R6.H=R0\(RND\);

0000019a <saturate>:
 19a:	08 c4 [0-3][[:xdigit:]] 20 	A0=A0\(S\);
 19e:	08 c4 [0-3][[:xdigit:]] 60 	A1=A1\(S\);
 1a2:	08 c4 [0-3][[:xdigit:]] a0 	A1=A1\(S\),A0=A0\(S\);

000001a6 <signbits>:
 1a6:	05 c6 00 0a 	R5.L=SIGNBITS R0;
 1aa:	05 c6 07 80 	R0.L=SIGNBITS R7.H;
 1ae:	06 c6 00 06 	R3.L=SIGNBITS A0;
 1b2:	06 c6 00 4e 	R7.L=SIGNBITS A1;

000001b6 <subtract>:
 1b6:	43 53       	R5=R3-R0;
 1b8:	04 c4 38 6e 	R7=R7-R0 \(S\);
 1bc:	04 c4 11 46 	R3=R2-R1 \(NS\);
 1c0:	03 c4 37 ea 	R5.L=R6.H-R7.H \(S\);
 1c4:	23 c4 1b 40 	R0.H=R3.L-R3.H \(NS\);

000001c8 <subtract_immediate>:
 1c8:	66 9f       	I2-=2;
 1ca:	6c 9f       	I0-=4;
