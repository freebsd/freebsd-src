#as: -J
#objdump: -dw
#name: i386 ssemmx2

.*: +file format .*

Disassembly of section .text:

0+ <foo>:
[ 	]+0:	66 0f e0 c1[ 	]+pavgb[ 	]+%xmm1,%xmm0
[ 	]+4:	66 0f e0 0a[ 	]+pavgb[ 	]+\(%edx\),%xmm1
[ 	]+8:	66 0f e3 d3[ 	]+pavgw[ 	]+%xmm3,%xmm2
[ 	]+c:	66 0f e3 1c 24[ 	]+pavgw[ 	]+\(%esp\),%xmm3
[ 	]+11:	66 0f c5 c1 00[ 	]+pextrw \$0x0,%xmm1,%eax
[ 	]+16:	66 0f c4 09 01[ 	]+pinsrw \$0x1,\(%ecx\),%xmm1
[ 	]+1b:	66 0f c4 d2 02[ 	]+pinsrw \$0x2,%edx,%xmm2
[ 	]+20:	66 0f ee c1[ 	]+pmaxsw %xmm1,%xmm0
[ 	]+24:	66 0f ee 0a[ 	]+pmaxsw \(%edx\),%xmm1
[ 	]+28:	66 0f de d2[ 	]+pmaxub %xmm2,%xmm2
[ 	]+2c:	66 0f de 1c 24[ 	]+pmaxub \(%esp\),%xmm3
[ 	]+31:	66 0f ea e5[ 	]+pminsw %xmm5,%xmm4
[ 	]+35:	66 0f ea 2e[ 	]+pminsw \(%esi\),%xmm5
[ 	]+39:	66 0f da f7[ 	]+pminub %xmm7,%xmm6
[ 	]+3d:	66 0f da 38[ 	]+pminub \(%eax\),%xmm7
[ 	]+41:	66 0f d7 c5[ 	]+pmovmskb %xmm5,%eax
[ 	]+45:	66 0f e4 e5[ 	]+pmulhuw %xmm5,%xmm4
[ 	]+49:	66 0f e4 2e[ 	]+pmulhuw \(%esi\),%xmm5
[ 	]+4d:	66 0f f6 f7[ 	]+psadbw %xmm7,%xmm6
[ 	]+51:	66 0f f6 38[ 	]+psadbw \(%eax\),%xmm7
[ 	]+55:	66 0f 70 da 01[ 	]+pshufd \$0x1,%xmm2,%xmm3
[ 	]+5a:	66 0f 70 75 00 04[ 	]+pshufd \$0x4,0x0\(%ebp\),%xmm6
[ 	]+60:	f3 0f 70 da 01[ 	]+pshufhw \$0x1,%xmm2,%xmm3
[ 	]+65:	f3 0f 70 75 00 04[ 	]+pshufhw \$0x4,0x0\(%ebp\),%xmm6
[ 	]+6b:	f2 0f 70 da 01[ 	]+pshuflw \$0x1,%xmm2,%xmm3
[ 	]+70:	f2 0f 70 75 00 04[ 	]+pshuflw \$0x4,0x0\(%ebp\),%xmm6
[ 	]+76:	66 0f e7 10[ 	]+movntdq %xmm2,\(%eax\)
[ 	]+7a:	66 0f 60 90 90 90 90 90 	punpcklbw 0x90909090\(%eax\),%xmm2
[ 	]+82:	66 0f 61 90 90 90 90 90 	punpcklwd 0x90909090\(%eax\),%xmm2
[ 	]+8a:	66 0f 62 90 90 90 90 90 	punpckldq 0x90909090\(%eax\),%xmm2
[ 	]+92:	66 0f 63 90 90 90 90 90 	packsswb 0x90909090\(%eax\),%xmm2
[ 	]+9a:	66 0f 64 90 90 90 90 90 	pcmpgtb 0x90909090\(%eax\),%xmm2
[ 	]+a2:	66 0f 65 90 90 90 90 90 	pcmpgtw 0x90909090\(%eax\),%xmm2
[ 	]+aa:	66 0f 66 90 90 90 90 90 	pcmpgtd 0x90909090\(%eax\),%xmm2
[ 	]+b2:	66 0f 67 90 90 90 90 90 	packuswb 0x90909090\(%eax\),%xmm2
[ 	]+ba:	66 0f 68 90 90 90 90 90 	punpckhbw 0x90909090\(%eax\),%xmm2
[ 	]+c2:	66 0f 69 90 90 90 90 90 	punpckhwd 0x90909090\(%eax\),%xmm2
[ 	]+ca:	66 0f 6a 90 90 90 90 90 	punpckhdq 0x90909090\(%eax\),%xmm2
[ 	]+d2:	66 0f 6b 90 90 90 90 90 	packssdw 0x90909090\(%eax\),%xmm2
[ 	]+da:	66 0f 6e 90 90 90 90 90 	movd[ 	]+0x90909090\(%eax\),%xmm2
[ 	]+e2:	f3 0f 7e 90 90 90 90 90 	movq[ 	]+0x90909090\(%eax\),%xmm2
[ 	]+ea:	66 0f 71 d0 90[ 	]+psrlw[ 	]+\$0x90,%xmm0
[ 	]+ef:	66 0f 72 d0 90[ 	]+psrld[ 	]+\$0x90,%xmm0
[ 	]+f4:	66 0f 73 d0 90[ 	]+psrlq[ 	]+\$0x90,%xmm0
[ 	]+f9:	66 0f 74 90 90 90 90 90 	pcmpeqb 0x90909090\(%eax\),%xmm2
 101:	66 0f 75 90 90 90 90 90 	pcmpeqw 0x90909090\(%eax\),%xmm2
 109:	66 0f 76 90 90 90 90 90 	pcmpeqd 0x90909090\(%eax\),%xmm2
 111:	66 0f 7e 90 90 90 90 90 	movd[ 	]+%xmm2,0x90909090\(%eax\)
 119:	66 0f d6 90 90 90 90 90 	movq[ 	]+%xmm2,0x90909090\(%eax\)
 121:	66 0f d1 90 90 90 90 90 	psrlw[ 	]+0x90909090\(%eax\),%xmm2
 129:	66 0f d2 90 90 90 90 90 	psrld[ 	]+0x90909090\(%eax\),%xmm2
 131:	66 0f d3 90 90 90 90 90 	psrlq[ 	]+0x90909090\(%eax\),%xmm2
 139:	66 0f d5 90 90 90 90 90 	pmullw 0x90909090\(%eax\),%xmm2
 141:	66 0f d8 90 90 90 90 90 	psubusb 0x90909090\(%eax\),%xmm2
 149:	66 0f d9 90 90 90 90 90 	psubusw 0x90909090\(%eax\),%xmm2
 151:	66 0f db 90 90 90 90 90 	pand[ 	]+0x90909090\(%eax\),%xmm2
 159:	66 0f dc 90 90 90 90 90 	paddusb 0x90909090\(%eax\),%xmm2
 161:	66 0f dd 90 90 90 90 90 	paddusw 0x90909090\(%eax\),%xmm2
 169:	66 0f df 90 90 90 90 90 	pandn[ 	]+0x90909090\(%eax\),%xmm2
 171:	66 0f e1 90 90 90 90 90 	psraw[ 	]+0x90909090\(%eax\),%xmm2
 179:	66 0f e2 90 90 90 90 90 	psrad[ 	]+0x90909090\(%eax\),%xmm2
 181:	66 0f e5 90 90 90 90 90 	pmulhw 0x90909090\(%eax\),%xmm2
 189:	66 0f e8 90 90 90 90 90 	psubsb 0x90909090\(%eax\),%xmm2
 191:	66 0f e9 90 90 90 90 90 	psubsw 0x90909090\(%eax\),%xmm2
 199:	66 0f eb 90 90 90 90 90 	por[ 	]+0x90909090\(%eax\),%xmm2
 1a1:	66 0f ec 90 90 90 90 90 	paddsb 0x90909090\(%eax\),%xmm2
 1a9:	66 0f ed 90 90 90 90 90 	paddsw 0x90909090\(%eax\),%xmm2
 1b1:	66 0f ef 90 90 90 90 90 	pxor[ 	]+0x90909090\(%eax\),%xmm2
 1b9:	66 0f f1 90 90 90 90 90 	psllw[ 	]+0x90909090\(%eax\),%xmm2
 1c1:	66 0f f2 90 90 90 90 90 	pslld[ 	]+0x90909090\(%eax\),%xmm2
 1c9:	66 0f f3 90 90 90 90 90 	psllq[ 	]+0x90909090\(%eax\),%xmm2
 1d1:	66 0f f5 90 90 90 90 90 	pmaddwd 0x90909090\(%eax\),%xmm2
 1d9:	66 0f f8 90 90 90 90 90 	psubb[ 	]+0x90909090\(%eax\),%xmm2
 1e1:	66 0f f9 90 90 90 90 90 	psubw[ 	]+0x90909090\(%eax\),%xmm2
 1e9:	66 0f fa 90 90 90 90 90 	psubd[ 	]+0x90909090\(%eax\),%xmm2
 1f1:	66 0f fc 90 90 90 90 90 	paddb[ 	]+0x90909090\(%eax\),%xmm2
 1f9:	66 0f fd 90 90 90 90 90 	paddw[ 	]+0x90909090\(%eax\),%xmm2
 201:	66 0f fe 90 90 90 90 90 	paddd[ 	]+0x90909090\(%eax\),%xmm2
 209:	8d b4 26 00 00 00 00 	lea[ 	]+0x0\(%esi\),%esi
