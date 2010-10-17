#objdump: -d -mmips:8000
#as: -32 -march=8000 -EB -mgp32 -mfp64
#name: MIPS -mgp32 -mfp64

.*: +file format.*

Disassembly of section .text:

0+000 <[^>]*>:
   0:	3c041234 	lui	a0,0x1234
   4:	34845678 	ori	a0,a0,0x5678
   8:	2784c000 	addiu	a0,gp,-16384
   c:	3c040000 	lui	a0,0x0
  10:	24840000 	addiu	a0,a0,0
  14:	3c040000 	lui	a0,0x0
  18:	24840104 	addiu	a0,a0,260
  1c:	08000041 	j	104 <[^>]*>
  20:	0c000041 	jal	104 <[^>]*>
  24:	8f84c000 	lw	a0,-16384\(gp\)
  28:	3c040000 	lui	a0,0x0
  2c:	8c840000 	lw	a0,0\(a0\)
  30:	3c040000 	lui	a0,0x0
  34:	8c840104 	lw	a0,260\(a0\)
  38:	8f84c000 	lw	a0,-16384\(gp\)
  3c:	8f85c004 	lw	a1,-16380\(gp\)
  40:	3c010000 	lui	at,0x0
  44:	8c240000 	lw	a0,0\(at\)
  48:	8c250004 	lw	a1,4\(at\)
  4c:	3c010000 	lui	at,0x0
  50:	8c240104 	lw	a0,260\(at\)
  54:	8c250108 	lw	a1,264\(at\)
  58:	af84c000 	sw	a0,-16384\(gp\)
  5c:	3c010000 	lui	at,0x0
  60:	ac240000 	sw	a0,0\(at\)
  64:	af84c000 	sw	a0,-16384\(gp\)
  68:	af85c004 	sw	a1,-16380\(gp\)
  6c:	3c010000 	lui	at,0x0
  70:	ac240000 	sw	a0,0\(at\)
  74:	ac250004 	sw	a1,4\(at\)
  78:	3c010000 	lui	at,0x0
  7c:	24210000 	addiu	at,at,0
  80:	80240000 	lb	a0,0\(at\)
  84:	90210001 	lbu	at,1\(at\)
  88:	00042200 	sll	a0,a0,0x8
  8c:	00812025 	or	a0,a0,at
  90:	3c010000 	lui	at,0x0
  94:	24210000 	addiu	at,at,0
  98:	a0240001 	sb	a0,1\(at\)
  9c:	00042202 	srl	a0,a0,0x8
  a0:	a0240000 	sb	a0,0\(at\)
  a4:	90210001 	lbu	at,1\(at\)
  a8:	00042200 	sll	a0,a0,0x8
  ac:	00812025 	or	a0,a0,at
  b0:	3c010000 	lui	at,0x0
  b4:	24210000 	addiu	at,at,0
  b8:	88240000 	lwl	a0,0\(at\)
  bc:	98240003 	lwr	a0,3\(at\)
  c0:	3c010000 	lui	at,0x0
  c4:	24210000 	addiu	at,at,0
  c8:	a8240000 	swl	a0,0\(at\)
  cc:	b8240003 	swr	a0,3\(at\)
  d0:	3c043ff0 	lui	a0,0x3ff0
  d4:	00002821 	move	a1,zero
  d8:	3c010000 	lui	at,0x0
  dc:	8c240000 	lw	a0,0\(at\)
  e0:	8c250004 	lw	a1,4\(at\)
  e4:	d780c000 	ldc1	\$f0,-16384\(gp\)
  e8:	d780c008 	ldc1	\$f0,-16376\(gp\)
  ec:	24a40064 	addiu	a0,a1,100
  f0:	2c840001 	sltiu	a0,a0,1
  f4:	24a40064 	addiu	a0,a1,100
  f8:	0004202b 	sltu	a0,zero,a0
  fc:	00a02021 	move	a0,a1
 100:	46231040 	add.d	\$f1,\$f2,\$f3

0+0104 <[^>]*>:
	...
