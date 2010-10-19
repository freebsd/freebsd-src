#objdump: -d -mmips:8000
#as: -march=8000 -EB -mabi=32 -KPIC
#name: MIPS -mabi=32 (SVR4 PIC)

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
  30:	2484015c 	addiu	a0,a0,348
  34:	10000049 	b	15c <[^>]*>
  38:	00000000 	nop
  3c:	8f990000 	lw	t9,0\(gp\)
  40:	2739015c 	addiu	t9,t9,348
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
  6c:	2484015c 	addiu	a0,a0,348
  70:	8c840000 	lw	a0,0\(a0\)
  74:	8f810000 	lw	at,0\(gp\)
  78:	8c240000 	lw	a0,0\(at\)
  7c:	8c250004 	lw	a1,4\(at\)
  80:	8f810000 	lw	at,0\(gp\)
  84:	8c240000 	lw	a0,0\(at\)
  88:	8c250004 	lw	a1,4\(at\)
  8c:	8f810000 	lw	at,0\(gp\)
  90:	8c24015c 	lw	a0,348\(at\)
  94:	8c250160 	lw	a1,352\(at\)
  98:	8f810000 	lw	at,0\(gp\)
  9c:	24210000 	addiu	at,at,0
  a0:	ac240000 	sw	a0,0\(at\)
  a4:	8f810000 	lw	at,0\(gp\)
  a8:	24210000 	addiu	at,at,0
  ac:	ac240000 	sw	a0,0\(at\)
  b0:	8f810000 	lw	at,0\(gp\)
  b4:	ac240000 	sw	a0,0\(at\)
  b8:	ac250004 	sw	a1,4\(at\)
  bc:	8f810000 	lw	at,0\(gp\)
  c0:	ac240000 	sw	a0,0\(at\)
  c4:	ac250004 	sw	a1,4\(at\)
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
 120:	3c043ff0 	lui	a0,0x3ff0
 124:	00002821 	move	a1,zero
 128:	8f810000 	lw	at,0\(gp\)
 12c:	8c240000 	lw	a0,0\(at\)
 130:	8c250004 	lw	a1,4\(at\)
 134:	3c013ff0 	lui	at,0x3ff0
 138:	44810800 	mtc1	at,\$f1
 13c:	44800000 	mtc1	zero,\$f0
 140:	8f810000 	lw	at,0\(gp\)
 144:	d4200008 	ldc1	\$f0,8\(at\)
 148:	24a40064 	addiu	a0,a1,100
 14c:	2c840001 	sltiu	a0,a0,1
 150:	24a40064 	addiu	a0,a1,100
 154:	0004202b 	sltu	a0,zero,a0
 158:	00a02021 	move	a0,a1

0+015c <[^>]*>:
	...
