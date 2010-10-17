#objdump: -Dr
#source: immediate-001.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	e0 00 7f ff 	ldi.l	r0, 0x7fff
   4:	e0 00 ff ff 	ldi.l	r0, -0x1
   8:	41 0f 8c 1f 	addi	r0, 0xf	->	bclri	r0, 0xf
   c:	84 0f 8a 1f 	bseti	r0, 0xf	<-	bnoti	r0, 0xf
  10:	a9 03 8e 1f 	rac	r0, a0, 0x3	<-	btsti	r0, 0xf
  14:	a1 03 d2 0d 	rachi	r0, a0, 0x3	<-	rac	r0, a0, -0x2
  18:	91 0f c2 0d 	slli	r0, 0xf	<-	rachi	r0, a0, -0x2
  1c:	92 0f b3 1f 	srai	r0, 0xf	<-	slli	a1, 0xf
  20:	9a 0f b4 01 	srai	a0, 0xf	<-	srai	a0, 0x0
  24:	98 00 a0 1f 	srli	a0, 0x0	<-	srli	r0, 0xf
  28:	80 00 b0 1f 	subi	r0, 0x0	<-	srli	a0, 0xf
  2c:	40 0f df 1e 	subi	r0, 0xf	->	trap	0xf
  30:	c7 00 7f ff 	tst0i	r0, 0x7fff
  34:	c7 00 ff ff 	tst0i	r0, -0x1
  38:	cf 00 7f ff 	tst1i	r0, 0x7fff
  3c:	cf 00 ff ff 	tst1i	r0, -0x1
