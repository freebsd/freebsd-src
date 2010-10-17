#objdump: -d -mmips:8000
#as: -mabi=o64 -march=8000 -EB -KPIC
#name: MIPS -mgp64 -mfp64 (SVR4 PIC)

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
  20:	00000000 	nop
  24:	24840000 	addiu	a0,a0,0
  28:	8f840000 	lw	a0,0\(gp\)
  2c:	00000000 	nop
  30:	24840000 	addiu	a0,a0,0
  34:	8f840000 	lw	a0,0\(gp\)
  38:	00000000 	nop
  3c:	24840234 	addiu	a0,a0,564
  40:	1000007c 	b	234 <[^>]*>
  44:	00000000 	nop
  48:	8f990000 	lw	t9,0\(gp\)
  4c:	00000000 	nop
  50:	27390234 	addiu	t9,t9,564
  54:	0320f809 	jalr	t9
  58:	00000000 	nop
  5c:	8fbc0008 	lw	gp,8\(sp\)
  60:	8f840000 	lw	a0,0\(gp\)
  64:	00000000 	nop
  68:	24840000 	addiu	a0,a0,0
  6c:	8c840000 	lw	a0,0\(a0\)
  70:	8f840000 	lw	a0,0\(gp\)
  74:	00000000 	nop
  78:	24840000 	addiu	a0,a0,0
  7c:	8c840000 	lw	a0,0\(a0\)
  80:	8f840000 	lw	a0,0\(gp\)
  84:	00000000 	nop
  88:	24840234 	addiu	a0,a0,564
  8c:	8c840000 	lw	a0,0\(a0\)
  90:	8f840000 	lw	a0,0\(gp\)
  94:	00000000 	nop
  98:	24840000 	addiu	a0,a0,0
  9c:	dc840000 	ld	a0,0\(a0\)
  a0:	8f840000 	lw	a0,0\(gp\)
  a4:	00000000 	nop
  a8:	24840000 	addiu	a0,a0,0
  ac:	dc840000 	ld	a0,0\(a0\)
  b0:	8f840000 	lw	a0,0\(gp\)
  b4:	00000000 	nop
  b8:	24840234 	addiu	a0,a0,564
  bc:	dc840000 	ld	a0,0\(a0\)
  c0:	8f810000 	lw	at,0\(gp\)
  c4:	00000000 	nop
  c8:	24210000 	addiu	at,at,0
  cc:	ac240000 	sw	a0,0\(at\)
  d0:	8f810000 	lw	at,0\(gp\)
  d4:	00000000 	nop
  d8:	24210000 	addiu	at,at,0
  dc:	ac240000 	sw	a0,0\(at\)
  e0:	8f810000 	lw	at,0\(gp\)
  e4:	00000000 	nop
  e8:	24210000 	addiu	at,at,0
  ec:	fc240000 	sd	a0,0\(at\)
  f0:	8f810000 	lw	at,0\(gp\)
  f4:	00000000 	nop
  f8:	24210000 	addiu	at,at,0
  fc:	fc240000 	sd	a0,0\(at\)
 100:	8f810000 	lw	at,0\(gp\)
 104:	00000000 	nop
 108:	24210000 	addiu	at,at,0
 10c:	80240000 	lb	a0,0\(at\)
 110:	90210001 	lbu	at,1\(at\)
 114:	00042200 	sll	a0,a0,0x8
 118:	00812025 	or	a0,a0,at
 11c:	8f810000 	lw	at,0\(gp\)
 120:	00000000 	nop
 124:	24210000 	addiu	at,at,0
 128:	a0240001 	sb	a0,1\(at\)
 12c:	00042202 	srl	a0,a0,0x8
 130:	a0240000 	sb	a0,0\(at\)
 134:	90210001 	lbu	at,1\(at\)
 138:	00042200 	sll	a0,a0,0x8
 13c:	00812025 	or	a0,a0,at
 140:	8f810000 	lw	at,0\(gp\)
 144:	00000000 	nop
 148:	24210000 	addiu	at,at,0
 14c:	88240000 	lwl	a0,0\(at\)
 150:	98240003 	lwr	a0,3\(at\)
 154:	8f810000 	lw	at,0\(gp\)
 158:	00000000 	nop
 15c:	24210000 	addiu	at,at,0
 160:	a8240000 	swl	a0,0\(at\)
 164:	b8240003 	swr	a0,3\(at\)
 168:	3404ffc0 	li	a0,0xffc0
 16c:	000423bc 	dsll32	a0,a0,0xe
 170:	8f810000 	lw	at,0\(gp\)
 174:	dc240000 	ld	a0,0\(at\)
 178:	3401ffc0 	li	at,0xffc0
 17c:	00010bbc 	dsll32	at,at,0xe
 180:	44a10000 	dmtc1	at,\$f0
 184:	8f810000 	lw	at,0\(gp\)
 188:	d4200008 	ldc1	\$f0,8\(at\)
 18c:	64a40064 	daddiu	a0,a1,100
 190:	2c840001 	sltiu	a0,a0,1
 194:	64a40064 	daddiu	a0,a1,100
 198:	0004202b 	sltu	a0,zero,a0
 19c:	00a0202d 	move	a0,a1
 1a0:	8f840000 	lw	a0,0\(gp\)
 1a4:	00000000 	nop
 1a8:	24840000 	addiu	a0,a0,0
 1ac:	8f840000 	lw	a0,0\(gp\)
 1b0:	00000000 	nop
 1b4:	24840000 	addiu	a0,a0,0
 1b8:	8f810000 	lw	at,0\(gp\)
 1bc:	00000000 	nop
 1c0:	24210000 	addiu	at,at,0
 1c4:	68240000 	ldl	a0,0\(at\)
 1c8:	6c240007 	ldr	a0,7\(at\)
 1cc:	8f810000 	lw	at,0\(gp\)
 1d0:	00000000 	nop
 1d4:	24210000 	addiu	at,at,0
 1d8:	b0240000 	sdl	a0,0\(at\)
 1dc:	b4240007 	sdr	a0,7\(at\)
 1e0:	34018000 	li	at,0x8000
 1e4:	00010c38 	dsll	at,at,0x10
 1e8:	0081082a 	slt	at,a0,at
 1ec:	10200011 	beqz	at,234 <[^>]*>
 1f0:	00000000 	nop
 1f4:	34018000 	li	at,0x8000
 1f8:	00010c78 	dsll	at,at,0x11
 1fc:	0081082b 	sltu	at,a0,at
 200:	1020000c 	beqz	at,234 <[^>]*>
 204:	00000000 	nop
 208:	34018000 	li	at,0x8000
 20c:	00010c38 	dsll	at,at,0x10
 210:	0081082a 	slt	at,a0,at
 214:	14200007 	bnez	at,234 <[^>]*>
 218:	00000000 	nop
 21c:	34018000 	li	at,0x8000
 220:	00010c78 	dsll	at,at,0x11
 224:	0081082b 	sltu	at,a0,at
 228:	14200002 	bnez	at,234 <[^>]*>
 22c:	00000000 	nop
 230:	46231040 	add.d	\$f1,\$f2,\$f3

0+0234 <[^>]*>:
	...
