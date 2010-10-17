#as:
#objdump: -dr
#name: i860 shift

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 22 a0 	shl	%r0,%r1,%sp
   4:	00 18 85 a0 	shl	%fp,%r4,%r5
   8:	00 30 e8 a0 	shl	%r6,%r7,%r8
   c:	00 48 4b a1 	shl	%r9,%r10,%r11
  10:	00 f8 ae a1 	shl	%r31,%r13,%r14
  14:	00 78 11 a2 	shl	%r15,%r16,%r17
  18:	00 90 74 a2 	shl	%r18,%r19,%r20
  1c:	00 a8 d7 a2 	shl	%r21,%r22,%r23
  20:	00 c0 3f a3 	shl	%r24,%r25,%r31
  24:	00 d8 9d a3 	shl	%r27,%r28,%r29
  28:	00 f0 e0 a3 	shl	%r30,%r31,%r0
  2c:	00 00 22 a8 	shr	%r0,%r1,%sp
  30:	00 18 85 a8 	shr	%fp,%r4,%r5
  34:	00 30 e8 a8 	shr	%r6,%r7,%r8
  38:	00 48 4b a9 	shr	%r9,%r10,%r11
  3c:	00 f8 ae a9 	shr	%r31,%r13,%r14
  40:	00 78 11 aa 	shr	%r15,%r16,%r17
  44:	00 90 74 aa 	shr	%r18,%r19,%r20
  48:	00 a8 d7 aa 	shr	%r21,%r22,%r23
  4c:	00 c0 3f ab 	shr	%r24,%r25,%r31
  50:	00 d8 9d ab 	shr	%r27,%r28,%r29
  54:	00 f0 e0 ab 	shr	%r30,%r31,%r0
  58:	00 00 22 b8 	shra	%r0,%r1,%sp
  5c:	00 18 85 b8 	shra	%fp,%r4,%r5
  60:	00 30 e8 b8 	shra	%r6,%r7,%r8
  64:	00 48 4b b9 	shra	%r9,%r10,%r11
  68:	00 f8 ae b9 	shra	%r31,%r13,%r14
  6c:	00 78 11 ba 	shra	%r15,%r16,%r17
  70:	00 90 74 ba 	shra	%r18,%r19,%r20
  74:	00 a8 d7 ba 	shra	%r21,%r22,%r23
  78:	00 c0 3f bb 	shra	%r24,%r25,%r31
  7c:	00 d8 9d bb 	shra	%r27,%r28,%r29
  80:	00 f0 e0 bb 	shra	%r30,%r31,%r0
  84:	00 00 22 b0 	shrd	%r0,%r1,%sp
  88:	00 18 85 b0 	shrd	%fp,%r4,%r5
  8c:	00 30 e8 b0 	shrd	%r6,%r7,%r8
  90:	00 48 4b b1 	shrd	%r9,%r10,%r11
  94:	00 f8 ae b1 	shrd	%r31,%r13,%r14
  98:	00 78 11 b2 	shrd	%r15,%r16,%r17
  9c:	00 90 74 b2 	shrd	%r18,%r19,%r20
  a0:	00 a8 d7 b2 	shrd	%r21,%r22,%r23
  a4:	00 c0 3f b3 	shrd	%r24,%r25,%r31
  a8:	00 d8 9d b3 	shrd	%r27,%r28,%r29
  ac:	00 f0 e0 b3 	shrd	%r30,%r31,%r0
  b0:	00 00 22 a4 	shl	0,%r1,%sp
  b4:	00 20 85 a4 	shl	8192,%r4,%r5
  b8:	f5 13 e8 a4 	shl	5109,%r7,%r8
  bc:	ff 7f 4b a5 	shl	32767,%r10,%r11
  c0:	00 80 ae a5 	shl	-32768,%r13,%r14
  c4:	00 e0 11 a6 	shl	-8192,%r16,%r17
  c8:	ff ff 74 a6 	shl	-1,%r19,%r20
  cc:	cd ab d7 a6 	shl	-21555,%r22,%r23
  d0:	34 12 3a a7 	shl	4660,%r25,%r26
  d4:	00 00 9d a7 	shl	0,%r28,%r29
  d8:	03 00 e0 a7 	shl	3,%r31,%r0
  dc:	00 00 22 ac 	shr	0,%r1,%sp
  e0:	00 20 85 ac 	shr	8192,%r4,%r5
  e4:	f5 13 e8 ac 	shr	5109,%r7,%r8
  e8:	ff 7f 4b ad 	shr	32767,%r10,%r11
  ec:	00 80 ae ad 	shr	-32768,%r13,%r14
  f0:	00 e0 11 ae 	shr	-8192,%r16,%r17
  f4:	ff ff 74 ae 	shr	-1,%r19,%r20
  f8:	cd ab d7 ae 	shr	-21555,%r22,%r23
  fc:	34 12 3a af 	shr	4660,%r25,%r26
 100:	00 00 9d af 	shr	0,%r28,%r29
 104:	03 00 e0 af 	shr	3,%r31,%r0
 108:	01 00 22 bc 	shra	1,%r1,%sp
 10c:	01 20 85 bc 	shra	8193,%r4,%r5
 110:	f6 13 e8 bc 	shra	5110,%r7,%r8
 114:	ff 7f 4b bd 	shra	32767,%r10,%r11
 118:	00 80 ae bd 	shra	-32768,%r13,%r14
 11c:	00 e0 11 be 	shra	-8192,%r16,%r17
 120:	ff ff 74 be 	shra	-1,%r19,%r20
 124:	cd ab d7 be 	shra	-21555,%r22,%r23
 128:	34 12 3a bf 	shra	4660,%r25,%r26
 12c:	00 00 9d bf 	shra	0,%r28,%r29
 130:	03 00 e0 bf 	shra	3,%r31,%r0
