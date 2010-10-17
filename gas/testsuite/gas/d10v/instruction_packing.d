#objdump: -Dr
#name: D10V intruction packing
#as: -W

.*: +file format elf32-d10v

Disassembly of section .text:

00000000 <main>:
   0:	2f 00 5e 00 	nop		||	nop	

00000004 <FM00_IU_MU>:
   4:	30 23 24 02 	ld	r2, @r3	||	sra	r0, r1
   8:	24 11 a4 02 	bra.s	94 <test_end>	||	sra	r0, r1

0000000c <FM00_MU_IU>:
   c:	30 23 24 02 	ld	r2, @r3	||	sra	r0, r1
  10:	24 10 a4 02 	bra.s	94 <test_end>	||	sra	r0, r1

00000014 <FM00_IM_MU>:
  14:	30 23 02 8a 	ld	r2, @r3	||	add	r4, r5
  18:	24 0f 82 8a 	bra.s	94 <test_end>	||	add	r4, r5

0000001c <FM00_IM_IU>:
  1c:	01 45 24 02 	add	r4, r5	||	sra	r0, r1
  20:	01 45 2c ce 	add	r4, r5	||	mulx	a0, r6, r7

00000024 <FM00_MU_IM>:
  24:	30 23 02 8a 	ld	r2, @r3	||	add	r4, r5
  28:	24 0d 82 8a 	bra.s	94 <test_end>	||	add	r4, r5

0000002c <FM00_IU_IM>:
  2c:	01 45 24 02 	add	r4, r5	||	sra	r0, r1
  30:	01 45 2c ce 	add	r4, r5	||	mulx	a0, r6, r7

00000034 <FM01_IU_MU>:
  34:	b0 23 24 02 	ld	r2, @r3	<-	sra	r0, r1
  38:	a4 0b a4 02 	bra.s	94 <test_end>	<-	sra	r0, r1

0000003c <FM01_MU_IU>:
  3c:	70 23 24 02 	ld	r2, @r3	->	sra	r0, r1
  40:	64 0a a4 02 	bra.s	94 <test_end>	->	sra	r0, r1

00000044 <FM01_IM_MU>:
  44:	41 45 60 46 	add	r4, r5	->	ld	r2, @r3
  48:	41 45 48 13 	add	r4, r5	->	bra.s	94 <test_end>

0000004c <FM01_IM_IU>:
  4c:	41 45 24 02 	add	r4, r5	->	sra	r0, r1
  50:	41 45 2c ce 	add	r4, r5	->	mulx	a0, r6, r7

00000054 <FM01_MU_IM>:
  54:	70 23 02 8a 	ld	r2, @r3	->	add	r4, r5
  58:	64 07 82 8a 	bra.s	94 <test_end>	->	add	r4, r5

0000005c <FM01_IU_IM>:
  5c:	81 45 24 02 	add	r4, r5	<-	sra	r0, r1
  60:	81 45 2c ce 	add	r4, r5	<-	mulx	a0, r6, r7

00000064 <FM10_IU_MU>:
  64:	70 23 24 02 	ld	r2, @r3	->	sra	r0, r1
  68:	64 05 a4 02 	bra.s	94 <test_end>	->	sra	r0, r1

0000006c <FM10_MU_IU>:
  6c:	b0 23 24 02 	ld	r2, @r3	<-	sra	r0, r1
  70:	a4 04 a4 02 	bra.s	94 <test_end>	<-	sra	r0, r1

00000074 <FM10_IM_MU>:
  74:	70 23 02 8a 	ld	r2, @r3	->	add	r4, r5
  78:	64 03 82 8a 	bra.s	94 <test_end>	->	add	r4, r5

0000007c <FM10_IM_IU>:
  7c:	81 45 24 02 	add	r4, r5	<-	sra	r0, r1
  80:	81 45 2c ce 	add	r4, r5	<-	mulx	a0, r6, r7

00000084 <FM10_MU_IM>:
  84:	b0 23 02 8a 	ld	r2, @r3	<-	add	r4, r5
  88:	a4 01 82 8a 	bra.s	94 <test_end>	<-	add	r4, r5

0000008c <FM10_IU_IM>:
  8c:	92 01 02 8a 	sra	r0, r1	<-	add	r4, r5
  90:	96 67 02 8a 	mulx	a0, r6, r7	<-	add	r4, r5

00000094 <test_end>:
  94:	26 0d 5e 00 	jmp	r13	||	nop	
