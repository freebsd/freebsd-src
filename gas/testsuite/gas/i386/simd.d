#as: -J
#objdump: -dw
#name: i386 SIMD

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[ 	]*[a-f0-9]+:	f2 0f d0 0d 78 56 34 12 	addsubps 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	66 0f 2f 0d 78 56 34 12 	comisd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 2f 0d 78 56 34 12 	comiss 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f e6 0d 78 56 34 12 	cvtdq2pd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f2 0f e6 0d 78 56 34 12 	cvtpd2dq 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 5a 0d 78 56 34 12 	cvtps2pd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f 5b 0d 78 56 34 12 	cvttps2dq 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f2 0f 7c 0d 78 56 34 12 	haddps 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f 7f 0d 78 56 34 12 	movdqu %xmm1,0x12345678
[ 	]*[a-f0-9]+:	f3 0f 6f 0d 78 56 34 12 	movdqu 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	66 0f 17 0d 78 56 34 12 	movhpd %xmm1,0x12345678
[ 	]*[a-f0-9]+:	66 0f 16 0d 78 56 34 12 	movhpd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 17 0d 78 56 34 12 	movhps %xmm1,0x12345678
[ 	]*[a-f0-9]+:	0f 16 0d 78 56 34 12 	movhps 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	66 0f 13 0d 78 56 34 12 	movlpd %xmm1,0x12345678
[ 	]*[a-f0-9]+:	66 0f 12 0d 78 56 34 12 	movlpd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 13 0d 78 56 34 12 	movlps %xmm1,0x12345678
[ 	]*[a-f0-9]+:	0f 12 0d 78 56 34 12 	movlps 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f 16 0d 78 56 34 12 	movshdup 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f 12 0d 78 56 34 12 	movsldup 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f3 0f 70 0d 78 56 34 12 90 	pshufhw \$0x90,0x12345678,%xmm1
[ 	]*[a-f0-9]+:	f2 0f 70 0d 78 56 34 12 90 	pshuflw \$0x90,0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 60 0d 78 56 34 12 	punpcklbw 0x12345678,%mm1
[ 	]*[a-f0-9]+:	0f 62 0d 78 56 34 12 	punpckldq 0x12345678,%mm1
[ 	]*[a-f0-9]+:	0f 61 0d 78 56 34 12 	punpcklwd 0x12345678,%mm1
[ 	]*[a-f0-9]+:	66 0f 2e 0d 78 56 34 12 	ucomisd 0x12345678,%xmm1
[ 	]*[a-f0-9]+:	0f 2e 0d 78 56 34 12 	ucomiss 0x12345678,%xmm1
