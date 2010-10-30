#as:
#objdump:  -dr
#name:  lpsp_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	14 00 91 00 	lpr	r1,psr
   4:	14 00 82 00 	lpr	r2,cfg
   8:	14 00 a2 00 	lpr	r2,intbasel
   c:	14 00 b3 00 	lpr	r3,intbaseh
  10:	14 00 c4 00 	lpr	r4,ispl
  14:	14 00 d5 00 	lpr	r5,isph
  18:	14 00 e6 00 	lpr	r6,uspl
  1c:	14 00 f7 00 	lpr	r7,usph
  20:	14 00 18 00 	lpr	r8,dsr
  24:	14 00 29 00 	lpr	r9,dcrl
  28:	14 00 3a 00 	lpr	r10,dcrh
  2c:	14 00 4b 00 	lpr	r11,car0l
  30:	14 00 50 00 	lpr	r0,car0h
  34:	14 00 61 00 	lpr	r1,car1l
  38:	14 00 73 00 	lpr	r3,car1h
  3c:	14 00 90 10 	lprd	\(r1,r0\),psr
  40:	14 00 81 10 	lprd	\(r2,r1\),cfg
  44:	14 00 a2 10 	lprd	\(r3,r2\),intbase
  48:	14 00 c3 10 	lprd	\(r4,r3\),isp
  4c:	14 00 e4 10 	lprd	\(r5,r4\),usp
  50:	14 00 15 10 	lprd	\(r6,r5\),dsr
  54:	14 00 26 10 	lprd	\(r7,r6\),dcr
  58:	14 00 47 10 	lprd	\(r8,r7\),car0
  5c:	14 00 68 10 	lprd	\(r9,r8\),car1
  60:	14 00 90 20 	spr	psr,r0
  64:	14 00 81 20 	spr	cfg,r1
  68:	14 00 a2 20 	spr	intbasel,r2
  6c:	14 00 b3 20 	spr	intbaseh,r3
  70:	14 00 c4 20 	spr	ispl,r4
  74:	14 00 d5 20 	spr	isph,r5
  78:	14 00 e6 20 	spr	uspl,r6
  7c:	14 00 f7 20 	spr	usph,r7
  80:	14 00 18 20 	spr	dsr,r8
  84:	14 00 29 20 	spr	dcrl,r9
  88:	14 00 3a 20 	spr	dcrh,r10
  8c:	14 00 4b 20 	spr	car0l,r11
  90:	14 00 50 20 	spr	car0h,r0
  94:	14 00 61 20 	spr	car1l,r1
  98:	14 00 72 20 	spr	car1h,r2
  9c:	14 00 90 30 	sprd	psr,\(r1,r0\)
  a0:	14 00 81 30 	sprd	cfg,\(r2,r1\)
  a4:	14 00 a2 30 	sprd	intbase,\(r3,r2\)
  a8:	14 00 c3 30 	sprd	isp,\(r4,r3\)
  ac:	14 00 e4 30 	sprd	usp,\(r5,r4\)
  b0:	14 00 15 30 	sprd	dsr,\(r6,r5\)
  b4:	14 00 26 30 	sprd	dcr,\(r7,r6\)
  b8:	14 00 47 30 	sprd	car0,\(r8,r7\)
  bc:	14 00 68 30 	sprd	car1,\(r9,r8\)
