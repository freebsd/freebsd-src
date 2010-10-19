#objdump: -dr
#name: vector
.*: +file format .*

Disassembly of section .text:

00000000 <add_on_sign>:
   0:	0c c4 0d 08 	R4.H=R4.L=SIGN\(R1.H\)\*R5.H\+SIGN\(R1.L\)\*R5.L\);

00000004 <vit_max>:
   4:	09 c6 15 8e 	R7=VIT_MAX\(R5,R2\)\(ASL\);
   8:	09 c6 30 c0 	R0=VIT_MAX\(R0,R6\)\(ASR\);
   c:	09 c6 03 0a 	R5.L=VIT_MAX \(R3\) \(ASL\);
  10:	09 c6 02 44 	R2.L=VIT_MAX \(R2\) \(ASR\);

00000014 <vector_abs>:
  14:	06 c4 28 8a 	R5= ABS R5\(V\);
  18:	06 c4 00 84 	R2= ABS R0\(V\);

0000001c <vector_add_sub>:
  1c:	00 c4 1a 0a 	R5=R3\+\|\+R2 ;
  20:	00 c4 1a 3a 	R5=R3\+\|\+R2 \(SCO\);
  24:	00 c4 06 8e 	R7=R0-\|\+R6 ;
  28:	00 c4 0b a4 	R2=R1-\|\+R3 \(S\);
  2c:	00 c4 02 48 	R4=R0\+\|-R2 ;
  30:	00 c4 0a 5a 	R5=R1\+\|-R2 \(CO\);
  34:	00 c4 1c cc 	R6=R3-\|-R4 ;
  38:	00 c4 2e de 	R7=R5-\|-R6 \(CO\);
  3c:	01 c4 63 bf 	R5=R4\+\|\+R3,R7=R4-\|-R3\(SCO,ASR\);
  40:	01 c4 1e c2 	R0=R3\+\|\+R6,R1=R3-\|-R6\(ASL\);
  44:	21 c4 ca 2d 	R7=R1\+\|-R2,R6=R1-\|\+R2\(S\);
  48:	21 c4 53 0a 	R1=R2\+\|-R3,R5=R2-\|\+R3;
  4c:	04 c4 41 8d 	R5=R0\+R1,R6=R0-R1 \(NS\);
  50:	04 c4 39 a6 	R0=R7\+R1,R3=R7-R1 \(S\);
  54:	11 c4 [c-f][[:xdigit:]] 0b 	R7=A1\+A0,R5=A1-A0 \(NS\);
  58:	11 c4 [c-f][[:xdigit:]] 6c 	R3=A0\+A1,R6=A0-A1 \(S\);

0000005c <vector_ashift>:
  5c:	81 c6 8b 03 	R1=R3>>>0xf \(V\);
  60:	81 c6 e0 09 	R4=R0>>>0x4 \(V\);
  64:	81 c6 00 4a 	R5=R0<<0x0 \(V, S\);
  68:	81 c6 62 44 	R2=R2<<0xc \(V, S\);
  6c:	01 c6 15 0e 	R7= ASHIFT R5 BY R2.L\(V\);
  70:	01 c6 02 40 	R0= ASHIFT R2 BY R0.L\(V,S\);

00000074 <vector_lshift>:
  74:	81 c6 8a 8b 	R5=R2 >> 0xf \(V\);
  78:	81 c6 11 80 	R0=R1<<0x2 \(V\);
  7c:	01 c6 11 88 	R4=SHIFT R1 BY R2.L\(V\);

00000080 <vector_max>:
  80:	06 c4 01 0c 	R6=MAX\(R0,R1\)\(V\);

00000084 <vector_min>:
  84:	06 c4 17 40 	R0=MIN\(R2,R7\)\(V\);

