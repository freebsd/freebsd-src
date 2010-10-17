#objdump: -dr
#name: D30V optimization test
#as: -O

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <start>:
   0:	08801080 08803100 	abs	r1, r2	||	abs	r3, r4
   8:	02900100 02901080 	notfg	f0, s	||	notfg	f1, f2
  10:	08801080 02901080 	abs	r1, r2	||	notfg	f1, f2
  18:	08001083 82907000 	add.s	r1, r2, r3	->	notfg	c, f0
  20:	08001083 829001c0 	add.s	r1, r2, r3	->	notfg	f0, c
  28:	00080000 00f00000 	bra.s	0	\(28 <start\+0x28>\)	||	nop	
  30:	08801080 88801080 	abs	r1, r2	->	abs	r1, r2
  38:	00080000 00f00000 	bra.s	0	\(38 <start\+0x38>\)	||	nop	
  40:	002bffff 00f00000 	bsr.s	-8	\(38 <start\+0x38>\)	||	nop	
  48:	08801080 88801080 	abs	r1, r2	->	abs	r1, r2
  50:	00280000 08801080 	bsr.s	0	\(50 <start\+0x50>\)	||	abs	r1, r2
  58:	04001083 85007209 	ldb.s	r1, @\(r2, r3\)	->	stb.s	r7, @\(r8, r9\)
  60:	05007209 84001083 	stb.s	r7, @\(r8, r9\)	->	ldb.s	r1, @\(r2, r3\)
  68:	04007209 84001083 	ldb.s	r7, @\(r8, r9\)	->	ldb.s	r1, @\(r2, r3\)
  70:	05007209 85001083 	stb.s	r7, @\(r8, r9\)	->	stb.s	r1, @\(r2, r3\)
  78:	080030c6 854820c0 	add.s	r3, r3, r6	->	stw.s	r2, @\(r3, 0x0\)
  80:	02c28105 90180000 	cmple.s	f0, r4, r5	->	jmp.s/tx	0 <start>
  88:	02c28105 a0180000 	cmple.s	f0, r4, r5	->	jmp.s/fx	0 <start>
  90:	30180000 02c28105 	jmp.s/xt	0 <start>	||	cmple.s	f0, r4, r5
  98:	40180000 02c28105 	jmp.s/xf	0 <start>	||	cmple.s	f0, r4, r5
  a0:	02c28105 d0180000 	cmple.s	f0, r4, r5	->	jmp.s/tt	0 <start>
  a8:	02c28105 e0180000 	cmple.s	f0, r4, r5	->	jmp.s/tf	0 <start>
  b0:	10180000 02c29105 	jmp.s/tx	0 <start>	||	cmple.s	f1, r4, r5
  b8:	02c29105 b0180000 	cmple.s	f1, r4, r5	->	jmp.s/xt	0 <start>
  c0:	08084001 82c28105 	add.s	r4, r0, 0x1	->	cmple.s	f0, r4, r5
  c8:	08084001 02c280c5 	add.s	r4, r0, 0x1	||	cmple.s	f0, r3, r5
  d0:	04604006 886054d4 	ld2w.s	r4, @\(r0, r6\)	->	adds.s	r5, r19, r20
  d8:	04604006 88603154 	ld2w.s	r4, @\(r0, r6\)	->	adds.s	r3, r5, r20
  e0:	04604006 086064d4 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r6, r19, r20
  e8:	04604006 086074d4 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r19, r20
  f0:	04604006 08607014 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r0, r20
  f8:	05604006 086054d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r5, r19, r20
 100:	05604006 08603154 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r3, r5, r20
 108:	05604006 086064d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r6, r19, r20
 110:	05604006 086074d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r19, r20
 118:	05604006 08607014 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r0, r20
 120:	0560a0c4 85628aec 	st2w.s	r10, @\(r3, r4\)	->	st2w.s	r40, @\(r43, r44\)
 128:	05401083 84429aab 	stw.s	r1, @\(r2, r3\)	->	ldw.s	r41, @\(r42, r43\)
 130:	04401083 84029aab 	ldw.s	r1, @\(r2, r3\)	->	ldb.s	r41, @\(r42, r43\)
 138:	0444418b 88689182 	ldw.s	r4, @\(r6\+, r11\)	->	adds.s	r9, r6, 0x2
 140:	044c418b 08689182 	ldw.s	r4, @\(r6-, r11\)	||	adds.s	r9, r6, 0x2
 148:	054c418b 88689182 	stw.s	r4, @\(r6-, r11\)	->	adds.s	r9, r6, 0x2
 150:	0440418b 08689182 	ldw.s	r4, @\(r6, r11\)	||	adds.s	r9, r6, 0x2
 158:	0440418b 08689182 	ldw.s	r4, @\(r6, r11\)	||	adds.s	r9, r6, 0x2
 160:	00180000 00f00000 	jmp.s	0 <start>	||	nop	
 168:	00380000 08801080 	jsr.s	0 <start>	||	abs	r1, r2
 170:	08801080 00f00000 	abs	r1, r2	||	nop	
 178:	00080000 00f00000 	bra.s	0	\(178 <start\+0x178>\)	||	nop	
 180:	00280000 08801080 	bsr.s	0	\(180 <start\+0x180>\)	||	abs	r1, r2
 188:	08801080 00f00000 	abs	r1, r2	||	nop	

00000190 <label1>:
 190:	05602083 89004146 	st2w.s	r2, @\(r2, r3\)	->	addhlll.s	r4, r5, r6

00000198 <label2>:
 198:	05508209 8990a2cc 	st4hb.s	r8, @\(r8, r9\)	->	subhllh.s	r10, r11, r12

000001a0 <label3>:
 1a0:	0460e38f 8a610452 	ld2w.s	r14, @\(r14, r15\)	->	mulhxhl	r16, r17, r18

000001a8 <label4>:
 1a8:	04413515 8a1165d8 	ldw.s	r19, @\(r20, r21\)	->	mulx2h	r22, r23, r24

000001b0 <label5>:
 1b0:	0421969b 8a01c75e 	ldh.s	r25, @\(r26, r27\)	->	mul2h	r28, r29, r30

000001b8 <label6>:
 1b8:	80f00000 0b001083 	nop		<-	mul	r1, r2, r3
 1c0:	08007209 0a404146 	add.s	r7, r8, r9	||	mulhxll	r4, r5, r6

000001c8 <label7>:
 1c8:	04405180 0b0020c4 	ldw.s	r5, @\(r6, r0\)	||	mul	r2, r3, r4
 1d0:	80f00000 0b007209 	nop		<-	mul	r7, r8, r9
 1d8:	0440a2c0 00f00000 	ldw.s	r10, @\(r11, r0\)	||	nop	
 1e0:	80f00000 0b00c34e 	nop		<-	mul	r12, r13, r14
 1e8:	0440f400 0b4420c4 	ldw.s	r15, @\(r16, r0\)	||	mac1	r2, r3, r4
 1f0:	00f00000 00f00000 	nop		||	nop	
 1f8:	04405180 00f00000 	ldw.s	r5, @\(r6, r0\)	||	nop	
 200:	80f00000 0b407209 	nop		<-	mac0	r7, r8, r9
 208:	0440a2c0 8440a2c0 	ldw.s	r10, @\(r11, r0\)	->	ldw.s	r10, @\(r11, r0\)
