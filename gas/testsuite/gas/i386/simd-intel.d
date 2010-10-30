#source: simd.s
#as: -J
#objdump: -dw -Mintel
#name: i386 SIMD (Intel mode)

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[ 	]*[a-f0-9]+:	f2 0f d0 0d 78 56 34 12 	addsubps xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	66 0f 2f 0d 78 56 34 12 	comisd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 2f 0d 78 56 34 12 	comiss xmm1,DWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f e6 0d 78 56 34 12 	cvtdq2pd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f2 0f e6 0d 78 56 34 12 	cvtpd2dq xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 5a 0d 78 56 34 12 	cvtps2pd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f 5b 0d 78 56 34 12 	cvttps2dq xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f2 0f 7c 0d 78 56 34 12 	haddps xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f 7f 0d 78 56 34 12 	movdqu XMMWORD PTR ds:0x12345678,xmm1
[ 	]*[a-f0-9]+:	f3 0f 6f 0d 78 56 34 12 	movdqu xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	66 0f 17 0d 78 56 34 12 	movhpd QWORD PTR ds:0x12345678,xmm1
[ 	]*[a-f0-9]+:	66 0f 16 0d 78 56 34 12 	movhpd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 17 0d 78 56 34 12 	movhps QWORD PTR ds:0x12345678,xmm1
[ 	]*[a-f0-9]+:	0f 16 0d 78 56 34 12 	movhps xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	66 0f 13 0d 78 56 34 12 	movlpd QWORD PTR ds:0x12345678,xmm1
[ 	]*[a-f0-9]+:	66 0f 12 0d 78 56 34 12 	movlpd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 13 0d 78 56 34 12 	movlps QWORD PTR ds:0x12345678,xmm1
[ 	]*[a-f0-9]+:	0f 12 0d 78 56 34 12 	movlps xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f 16 0d 78 56 34 12 	movshdup xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f 12 0d 78 56 34 12 	movsldup xmm1,XMMWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	f3 0f 70 0d 78 56 34 12 90 	pshufhw xmm1,XMMWORD PTR ds:0x12345678,0x90
[ 	]*[a-f0-9]+:	f2 0f 70 0d 78 56 34 12 90 	pshuflw xmm1,XMMWORD PTR ds:0x12345678,0x90
[ 	]*[a-f0-9]+:	0f 60 0d 78 56 34 12 	punpcklbw mm1,DWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 62 0d 78 56 34 12 	punpckldq mm1,DWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 61 0d 78 56 34 12 	punpcklwd mm1,DWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	66 0f 2e 0d 78 56 34 12 	ucomisd xmm1,QWORD PTR ds:0x12345678
[ 	]*[a-f0-9]+:	0f 2e 0d 78 56 34 12 	ucomiss xmm1,DWORD PTR ds:0x12345678
