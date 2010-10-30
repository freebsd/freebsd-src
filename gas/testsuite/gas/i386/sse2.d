#as: -J
#objdump: -dw
#name: i386 sse2

.*: +file format .*

Disassembly of section .text:

0+ <foo>:
[ 	]+0:	0f c3 00[ 	]+movnti %eax,\(%eax\)
[ 	]+3:	0f ae f8[ 	]+sfence 
[ 	]+6:	0f ae e8[ 	]+lfence 
[ 	]+9:	0f ae f0[ 	]+mfence 
[ 	]+c:	66 0f 58 01[ 	]+addpd[ 	]+\(%ecx\),%xmm0
[ 	]+10:	66 0f 58 ca[ 	]+addpd[ 	]+%xmm2,%xmm1
[ 	]+14:	f2 0f 58 13[ 	]+addsd[ 	]+\(%ebx\),%xmm2
[ 	]+18:	f2 0f 58 dc[ 	]+addsd[ 	]+%xmm4,%xmm3
[ 	]+1c:	66 0f 55 65 00[ 	]+andnpd 0x0\(%ebp\),%xmm4
[ 	]+21:	66 0f 55 ee[ 	]+andnpd %xmm6,%xmm5
[ 	]+25:	66 0f 54 37[ 	]+andpd[ 	]+\(%edi\),%xmm6
[ 	]+29:	66 0f 54 f8[ 	]+andpd[ 	]+%xmm0,%xmm7
[ 	]+2d:	66 0f c2 c1 02[ 	]+cmplepd %xmm1,%xmm0
[ 	]+32:	66 0f c2 0a 03[ 	]+cmpunordpd \(%edx\),%xmm1
[ 	]+37:	f2 0f c2 d2 04[ 	]+cmpneqsd %xmm2,%xmm2
[ 	]+3c:	f2 0f c2 1c 24 05[ 	]+cmpnltsd \(%esp\),%xmm3
[ 	]+42:	66 0f c2 e5 06[ 	]+cmpnlepd %xmm5,%xmm4
[ 	]+47:	66 0f c2 2e 07[ 	]+cmpordpd \(%esi\),%xmm5
[ 	]+4c:	f2 0f c2 f7 00[ 	]+cmpeqsd %xmm7,%xmm6
[ 	]+51:	f2 0f c2 38 01[ 	]+cmpltsd \(%eax\),%xmm7
[ 	]+56:	66 0f c2 c1 00[ 	]+cmpeqpd %xmm1,%xmm0
[ 	]+5b:	66 0f c2 0a 00[ 	]+cmpeqpd \(%edx\),%xmm1
[ 	]+60:	f2 0f c2 d2 00[ 	]+cmpeqsd %xmm2,%xmm2
[ 	]+65:	f2 0f c2 1c 24 00[ 	]+cmpeqsd \(%esp\),%xmm3
[ 	]+6b:	66 0f c2 e5 01[ 	]+cmpltpd %xmm5,%xmm4
[ 	]+70:	66 0f c2 2e 01[ 	]+cmpltpd \(%esi\),%xmm5
[ 	]+75:	f2 0f c2 f7 01[ 	]+cmpltsd %xmm7,%xmm6
[ 	]+7a:	f2 0f c2 38 01[ 	]+cmpltsd \(%eax\),%xmm7
[ 	]+7f:	66 0f c2 01 02[ 	]+cmplepd \(%ecx\),%xmm0
[ 	]+84:	66 0f c2 ca 02[ 	]+cmplepd %xmm2,%xmm1
[ 	]+89:	f2 0f c2 13 02[ 	]+cmplesd \(%ebx\),%xmm2
[ 	]+8e:	f2 0f c2 dc 02[ 	]+cmplesd %xmm4,%xmm3
[ 	]+93:	66 0f c2 65 00 03[ 	]+cmpunordpd 0x0\(%ebp\),%xmm4
[ 	]+99:	66 0f c2 ee 03[ 	]+cmpunordpd %xmm6,%xmm5
[ 	]+9e:	f2 0f c2 37 03[ 	]+cmpunordsd \(%edi\),%xmm6
[ 	]+a3:	f2 0f c2 f8 03[ 	]+cmpunordsd %xmm0,%xmm7
[ 	]+a8:	66 0f c2 c1 04[ 	]+cmpneqpd %xmm1,%xmm0
[ 	]+ad:	66 0f c2 0a 04[ 	]+cmpneqpd \(%edx\),%xmm1
[ 	]+b2:	f2 0f c2 d2 04[ 	]+cmpneqsd %xmm2,%xmm2
[ 	]+b7:	f2 0f c2 1c 24 04[ 	]+cmpneqsd \(%esp\),%xmm3
[ 	]+bd:	66 0f c2 e5 05[ 	]+cmpnltpd %xmm5,%xmm4
[ 	]+c2:	66 0f c2 2e 05[ 	]+cmpnltpd \(%esi\),%xmm5
[ 	]+c7:	f2 0f c2 f7 05[ 	]+cmpnltsd %xmm7,%xmm6
[ 	]+cc:	f2 0f c2 38 05[ 	]+cmpnltsd \(%eax\),%xmm7
[ 	]+d1:	66 0f c2 01 06[ 	]+cmpnlepd \(%ecx\),%xmm0
[ 	]+d6:	66 0f c2 ca 06[ 	]+cmpnlepd %xmm2,%xmm1
[ 	]+db:	f2 0f c2 13 06[ 	]+cmpnlesd \(%ebx\),%xmm2
[ 	]+e0:	f2 0f c2 dc 06[ 	]+cmpnlesd %xmm4,%xmm3
[ 	]+e5:	66 0f c2 65 00 07[ 	]+cmpordpd 0x0\(%ebp\),%xmm4
[ 	]+eb:	66 0f c2 ee 07[ 	]+cmpordpd %xmm6,%xmm5
[ 	]+f0:	f2 0f c2 37 07[ 	]+cmpordsd \(%edi\),%xmm6
[ 	]+f5:	f2 0f c2 f8 07[ 	]+cmpordsd %xmm0,%xmm7
[ 	]+fa:	66 0f 2f c1[ 	]+comisd %xmm1,%xmm0
[ 	]+fe:	66 0f 2f 0a[ 	]+comisd \(%edx\),%xmm1
 102:	66 0f 2a d3[ 	]+cvtpi2pd %mm3,%xmm2
 106:	66 0f 2a 1c 24[ 	]+cvtpi2pd \(%esp\),%xmm3
 10b:	f2 0f 2a e5[ 	]+cvtsi2sd %ebp,%xmm4
 10f:	f2 0f 2a 2e[ 	]+cvtsi2sd \(%esi\),%xmm5
 113:	66 0f 2d f7[ 	]+cvtpd2pi %xmm7,%mm6
 117:	66 0f 2d 38[ 	]+cvtpd2pi \(%eax\),%mm7
 11b:	f2 0f 2d 01[ 	]+cvtsd2si \(%ecx\),%eax
 11f:	f2 0f 2d ca[ 	]+cvtsd2si %xmm2,%ecx
 123:	66 0f 2c 13[ 	]+cvttpd2pi \(%ebx\),%mm2
 127:	66 0f 2c dc[ 	]+cvttpd2pi %xmm4,%mm3
 12b:	f2 0f 2c 65 00[ 	]+cvttsd2si 0x0\(%ebp\),%esp
 130:	f2 0f 2c ee[ 	]+cvttsd2si %xmm6,%ebp
 134:	66 0f 5e c1[ 	]+divpd[ 	]+%xmm1,%xmm0
 138:	66 0f 5e 0a[ 	]+divpd[ 	]+\(%edx\),%xmm1
 13c:	f2 0f 5e d3[ 	]+divsd[ 	]+%xmm3,%xmm2
 140:	f2 0f 5e 1c 24[ 	]+divsd[ 	]+\(%esp\),%xmm3
 145:	0f ae 55 00[ 	]+ldmxcsr 0x0\(%ebp\)
 149:	0f ae 1e[ 	]+stmxcsr \(%esi\)
 14c:	0f ae f8[ 	]+sfence 
 14f:	66 0f 5f c1[ 	]+maxpd[ 	]+%xmm1,%xmm0
 153:	66 0f 5f 0a[ 	]+maxpd[ 	]+\(%edx\),%xmm1
 157:	f2 0f 5f d3[ 	]+maxsd[ 	]+%xmm3,%xmm2
 15b:	f2 0f 5f 1c 24[ 	]+maxsd[ 	]+\(%esp\),%xmm3
 160:	66 0f 5d e5[ 	]+minpd[ 	]+%xmm5,%xmm4
 164:	66 0f 5d 2e[ 	]+minpd[ 	]+\(%esi\),%xmm5
 168:	f2 0f 5d f7[ 	]+minsd[ 	]+%xmm7,%xmm6
 16c:	f2 0f 5d 38[ 	]+minsd[ 	]+\(%eax\),%xmm7
 170:	66 0f 28 c1[ 	]+movapd %xmm1,%xmm0
 174:	66 0f 29 11[ 	]+movapd %xmm2,\(%ecx\)
 178:	66 0f 28 12[ 	]+movapd \(%edx\),%xmm2
 17c:	66 0f 17 2c 24[ 	]+movhpd %xmm5,\(%esp\)
 181:	66 0f 16 2e[ 	]+movhpd \(%esi\),%xmm5
 185:	66 0f 13 07[ 	]+movlpd %xmm0,\(%edi\)
 189:	66 0f 12 00[ 	]+movlpd \(%eax\),%xmm0
 18d:	66 0f 50 ca[ 	]+movmskpd %xmm2,%ecx
 191:	66 0f 10 d3[ 	]+movupd %xmm3,%xmm2
 195:	66 0f 11 22[ 	]+movupd %xmm4,\(%edx\)
 199:	66 0f 10 65 00[ 	]+movupd 0x0\(%ebp\),%xmm4
 19e:	f2 0f 10 ee[ 	]+movsd[ 	]+%xmm6,%xmm5
 1a2:	f2 0f 11 3e[ 	]+movsd[ 	]+%xmm7,\(%esi\)
 1a6:	f2 0f 10 38[ 	]+movsd[ 	]+\(%eax\),%xmm7
 1aa:	66 0f 59 c1[ 	]+mulpd[ 	]+%xmm1,%xmm0
 1ae:	66 0f 59 0a[ 	]+mulpd[ 	]+\(%edx\),%xmm1
 1b2:	f2 0f 59 d2[ 	]+mulsd[ 	]+%xmm2,%xmm2
 1b6:	f2 0f 59 1c 24[ 	]+mulsd[ 	]+\(%esp\),%xmm3
 1bb:	66 0f 56 e5[ 	]+orpd[ 	]+%xmm5,%xmm4
 1bf:	66 0f 56 2e[ 	]+orpd[ 	]+\(%esi\),%xmm5
 1c3:	66 0f c6 37 02[ 	]+shufpd \$0x2,\(%edi\),%xmm6
 1c8:	66 0f c6 f8 03[ 	]+shufpd \$0x3,%xmm0,%xmm7
 1cd:	66 0f 51 c1[ 	]+sqrtpd %xmm1,%xmm0
 1d1:	66 0f 51 0a[ 	]+sqrtpd \(%edx\),%xmm1
 1d5:	f2 0f 51 d2[ 	]+sqrtsd %xmm2,%xmm2
 1d9:	f2 0f 51 1c 24[ 	]+sqrtsd \(%esp\),%xmm3
 1de:	66 0f 5c e5[ 	]+subpd[ 	]+%xmm5,%xmm4
 1e2:	66 0f 5c 2e[ 	]+subpd[ 	]+\(%esi\),%xmm5
 1e6:	f2 0f 5c f7[ 	]+subsd[ 	]+%xmm7,%xmm6
 1ea:	f2 0f 5c 38[ 	]+subsd[ 	]+\(%eax\),%xmm7
 1ee:	66 0f 2e 01[ 	]+ucomisd \(%ecx\),%xmm0
 1f2:	66 0f 2e ca[ 	]+ucomisd %xmm2,%xmm1
 1f6:	66 0f 15 13[ 	]+unpckhpd \(%ebx\),%xmm2
 1fa:	66 0f 15 dc[ 	]+unpckhpd %xmm4,%xmm3
 1fe:	66 0f 14 65 00[ 	]+unpcklpd 0x0\(%ebp\),%xmm4
 203:	66 0f 14 ee[ 	]+unpcklpd %xmm6,%xmm5
 207:	66 0f 57 37[ 	]+xorpd[ 	]+\(%edi\),%xmm6
 20b:	66 0f 57 f8[ 	]+xorpd[ 	]+%xmm0,%xmm7
 20f:	66 0f 2b 33[ 	]+movntpd %xmm6,\(%ebx\)
 213:	66 0f 57 c8[ 	]+xorpd[ 	]+%xmm0,%xmm1
 217:	f3 0f e6 c8[ 	]+cvtdq2pd %xmm0,%xmm1
 21b:	f2 0f e6 c8[ 	]+cvtpd2dq %xmm0,%xmm1
 21f:	0f 5b c8[ 	]+cvtdq2ps %xmm0,%xmm1
 222:	66 0f 5a c8[ 	]+cvtpd2ps %xmm0,%xmm1
 226:	0f 5a c8[ 	]+cvtps2pd %xmm0,%xmm1
 229:	66 0f 5b c8[ 	]+cvtps2dq %xmm0,%xmm1
 22d:	f2 0f 5a c8[ 	]+cvtsd2ss %xmm0,%xmm1
 231:	f3 0f 5a c8[ 	]+cvtss2sd %xmm0,%xmm1
 235:	66 0f e6 c8[ 	]+cvttpd2dq %xmm0,%xmm1
 239:	f3 0f 5b c8[ 	]+cvttps2dq %xmm0,%xmm1
 23d:	66 0f f7 c8[ 	]+maskmovdqu %xmm0,%xmm1
 241:	66 0f 6f c8[ 	]+movdqa %xmm0,%xmm1
 245:	66 0f 7f 06[ 	]+movdqa %xmm0,\(%esi\)
 249:	f3 0f 6f c8[ 	]+movdqu %xmm0,%xmm1
 24d:	f3 0f 7f 06[ 	]+movdqu %xmm0,\(%esi\)
 251:	f2 0f d6 c8[ 	]+movdq2q %xmm0,%mm1
 255:	f3 0f d6 c8[ 	]+movq2dq %mm0,%xmm1
 259:	66 0f f4 c8[ 	]+pmuludq %xmm0,%xmm1
 25d:	66 0f f4 c8[ 	]+pmuludq %xmm0,%xmm1
 261:	66 0f 70 c8 01[ 	]+pshufd \$0x1,%xmm0,%xmm1
 266:	f3 0f 70 c8 01[ 	]+pshufhw \$0x1,%xmm0,%xmm1
 26b:	f2 0f 70 c8 01[ 	]+pshuflw \$0x1,%xmm0,%xmm1
 270:	66 0f 73 f8 01[ 	]+pslldq \$0x1,%xmm0
 275:	66 0f 73 d8 01[ 	]+psrldq \$0x1,%xmm0
 27a:	66 0f 6d c8[ 	]+punpckhqdq %xmm0,%xmm1
 27e:	66 90[ 	]+xchg[ 	]+%ax,%ax
