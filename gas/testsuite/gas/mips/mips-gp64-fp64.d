#objdump: -d -mmips:8000
#as: -mabi=o64 -march=8000 -EB
#name: MIPS -mgp64 -mfp64
#stderr: mips-gp64-fp32.l

.*: +file format.*

Disassembly of section .text:

0+000 <[^>]*>:
   0:	3c041234 	lui	a0,0x1234
   4:	34845678 	ori	a0,a0,0x5678
   8:	2784c000 	addiu	a0,gp,-16384
   c:	3c040000 	lui	a0,0x0
  10:	24840000 	addiu	a0,a0,0
  14:	3c040000 	lui	a0,0x0
  18:	24840160 	addiu	a0,a0,352
  1c:	08000058 	j	160 <[^>]*>
  20:	0c000058 	jal	160 <[^>]*>
  24:	8f84c000 	lw	a0,-16384\(gp\)
  28:	3c040000 	lui	a0,0x0
  2c:	8c840000 	lw	a0,0\(a0\)
  30:	3c040000 	lui	a0,0x0
  34:	8c840160 	lw	a0,352\(a0\)
  38:	df84c000 	ld	a0,-16384\(gp\)
  3c:	3c040000 	lui	a0,0x0
  40:	dc840000 	ld	a0,0\(a0\)
  44:	3c040000 	lui	a0,0x0
  48:	dc840160 	ld	a0,352\(a0\)
  4c:	af84c000 	sw	a0,-16384\(gp\)
  50:	3c010000 	lui	at,0x0
  54:	ac240000 	sw	a0,0\(at\)
  58:	ff84c000 	sd	a0,-16384\(gp\)
  5c:	3c010000 	lui	at,0x0
  60:	fc240000 	sd	a0,0\(at\)
  64:	3c010000 	lui	at,0x0
  68:	24210000 	addiu	at,at,0
  6c:	80240000 	lb	a0,0\(at\)
  70:	90210001 	lbu	at,1\(at\)
  74:	00042200 	sll	a0,a0,0x8
  78:	00812025 	or	a0,a0,at
  7c:	3c010000 	lui	at,0x0
  80:	24210000 	addiu	at,at,0
  84:	a0240001 	sb	a0,1\(at\)
  88:	00042202 	srl	a0,a0,0x8
  8c:	a0240000 	sb	a0,0\(at\)
  90:	90210001 	lbu	at,1\(at\)
  94:	00042200 	sll	a0,a0,0x8
  98:	00812025 	or	a0,a0,at
  9c:	3c010000 	lui	at,0x0
  a0:	24210000 	addiu	at,at,0
  a4:	88240000 	lwl	a0,0\(at\)
  a8:	98240003 	lwr	a0,3\(at\)
  ac:	3c010000 	lui	at,0x0
  b0:	24210000 	addiu	at,at,0
  b4:	a8240000 	swl	a0,0\(at\)
  b8:	b8240003 	swr	a0,3\(at\)
  bc:	3404ffc0 	li	a0,0xffc0
  c0:	000423bc 	dsll32	a0,a0,0xe
  c4:	3c010000 	lui	at,0x0
  c8:	dc240000 	ld	a0,0\(at\)
  cc:	3401ffc0 	li	at,0xffc0
  d0:	00010bbc 	dsll32	at,at,0xe
  d4:	44a10000 	dmtc1	at,\$f0
  d8:	d780c000 	ldc1	\$f0,-16384\(gp\)
  dc:	64a40064 	daddiu	a0,a1,100
  e0:	2c840001 	sltiu	a0,a0,1
  e4:	64a40064 	daddiu	a0,a1,100
  e8:	0004202b 	sltu	a0,zero,a0
  ec:	00a0202d 	move	a0,a1
  f0:	2784c000 	addiu	a0,gp,-16384
  f4:	3c040000 	lui	a0,0x0
  f8:	24840000 	addiu	a0,a0,0
  fc:	3c010000 	lui	at,0x0
 100:	24210000 	addiu	at,at,0
 104:	68240000 	ldl	a0,0\(at\)
 108:	6c240007 	ldr	a0,7\(at\)
 10c:	3c010000 	lui	at,0x0
 110:	24210000 	addiu	at,at,0
 114:	b0240000 	sdl	a0,0\(at\)
 118:	b4240007 	sdr	a0,7\(at\)
 11c:	34018000 	li	at,0x8000
 120:	00010c38 	dsll	at,at,0x10
 124:	0081082a 	slt	at,a0,at
 128:	1020000d 	beqz	at,160 <[^>]*>
 12c:	34018000 	li	at,0x8000
 130:	00010c78 	dsll	at,at,0x11
 134:	0081082b 	sltu	at,a0,at
 138:	10200009 	beqz	at,160 <[^>]*>
 13c:	34018000 	li	at,0x8000
 140:	00010c38 	dsll	at,at,0x10
 144:	0081082a 	slt	at,a0,at
 148:	14200005 	bnez	at,160 <[^>]*>
 14c:	34018000 	li	at,0x8000
 150:	00010c78 	dsll	at,at,0x11
 154:	0081082b 	sltu	at,a0,at
 158:	14200001 	bnez	at,160 <[^>]*>
 15c:	46231040 	add.d	\$f1,\$f2,\$f3

0+0160 <[^>]*>:
	...