00000088 <vector_mul>:
  88:	04 c2 be 66 	R2.H = R7.L \* R6.H, R2 = R7.H \* R6.H;
  8c:	04 c2 08 e1 	R4.H = R1.H \* R0.H, R4 = R1.L \* R0.L;
  90:	04 c2 1a a0 	R0.H = R3.H \* R2.L, R0 = R3.L \* R2.L;
  94:	94 c2 5a e1 	R5.H = R3.H \* R2.H \(M\), R5 = R3.L \* R2.L \(FU\);
  98:	2c c2 27 e0 	R1 = R4.H \* R7.H, R0 = R4.L \* R7.L \(S2RND\);
  9c:	0c c2 95 27 	R7 = R2.L \* R5.L, R6 = R2.H \* R5.H;
  a0:	24 c3 3e e0 	R0.H = R7.H \* R6.H, R0 = R7.L \* R6.L \(ISS2\);
  a4:	04 c3 c1 e0 	R3.H = R0.H \* R1.H, R3 = R0.L \* R1.L \(IS\);
  a8:	00 c0 13 46 	a1 = R2.L \* R3.H, a0 = R2.H \* R3.H;
  ac:	01 c0 08 c0 	a1 \+= R1.H \* R0.H, a0 = R1.L \* R0.L;
  b0:	60 c0 2f c8 	a1 = R5.H \* R7.H, a0 \+= R5.L \* R7.L \(W32\);
  b4:	01 c1 01 c0 	a1 \+= R0.H \* R1.H, a0 = R0.L \* R1.L \(IS\);
  b8:	90 c0 1c c8 	a1 = R3.H \* R4.H \(M\), a0 \+= R3.L \* R4.L \(FU\);
  bc:	01 c0 24 96 	a1 \+= R4.H \* R4.L, a0 -= R4.H \* R4.H;
  c0:	25 c1 3e e8 	R0.H = \(a1 \+= R7.H \* R6.H\), R0.L = \(a0 \+= R7.L \* R6.L\) \(ISS2\);
  c4:	27 c0 81 28 	R2.H = A1, R2.L = \(a0 \+= R0.L \* R1.L\) \(S2RND\);
  c8:	04 c0 d1 c9 	R7.H = \(a1 = R2.H \* R1.H\), a0 \+= R2.L \* R1.L;
  cc:	04 c0 be 66 	R2.H = \(a1 = R7.L \* R6.H\), R2.L = \(a0 = R7.H \* R6.H\);
  d0:	05 c0 9a e1 	R6.H = \(a1 \+= R3.H \* R2.H\), R6.L = \(a0 = R3.L \* R2.L\);
  d4:	05 c0 f5 a7 	R7.H = \(a1 \+= R6.H \* R5.L\), R7.L = \(a0 = R6.H \* R5.H\);
  d8:	14 c0 3c a8 	R0.H = \(a1 = R7.H \* R4.L\) \(M\), R0.L = \(a0 \+= R7.L \* R4.L\);
  dc:	94 c0 5a e9 	R5.H = \(a1 = R3.H \* R2.H\) \(M\), R5.L = \(a0 \+= R3.L \* R2.L\) \(FU\);
  e0:	05 c1 1a e0 	R0.H = \(a1 \+= R3.H \* R2.H\), R0.L = \(a0 = R3.L \* R2.L\) \(IS\);
  e4:	1c c0 b7 d0 	R3 = \(a1 = R6.H \* R7.H\) \(M\), a0 -= R6.L \* R7.L;
  e8:	1c c0 3c 2e 	R1 = \(a1 = R7.L \* R4.L\) \(M\), R0 = \(a0 \+= R7.H \* R4.H\);
  ec:	2d c1 3e e8 	R1 = \(a1 \+= R7.H \* R6.H\), R0 = \(a0 \+= R7.L \* R6.L\) \(ISS2\);
  f0:	0d c0 37 e1 	R5 = \(a1 \+= R6.H \* R7.H\), R4 = \(a0 = R6.L \* R7.L\);
  f4:	0d c0 9d f1 	R7 = \(a1 \+= R3.H \* R5.H\), R6 = \(a0 -= R3.L \* R5.L\);
  f8:	0e c0 37 c9 	R5 = \(a1 -= R6.H \* R7.H\), a0 \+= R6.L \* R7.L;
  fc:	0c c0 b7 e0 	R3 = \(a1 = R6.H \* R7.H\), R2 = \(a0 = R6.L \* R7.L\);
 100:	9c c0 1f e9 	R5 = \(a1 = R3.H \* R7.H\) \(M\), R4 = \(a0 \+= R3.L \* R7.L\) \(FU\);
 104:	2f c0 81 28 	R3 = A1, R2 = \(a0 \+= R0.L \* R1.L\) \(S2RND\);
 108:	0d c1 1a e0 	R1 = \(a1 \+= R3.H \* R2.H\), R0 = \(a0 = R3.L \* R2.L\) \(IS\);

0000010c <vector_negate>:
 10c:	0f c4 08 c0 	R0=-R1\(V\);
 110:	0f c4 10 ce 	R7=-R2\(V\);

00000114 <vector_pack>:
 114:	04 c6 08 8e 	R7=PACK\(R0.H,R1.L\);
 118:	04 c6 31 cc 	R6=PACK\(R1.H,R6.H\);
 11c:	04 c6 12 4a 	R5=PACK\(R2.L,R2.H\);

00000120 <vector_search>:
 120:	0d c4 10 82 	\(R0,R1\) = SEARCH R2\(LT\);
 124:	0d c4 80 cf 	\(R6,R7\) = SEARCH R0\(LE\);
 128:	0d c4 c8 0c 	\(R3,R6\) = SEARCH R1\(GT\);
 12c:	0d c4 18 4b 	\(R4,R5\) = SEARCH R3\(GE\);
