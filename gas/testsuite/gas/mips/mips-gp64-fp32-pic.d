#objdump: -d -mmips:8000
#as: -mabi=o64 -march=8000 -EB -mfp32 -KPIC
#name: MIPS -mgp64 -mfp32 (SVR4 PIC)

.*: +file format.*

Disassembly of section .text:

0+000 <[^>]*>:
   0:	3c1c0000 	lui	gp,0x0
   4:	279c0000 	addiu	gp,gp,0
   8:	0399e021 	addu	gp,gp,t9
   c:	afbc0008 	sw	gp,8\(sp\)
  10:	009c2021 	addu	a0,a0,gp
  14:	3c041234 	lui	a0,0x1234
  18:	34845678 	ori	a0,a0,0x5678
  1c:	8f840000 	lw	a0,0\(gp\)
  20:	24840000 	addiu	a0,a0,0
  24:	8f840000 	lw	a0,0\(gp\)
  28:	24840000 	addiu	a0,a0,0
  2c:	8f840000 	lw	a0,0\(gp\)
  30:	248401d8 	addiu	a0,a0,472
  34:	10000068 	b	1d8 <[^>]*>
  38:	00000000 	nop
  3c:	8f990000 	lw	t9,0\(gp\)
  40:	273901d8 	addiu	t9,t9,472
  44:	0320f809 	jalr	t9
  48:	00000000 	nop
  4c:	8fbc0008 	lw	gp,8\(sp\)
  50:	8f840000 	lw	a0,0\(gp\)
  54:	24840000 	addiu	a0,a0,0
  58:	8c840000 	lw	a0,0\(a0\)
  5c:	8f840000 	lw	a0,0\(gp\)
  60:	24840000 	addiu	a0,a0,0
  64:	8c840000 	lw	a0,0\(a0\)
  68:	8f840000 	lw	a0,0\(gp\)
  6c:	248401d8 	addiu	a0,a0,472
  70:	8c840000 	lw	a0,0\(a0\)
  74:	8f840000 	lw	a0,0\(gp\)
  78:	24840000 	addiu	a0,a0,0
  7c:	dc840000 	ld	a0,0\(a0\)
  80:	8f840000 	lw	a0,0\(gp\)
  84:	24840000 	addiu	a0,a0,0
  88:	dc840000 	ld	a0,0\(a0\)
  8c:	8f840000 	lw	a0,0\(gp\)
  90:	248401d8 	addiu	a0,a0,472
  94:	dc840000 	ld	a0,0\(a0\)
  98:	8f810000 	lw	at,0\(gp\)
  9c:	24210000 	addiu	at,at,0
  a0:	ac240000 	sw	a0,0\(at\)
  a4:	8f810000 	lw	at,0\(gp\)
  a8:	24210000 	addiu	at,at,0
  ac:	ac240000 	sw	a0,0\(at\)
  b0:	8f810000 	lw	at,0\(gp\)
  b4:	24210000 	addiu	at,at,0
  b8:	fc240000 	sd	a0,0\(at\)
  bc:	8f810000 	lw	at,0\(gp\)
  c0:	24210000 	addiu	at,at,0
  c4:	fc240000 	sd	a0,0\(at\)
  c8:	8f810000 	lw	at,0\(gp\)
  cc:	24210000 	addiu	at,at,0
  d0:	80240000 	lb	a0,0\(at\)
  d4:	90210001 	lbu	at,1\(at\)
  d8:	00042200 	sll	a0,a0,0x8
  dc:	00812025 	or	a0,a0,at
  e0:	8f810000 	lw	at,0\(gp\)
  e4:	24210000 	addiu	at,at,0
  e8:	a0240001 	sb	a0,1\(at\)
  ec:	00042202 	srl	a0,a0,0x8
  f0:	a0240000 	sb	a0,0\(at\)
  f4:	90210001 	lbu	at,1\(at\)
  f8:	00042200 	sll	a0,a0,0x8
  fc:	00812025 	or	a0,a0,at
 100:	8f810000 	lw	at,0\(gp\)
 104:	24210000 	addiu	at,at,0
 108:	88240000 	lwl	a0,0\(at\)
 10c:	98240003 	lwr	a0,3\(at\)
 110:	8f810000 	lw	at,0\(gp\)
 114:	24210000 	addiu	at,at,0
 118:	a8240000 	swl	a0,0\(at\)
 11c:	b8240003 	swr	a0,3\(at\)
 120:	3404ffc0 	li	a0,0xffc0
 124:	000423bc 	dsll32	a0,a0,0xe
 128:	8f810000 	lw	at,0\(gp\)
 12c:	dc240000 	ld	a0,0\(at\)
 130:	3c013ff0 	lui	at,0x3ff0
 134:	44810800 	mtc1	at,\$f1
 138:	44800000 	mtc1	zero,\$f0
 13c:	8f810000 	lw	at,0\(gp\)
 140:	d4200008 	ldc1	\$f0,8\(at\)
 144:	64a40064 	daddiu	a0,a1,100
 148:	2c840001 	sltiu	a0,a0,1
 14c:	64a40064 	daddiu	a0,a1,100
 150:	0004202b 	sltu	a0,zero,a0
 154:	00a0202d 	move	a0,a1
 158:	8f840000 	lw	a0,0\(gp\)
 15c:	24840000 	addiu	a0,a0,0
 160:	8f840000 	lw	a0,0\(gp\)
 164:	24840000 	addiu	a0,a0,0
 168:	8f810000 	lw	at,0\(gp\)
 16c:	24210000 	addiu	at,at,0
 170:	68240000 	ldl	a0,0\(at\)
 174:	6c240007 	ldr	a0,7\(at\)
 178:	8f810000 	lw	at,0\(gp\)
 17c:	24210000 	addiu	at,at,0
 180:	b0240000 	sdl	a0,0\(at\)
 184:	b4240007 	sdr	a0,7\(at\)
 188:	34018000 	li	at,0x8000
 18c:	00010c38 	dsll	at,at,0x10
 190:	0081082a 	slt	at,a0,at
 194:	10200010 	beqz	at,1d8 <[^>]*>
 198:	00000000 	nop
 19c:	34018000 	li	at,0x8000
 1a0:	00010c78 	dsll	at,at,0x11
 1a4:	0081082b 	sltu	at,a0,at
 1a8:	1020000b 	beqz	at,1d8 <[^>]*>
 1ac:	00000000 	nop
 1b0:	34018000 	li	at,0x8000
 1b4:	00010c38 	dsll	at,at,0x10
 1b8:	0081082a 	slt	at,a0,at
 1bc:	14200006 	bnez	at,1d8 <[^>]*>
 1c0:	00000000 	nop
 1c4:	34018000 	li	at,0x8000
 1c8:	00010c78 	dsll	at,at,0x11
 1cc:	0081082b 	sltu	at,a0,at
 1d0:	14200001 	bnez	at,1d8 <[^>]*>
 1d4:	00000000 	nop

0+01d8 <[^>]*>:
	...
