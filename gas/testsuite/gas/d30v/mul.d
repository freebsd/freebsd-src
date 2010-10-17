#objdump: -dr
#name: D30V alignment test
#as: -WO

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <.text>:
   0:	8a105187 0a1020c4 	mulx2h	r5, r6, r7	<-	mulx2h	r2, r3, r4
   8:	00f00000 0a10824a 	nop		||	mulx2h	r8, r9, r10
  10:	00f00000 0a10b30d 	nop		||	mulx2h	r11, r12, r13
  18:	8a111493 0a10e3d0 	mulx2h	r17, r18, r19	<-	mulx2h	r14, r15, r16
  20:	8a117619 0a114556 	mulx2h	r23, r24, r25	<-	mulx2h	r20, r21, r22
  28:	8b01d79f 0a11a6dc 	mul	r29, r30, r31	<-	mulx2h	r26, r27, r28
  30:	8b005187 0b0020c4 	mul	r5, r6, r7	<-	mul	r2, r3, r4
  38:	8a10b30d 0a10824a 	mulx2h	r11, r12, r13	<-	mulx2h	r8, r9, r10
  40:	80f00000 0b00e3d0 	nop		<-	mul	r14, r15, r16
  48:	00f00000 0a111493 	nop		||	mulx2h	r17, r18, r19
  50:	8b017619 0a114556 	mul	r23, r24, r25	<-	mulx2h	r20, r21, r22
