#objdump: -dw
#name: i386 katmai

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 58 01 [ 	]*addps  \(%ecx\),%xmm0
   3:	0f 58 ca [ 	]*addps  %xmm2,%xmm1
   6:	f3 0f 58 13 [ 	]*addss  \(%ebx\),%xmm2
   a:	f3 0f 58 dc [ 	]*addss  %xmm4,%xmm3
   e:	0f 55 65 00 [ 	]*andnps 0x0\(%ebp\),%xmm4
  12:	0f 55 ee [ 	]*andnps %xmm6,%xmm5
  15:	0f 54 37 [ 	]*andps  \(%edi\),%xmm6
  18:	0f 54 f8 [ 	]*andps  %xmm0,%xmm7
  1b:	0f c2 c1 02 [ 	]*cmpleps %xmm1,%xmm0
  1f:	0f c2 0a 03 [ 	]*cmpunordps \(%edx\),%xmm1
  23:	f3 0f c2 d2 04 [ 	]*cmpneqss %xmm2,%xmm2
  28:	f3 0f c2 1c 24 05 [ 	]*cmpnltss \(%esp\),%xmm3
  2e:	0f c2 e5 06 [ 	]*cmpnleps %xmm5,%xmm4
  32:	0f c2 2e 07 [ 	]*cmpordps \(%esi\),%xmm5
  36:	f3 0f c2 f7 00 [ 	]*cmpeqss %xmm7,%xmm6
  3b:	f3 0f c2 38 01 [ 	]*cmpltss \(%eax\),%xmm7
  40:	0f c2 c1 00 [ 	]*cmpeqps %xmm1,%xmm0
  44:	0f c2 0a 00 [ 	]*cmpeqps \(%edx\),%xmm1
  48:	f3 0f c2 d2 00 [ 	]*cmpeqss %xmm2,%xmm2
  4d:	f3 0f c2 1c 24 00 [ 	]*cmpeqss \(%esp\),%xmm3
  53:	0f c2 e5 01 [ 	]*cmpltps %xmm5,%xmm4
  57:	0f c2 2e 01 [ 	]*cmpltps \(%esi\),%xmm5
  5b:	f3 0f c2 f7 01 [ 	]*cmpltss %xmm7,%xmm6
  60:	f3 0f c2 38 01 [ 	]*cmpltss \(%eax\),%xmm7
  65:	0f c2 01 02 [ 	]*cmpleps \(%ecx\),%xmm0
  69:	0f c2 ca 02 [ 	]*cmpleps %xmm2,%xmm1
  6d:	f3 0f c2 13 02 [ 	]*cmpless \(%ebx\),%xmm2
  72:	f3 0f c2 dc 02 [ 	]*cmpless %xmm4,%xmm3
  77:	0f c2 65 00 03 [ 	]*cmpunordps 0x0\(%ebp\),%xmm4
  7c:	0f c2 ee 03 [ 	]*cmpunordps %xmm6,%xmm5
  80:	f3 0f c2 37 03 [ 	]*cmpunordss \(%edi\),%xmm6
  85:	f3 0f c2 f8 03 [ 	]*cmpunordss %xmm0,%xmm7
  8a:	0f c2 c1 04 [ 	]*cmpneqps %xmm1,%xmm0
  8e:	0f c2 0a 04 [ 	]*cmpneqps \(%edx\),%xmm1
  92:	f3 0f c2 d2 04 [ 	]*cmpneqss %xmm2,%xmm2
  97:	f3 0f c2 1c 24 04 [ 	]*cmpneqss \(%esp\),%xmm3
  9d:	0f c2 e5 05 [ 	]*cmpnltps %xmm5,%xmm4
  a1:	0f c2 2e 05 [ 	]*cmpnltps \(%esi\),%xmm5
  a5:	f3 0f c2 f7 05 [ 	]*cmpnltss %xmm7,%xmm6
  aa:	f3 0f c2 38 05 [ 	]*cmpnltss \(%eax\),%xmm7
  af:	0f c2 01 06 [ 	]*cmpnleps \(%ecx\),%xmm0
  b3:	0f c2 ca 06 [ 	]*cmpnleps %xmm2,%xmm1
  b7:	f3 0f c2 13 06 [ 	]*cmpnless \(%ebx\),%xmm2
  bc:	f3 0f c2 dc 06 [ 	]*cmpnless %xmm4,%xmm3
  c1:	0f c2 65 00 07 [ 	]*cmpordps 0x0\(%ebp\),%xmm4
  c6:	0f c2 ee 07 [ 	]*cmpordps %xmm6,%xmm5
  ca:	f3 0f c2 37 07 [ 	]*cmpordss \(%edi\),%xmm6
  cf:	f3 0f c2 f8 07 [ 	]*cmpordss %xmm0,%xmm7
  d4:	0f 2f c1 [ 	]*comiss %xmm1,%xmm0
  d7:	0f 2f 0a [ 	]*comiss \(%edx\),%xmm1
  da:	0f 2a d3 [ 	]*cvtpi2ps %mm3,%xmm2
  dd:	0f 2a 1c 24 [ 	]*cvtpi2ps \(%esp\),%xmm3
  e1:	f3 0f 2a e5 [ 	]*cvtsi2ss %ebp,%xmm4
  e5:	f3 0f 2a 2e [ 	]*cvtsi2ss \(%esi\),%xmm5
  e9:	0f 2d f7 [ 	]*cvtps2pi %xmm7,%mm6
  ec:	0f 2d 38 [ 	]*cvtps2pi \(%eax\),%mm7
  ef:	f3 0f 2d 01 [ 	]*cvtss2si \(%ecx\),%eax
  f3:	f3 0f 2d ca [ 	]*cvtss2si %xmm2,%ecx
  f7:	0f 2c 13 [ 	]*cvttps2pi \(%ebx\),%mm2
  fa:	0f 2c dc [ 	]*cvttps2pi %xmm4,%mm3
  fd:	f3 0f 2c 65 00 [ 	]*cvttss2si 0x0\(%ebp\),%esp
 102:	f3 0f 2c ee [ 	]*cvttss2si %xmm6,%ebp
 106:	0f 5e c1 [ 	]*divps  %xmm1,%xmm0
 109:	0f 5e 0a [ 	]*divps  \(%edx\),%xmm1
 10c:	f3 0f 5e d3 [ 	]*divss  %xmm3,%xmm2
 110:	f3 0f 5e 1c 24 [ 	]*divss  \(%esp\),%xmm3
 115:	0f ae 55 00 [ 	]*ldmxcsr 0x0\(%ebp\)
 119:	0f ae 1e [ 	]*stmxcsr \(%esi\)
 11c:	0f ae f8 [ 	]*sfence 
 11f:	0f 5f c1 [ 	]*maxps  %xmm1,%xmm0
 122:	0f 5f 0a [ 	]*maxps  \(%edx\),%xmm1
 125:	f3 0f 5f d3 [ 	]*maxss  %xmm3,%xmm2
 129:	f3 0f 5f 1c 24 [ 	]*maxss  \(%esp\),%xmm3
 12e:	0f 5d e5 [ 	]*minps  %xmm5,%xmm4
 131:	0f 5d 2e [ 	]*minps  \(%esi\),%xmm5
 134:	f3 0f 5d f7 [ 	]*minss  %xmm7,%xmm6
 138:	f3 0f 5d 38 [ 	]*minss  \(%eax\),%xmm7
 13c:	0f 28 c1 [ 	]*movaps %xmm1,%xmm0
 13f:	0f 29 11 [ 	]*movaps %xmm2,\(%ecx\)
 142:	0f 28 12 [ 	]*movaps \(%edx\),%xmm2
 145:	0f 16 dc [ 	]*movlhps %xmm4,%xmm3
 148:	0f 17 2c 24 [ 	]*movhps %xmm5,\(%esp\)
 14c:	0f 16 2e [ 	]*movhps \(%esi\),%xmm5
 14f:	0f 12 f7 [ 	]*movhlps %xmm7,%xmm6
 152:	0f 13 07 [ 	]*movlps %xmm0,\(%edi\)
 155:	0f 12 00 [ 	]*movlps \(%eax\),%xmm0
 158:	0f 50 ca [ 	]*movmskps %xmm2,%ecx
 15b:	0f 10 d3 [ 	]*movups %xmm3,%xmm2
 15e:	0f 11 22 [ 	]*movups %xmm4,\(%edx\)
 161:	0f 10 65 00 [ 	]*movups 0x0\(%ebp\),%xmm4
 165:	f3 0f 10 ee [ 	]*movss  %xmm6,%xmm5
 169:	f3 0f 11 3e [ 	]*movss  %xmm7,\(%esi\)
 16d:	f3 0f 10 38 [ 	]*movss  \(%eax\),%xmm7
 171:	0f 59 c1 [ 	]*mulps  %xmm1,%xmm0
 174:	0f 59 0a [ 	]*mulps  \(%edx\),%xmm1
 177:	f3 0f 59 d2 [ 	]*mulss  %xmm2,%xmm2
 17b:	f3 0f 59 1c 24 [ 	]*mulss  \(%esp\),%xmm3
 180:	0f 56 e5 [ 	]*orps   %xmm5,%xmm4
 183:	0f 56 2e [ 	]*orps   \(%esi\),%xmm5
 186:	0f 53 f7 [ 	]*rcpps  %xmm7,%xmm6
 189:	0f 53 38 [ 	]*rcpps  \(%eax\),%xmm7
 18c:	f3 0f 53 01 [ 	]*rcpss  \(%ecx\),%xmm0
 190:	f3 0f 53 ca [ 	]*rcpss  %xmm2,%xmm1
 194:	0f 52 13 [ 	]*rsqrtps \(%ebx\),%xmm2
 197:	0f 52 dc [ 	]*rsqrtps %xmm4,%xmm3
 19a:	f3 0f 52 65 00 [ 	]*rsqrtss 0x0\(%ebp\),%xmm4
 19f:	f3 0f 52 ee [ 	]*rsqrtss %xmm6,%xmm5
 1a3:	0f c6 37 02 [ 	]*shufps \$0x2,\(%edi\),%xmm6
 1a7:	0f c6 f8 03 [ 	]*shufps \$0x3,%xmm0,%xmm7
 1ab:	0f 51 c1 [ 	]*sqrtps %xmm1,%xmm0
 1ae:	0f 51 0a [ 	]*sqrtps \(%edx\),%xmm1
 1b1:	f3 0f 51 d2 [ 	]*sqrtss %xmm2,%xmm2
 1b5:	f3 0f 51 1c 24 [ 	]*sqrtss \(%esp\),%xmm3
 1ba:	0f 5c e5 [ 	]*subps  %xmm5,%xmm4
 1bd:	0f 5c 2e [ 	]*subps  \(%esi\),%xmm5
 1c0:	f3 0f 5c f7 [ 	]*subss  %xmm7,%xmm6
 1c4:	f3 0f 5c 38 [ 	]*subss  \(%eax\),%xmm7
 1c8:	0f 2e 01 [ 	]*ucomiss \(%ecx\),%xmm0
 1cb:	0f 2e ca [ 	]*ucomiss %xmm2,%xmm1
 1ce:	0f 15 13 [ 	]*unpckhps \(%ebx\),%xmm2
 1d1:	0f 15 dc [ 	]*unpckhps %xmm4,%xmm3
 1d4:	0f 14 65 00 [ 	]*unpcklps 0x0\(%ebp\),%xmm4
 1d8:	0f 14 ee [ 	]*unpcklps %xmm6,%xmm5
 1db:	0f 57 37 [ 	]*xorps  \(%edi\),%xmm6
 1de:	0f 57 f8 [ 	]*xorps  %xmm0,%xmm7
 1e1:	0f e0 c1 [ 	]*pavgb  %mm1,%mm0
 1e4:	0f e0 0a [ 	]*pavgb  \(%edx\),%mm1
 1e7:	0f e3 d3 [ 	]*pavgw  %mm3,%mm2
 1ea:	0f e3 1c 24 [ 	]*pavgw  \(%esp\),%mm3
 1ee:	0f c5 c1 00 [ 	]*pextrw \$0x0,%mm1,%eax
 1f2:	0f c4 09 01 [ 	]*pinsrw \$0x1,\(%ecx\),%mm1
 1f6:	0f c4 d2 02 [ 	]*pinsrw \$0x2,%edx,%mm2
 1fa:	0f ee c1 [ 	]*pmaxsw %mm1,%mm0
 1fd:	0f ee 0a [ 	]*pmaxsw \(%edx\),%mm1
 200:	0f de d2 [ 	]*pmaxub %mm2,%mm2
 203:	0f de 1c 24 [ 	]*pmaxub \(%esp\),%mm3
 207:	0f ea e5 [ 	]*pminsw %mm5,%mm4
 20a:	0f ea 2e [ 	]*pminsw \(%esi\),%mm5
 20d:	0f da f7 [ 	]*pminub %mm7,%mm6
 210:	0f da 38 [ 	]*pminub \(%eax\),%mm7
 213:	0f d7 c5 [ 	]*pmovmskb %mm5,%eax
 216:	0f e4 e5 [ 	]*pmulhuw %mm5,%mm4
 219:	0f e4 2e [ 	]*pmulhuw \(%esi\),%mm5
 21c:	0f f6 f7 [ 	]*psadbw %mm7,%mm6
 21f:	0f f6 38 [ 	]*psadbw \(%eax\),%mm7
 222:	0f 70 da 01 [ 	]*pshufw \$0x1,%mm2,%mm3
 226:	0f 70 75 00 04 [ 	]*pshufw \$0x4,0x0\(%ebp\),%mm6
 22b:	0f f7 c7 [ 	]*maskmovq %mm7,%mm0
 22e:	0f 2b 33 [ 	]*movntps %xmm6,\(%ebx\)
 231:	0f e7 10 [ 	]*movntq %mm2,\(%eax\)
 234:	0f 18 06 [ 	]*prefetchnta \(%esi\)
 237:	0f 18 0c 98 [ 	]*prefetcht0 \(%eax,%ebx,4\)
 23b:	0f 18 12 [ 	]*prefetcht1 \(%edx\)
 23e:	0f 18 19 [ 	]*prefetcht2 \(%ecx\)
 241:	2e 0f [ 	]*\(bad\)  
 243:	c2 0a 08 [ 	]*ret    \$0x80a
 246:	90 [ 	]*nop    
 247:	90 [ 	]*nop    
 248:	65 [ 	]*gs
 249:	0f [ 	]*sfence.*\(bad\).*
 24a:	ae [ 	]*scas   %es:\(%edi\),%al
 24b:	ff 00 [ 	]*incl   \(%eax\)
 24d:	00 00 [ 	]*add    %al,\(%eax\)
	...
