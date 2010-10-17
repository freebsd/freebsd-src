#as:
#objdump: -dr
#name: i860 iarith

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 22 80 	addu	%r0,%r1,%sp
   4:	00 18 85 80 	addu	%fp,%r4,%r5
   8:	00 30 e8 80 	addu	%r6,%r7,%r8
   c:	00 48 4b 81 	addu	%r9,%r10,%r11
  10:	00 f8 ae 81 	addu	%r31,%r13,%r14
  14:	00 78 11 82 	addu	%r15,%r16,%r17
  18:	00 90 74 82 	addu	%r18,%r19,%r20
  1c:	00 a8 d7 82 	addu	%r21,%r22,%r23
  20:	00 c0 3f 83 	addu	%r24,%r25,%r31
  24:	00 d8 9d 83 	addu	%r27,%r28,%r29
  28:	00 f0 e0 83 	addu	%r30,%r31,%r0
  2c:	00 00 22 90 	adds	%r0,%r1,%sp
  30:	00 18 85 90 	adds	%fp,%r4,%r5
  34:	00 30 e8 90 	adds	%r6,%r7,%r8
  38:	00 48 4b 91 	adds	%r9,%r10,%r11
  3c:	00 f8 ae 91 	adds	%r31,%r13,%r14
  40:	00 78 11 92 	adds	%r15,%r16,%r17
  44:	00 90 74 92 	adds	%r18,%r19,%r20
  48:	00 a8 d7 92 	adds	%r21,%r22,%r23
  4c:	00 c0 3f 93 	adds	%r24,%r25,%r31
  50:	00 d8 9d 93 	adds	%r27,%r28,%r29
  54:	00 f0 e0 93 	adds	%r30,%r31,%r0
  58:	00 00 22 88 	subu	%r0,%r1,%sp
  5c:	00 18 85 88 	subu	%fp,%r4,%r5
  60:	00 30 e8 88 	subu	%r6,%r7,%r8
  64:	00 48 4b 89 	subu	%r9,%r10,%r11
  68:	00 f8 ae 89 	subu	%r31,%r13,%r14
  6c:	00 78 11 8a 	subu	%r15,%r16,%r17
  70:	00 90 74 8a 	subu	%r18,%r19,%r20
  74:	00 a8 d7 8a 	subu	%r21,%r22,%r23
  78:	00 c0 3f 8b 	subu	%r24,%r25,%r31
  7c:	00 d8 9d 8b 	subu	%r27,%r28,%r29
  80:	00 f0 e0 8b 	subu	%r30,%r31,%r0
  84:	00 00 22 98 	subs	%r0,%r1,%sp
  88:	00 18 85 98 	subs	%fp,%r4,%r5
  8c:	00 30 e8 98 	subs	%r6,%r7,%r8
  90:	00 48 4b 99 	subs	%r9,%r10,%r11
  94:	00 f8 ae 99 	subs	%r31,%r13,%r14
  98:	00 78 11 9a 	subs	%r15,%r16,%r17
  9c:	00 90 74 9a 	subs	%r18,%r19,%r20
  a0:	00 a8 d7 9a 	subs	%r21,%r22,%r23
  a4:	00 c0 3f 9b 	subs	%r24,%r25,%r31
  a8:	00 d8 9d 9b 	subs	%r27,%r28,%r29
  ac:	00 f0 e0 9b 	subs	%r30,%r31,%r0
  b0:	00 00 22 84 	addu	0,%r1,%sp
  b4:	00 20 85 84 	addu	8192,%r4,%r5
  b8:	f5 13 e8 84 	addu	5109,%r7,%r8
  bc:	ff 7f 4b 85 	addu	32767,%r10,%r11
  c0:	00 80 ae 85 	addu	-32768,%r13,%r14
  c4:	00 e0 11 86 	addu	-8192,%r16,%r17
  c8:	ff ff 74 86 	addu	-1,%r19,%r20
  cc:	cd ab d7 86 	addu	-21555,%r22,%r23
  d0:	34 12 3a 87 	addu	4660,%r25,%r26
  d4:	00 00 9d 87 	addu	0,%r28,%r29
  d8:	03 00 e0 87 	addu	3,%r31,%r0
  dc:	00 00 22 94 	adds	0,%r1,%sp
  e0:	00 20 85 94 	adds	8192,%r4,%r5
  e4:	f5 13 e8 94 	adds	5109,%r7,%r8
  e8:	ff 7f 4b 95 	adds	32767,%r10,%r11
  ec:	00 80 ae 95 	adds	-32768,%r13,%r14
  f0:	00 e0 11 96 	adds	-8192,%r16,%r17
  f4:	ff ff 74 96 	adds	-1,%r19,%r20
  f8:	cd ab d7 96 	adds	-21555,%r22,%r23
  fc:	34 12 3a 97 	adds	4660,%r25,%r26
 100:	00 00 9d 97 	adds	0,%r28,%r29
 104:	03 00 e0 97 	adds	3,%r31,%r0
 108:	01 00 22 8c 	subu	1,%r1,%sp
 10c:	01 20 85 8c 	subu	8193,%r4,%r5
 110:	f6 13 e8 8c 	subu	5110,%r7,%r8
 114:	ff 7f 4b 8d 	subu	32767,%r10,%r11
 118:	00 80 ae 8d 	subu	-32768,%r13,%r14
 11c:	00 e0 11 8e 	subu	-8192,%r16,%r17
 120:	ff ff 74 8e 	subu	-1,%r19,%r20
 124:	cd ab d7 8e 	subu	-21555,%r22,%r23
 128:	34 12 3a 8f 	subu	4660,%r25,%r26
 12c:	00 00 9d 8f 	subu	0,%r28,%r29
 130:	03 00 e0 8f 	subu	3,%r31,%r0
 134:	01 00 22 9c 	subs	1,%r1,%sp
 138:	01 20 85 9c 	subs	8193,%r4,%r5
 13c:	f6 13 e8 9c 	subs	5110,%r7,%r8
 140:	ff 7f 4b 9d 	subs	32767,%r10,%r11
 144:	00 80 ae 9d 	subs	-32768,%r13,%r14
 148:	00 e0 11 9e 	subs	-8192,%r16,%r17
 14c:	ff ff 74 9e 	subs	-1,%r19,%r20
 150:	cd ab d7 9e 	subs	-21555,%r22,%r23
 154:	34 12 3a 9f 	subs	4660,%r25,%r26
 158:	00 00 9d 9f 	subs	0,%r28,%r29
 15c:	03 00 e0 9f 	subs	3,%r31,%r0
