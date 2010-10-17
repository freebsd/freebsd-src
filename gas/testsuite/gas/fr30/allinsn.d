#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

0+0000 <add>:
   0:	a6 01       	add r0,r1
   2:	a4 02       	add 0x0,r2

0+0004 <add2>:
   4:	a5 f3       	add2 -1,r3

0+0006 <addc>:
   6:	a7 45       	addc r4,r5

0+0008 <addn>:
   8:	a2 67       	addn r6,r7
   a:	a0 f8       	addn 0xf,r8

0+000c <addn2>:
   c:	a1 09       	addn2 -16,r9

0+000e <sub>:
   e:	ac ab       	sub r10,r11

0+0010 <subc>:
  10:	ad cd       	subc r12,r13

0+0012 <subn>:
  12:	ae ef       	subn r14,r15

0+0014 <cmp>:
  14:	aa de       	cmp r13,r14
  16:	a8 1f       	cmp 0x1,r15

0+0018 <cmp2>:
  18:	a9 10       	cmp2 -15,r0

0+001a <and>:
  1a:	82 12       	and r1,r2
  1c:	84 34       	and r3,@r4

0+001e <andh>:
  1e:	85 56       	andh r5,@r6

0+0020 <andb>:
  20:	86 78       	andb r7,@r8

0+0022 <or>:
  22:	92 9a       	or r9,r10
  24:	94 bc       	or r11,@r12

0+0026 <orh>:
  26:	95 de       	orh r13,@r14

0+0028 <orb>:
  28:	96 fd       	orb r15,@r13

0+002a <eor>:
  2a:	9a ef       	eor r14,r15
  2c:	9c 01       	eor r0,@r1

0+002e <eorh>:
  2e:	9d 23       	eorh r2,@r3

0+0030 <eorb>:
  30:	9e 45       	eorb r4,@r5

0+0032 <bandl>:
  32:	80 f6       	bandl 0xf,@r6

0+0034 <nadh>:
  34:	81 77       	bandh 0x7,@r7

0+0036 <borl>:
  36:	90 38       	borl 0x3,@r8

0+0038 <borh>:
  38:	91 d9       	borh 0xd,@r9

0+003a <beorl>:
  3a:	98 fa       	beorl 0xf,@r10

0+003c <beorh>:
  3c:	99 1b       	beorh 0x1,@r11

0+003e <btstl>:
  3e:	88 0c       	btstl 0x0,@r12

0+0040 <btsth>:
  40:	89 8d       	btsth 0x8,@r13

0+0042 <mul>:
  42:	af ef       	mul r14,r15

0+0044 <mulu>:
  44:	ab de       	mulu r13,r14

0+0046 <muluh>:
  46:	bb f0       	muluh r15,r0

0+0048 <mulh>:
  48:	bf 12       	mulh r1,r2

0+004a <div0s>:
  4a:	97 43       	div0s r3

0+004c <div0u>:
  4c:	97 54       	div0u r4

0+004e <div1>:
  4e:	97 65       	div1 r5

0+0050 <div2>:
  50:	97 76       	div2 r6

0+0052 <div3>:
  52:	9f 60       	div3

0+0054 <div4s>:
  54:	9f 70       	div4s

0+0056 <lsl>:
  56:	b6 78       	lsl r7,r8
  58:	b4 39       	lsl 0x3,r9

0+005a <lsl2>:
  5a:	b5 0a       	lsl2 0x0,r10

0+005c <lsr>:
  5c:	b2 bc       	lsr r11,r12
  5e:	b0 fd       	lsr 0xf,r13

0+0060 <lsr2>:
  60:	b1 fe       	lsr2 0xf,r14

0+0062 <asr>:
  62:	ba fd       	asr r15,r13
  64:	b8 6e       	asr 0x6,r14

0+0066 <asr2>:
  66:	b9 7f       	asr2 0x7,r15

0+0068 <ldi_8>:
  68:	cf f2       	ldi:8 0xff,r2

0+006a <ld>:
  6a:	04 34       	ld @r3,r4
  6c:	00 56       	ld @\(r13,r5\),r6
  6e:	27 f7       	ld @\(r14,508\),r7
  70:	03 f8       	ld @\(r15,0x3c\),r8
  72:	07 09       	ld @r15\+,r9
  74:	07 90       	ld @r15\+,ps
  76:	07 80       	ld @r15\+,tbr
  78:	07 81       	ld @r15\+,rp
  7a:	07 82       	ld @r15\+,ssp

0+007c <lduh>:
  7c:	05 ab       	lduh @r10,r11
  7e:	01 cd       	lduh @\(r13,r12\),r13
  80:	48 0f       	lduh @\(r14,-256\),r15

0+0082 <ldub>:
  82:	06 de       	ldub @r13,r14
  84:	02 f0       	ldub @\(r13,r15\),r0
  86:	68 01       	ldub @\(r14,-128\),r1

0+0088 <st>:
  88:	14 32       	st r2,@r3
  8a:	10 54       	st r4,@\(r13,r5\)
  8c:	38 06       	st r6,@\(r14,-512\)
  8e:	13 f7       	st r7,@\(r15,0x3c\)
  90:	17 08       	st r8,@-r15
  92:	17 84       	st mdh,@-r15
  94:	17 90       	st ps,@-r15

0+0096 <sth>:
  96:	15 a9       	sth r9,@r10
  98:	11 cb       	sth r11,@\(r13,r12\)
  9a:	54 0d       	sth r13,@\(r14,128\)

0+009c <stb>:
  9c:	16 fe       	stb r14,@r15
  9e:	12 10       	stb r0,@\(r13,r1\)
  a0:	78 02       	stb r2,@\(r14,-128\)

0+00a2 <mov>:
  a2:	8b 34       	mov r3,r4
  a4:	b7 55       	mov mdl,r5
  a6:	17 16       	mov ps,r6
  a8:	b3 37       	mov r7,usp
  aa:	07 18       	mov r8,ps

0+00ac <jmp>:
  ac:	97 09       	jmp @r9

0+00ae <ret>:
  ae:	97 20       	ret

0+00b0 <bra>:
  b0:	e0 a7       	bra 0 \<add\>

0+00b2 <bno>:
  b2:	e1 a6       	bno 0 \<add\>

0+00b4 <beq>:
  b4:	e2 a5       	beq 0 \<add\>

0+00b6 <bne>:
  b6:	e3 a4       	bne 0 \<add\>

0+00b8 <bc>:
  b8:	e4 a3       	bc 0 \<add\>

0+00ba <bnc>:
  ba:	e5 a2       	bnc 0 \<add\>

0+00bc <bn>:
  bc:	e6 a1       	bn 0 \<add\>

0+00be <bp>:
  be:	e7 a0       	bp 0 \<add\>

0+00c0 <bv>:
  c0:	e8 9f       	bv 0 \<add\>

0+00c2 <bnv>:
  c2:	e9 9e       	bnv 0 \<add\>

0+00c4 <blt>:
  c4:	ea 9d       	blt 0 \<add\>

0+00c6 <bge>:
  c6:	eb 9c       	bge 0 \<add\>

0+00c8 <ble>:
  c8:	ec 9b       	ble 0 \<add\>

0+00ca <bgt>:
  ca:	ed 9a       	bgt 0 \<add\>

0+00cc <bls>:
  cc:	ee 99       	bls 0 \<add\>

0+00ce <bhi>:
  ce:	ef 98       	bhi 0 \<add\>

0+00d0 <jmp_d>:
  d0:	9f 0b       	jmp:d @r11
  d2:	9f a0       	nop

0+00d4 <ret_d>:
  d4:	9f 20       	ret:d
  d6:	9f a0       	nop

0+00d8 <bra_d>:
  d8:	f0 fb       	bra:d d0 \<jmp_d\>
  da:	9f a0       	nop

0+00dc <bno_d>:
  dc:	f1 f9       	bno:d d0 \<jmp_d\>
  de:	9f a0       	nop

0+00e0 <beq_d>:
  e0:	f2 f7       	beq:d d0 \<jmp_d\>
  e2:	9f a0       	nop

0+00e4 <bne_d>:
  e4:	f3 f5       	bne:d d0 \<jmp_d\>
  e6:	9f a0       	nop

0+00e8 <bc_d>:
  e8:	f4 f3       	bc:d d0 \<jmp_d\>
  ea:	9f a0       	nop

0+00ec <bnc_d>:
  ec:	f5 f1       	bnc:d d0 \<jmp_d\>
  ee:	9f a0       	nop

0+00f0 <bn_d>:
  f0:	f6 ef       	bn:d d0 \<jmp_d\>
  f2:	9f a0       	nop

0+00f4 <bp_d>:
  f4:	f7 ed       	bp:d d0 \<jmp_d\>
  f6:	9f a0       	nop

0+00f8 <bv_d>:
  f8:	f8 eb       	bv:d d0 \<jmp_d\>
  fa:	9f a0       	nop

0+00fc <bnv_d>:
  fc:	f9 e9       	bnv:d d0 \<jmp_d\>
  fe:	9f a0       	nop

0+0100 <blt_d>:
 100:	fa e7       	blt:d d0 \<jmp_d\>
 102:	9f a0       	nop

0+0104 <bge_d>:
 104:	fb e5       	bge:d d0 \<jmp_d\>
 106:	9f a0       	nop

0+0108 <ble_d>:
 108:	fc e3       	ble:d d0 \<jmp_d\>
 10a:	9f a0       	nop

0+010c <bgt_d>:
 10c:	fd e1       	bgt:d d0 \<jmp_d\>
 10e:	9f a0       	nop

0+0110 <bls_d>:
 110:	fe df       	bls:d d0 \<jmp_d\>
 112:	9f a0       	nop

0+0114 <bhi_d>:
 114:	ff dd       	bhi:d d0 \<jmp_d\>
 116:	9f a0       	nop

0+0118 <ldres>:
 118:	bc 82       	ldres @r2\+,0x8

0+011a <stres>:
 11a:	bd f3       	stres 0xf,@r3\+

0+011c <nop>:
 11c:	9f a0       	nop

0+011e <andccr>:
 11e:	83 ff       	andccr 0xff

0+0120 <orccr>:
 120:	93 7d       	orccr 0x7d

0+0122 <stilm>:
 122:	87 61       	stilm 0x61

0+0124 <addsp>:
 124:	a3 80       	addsp -512

0+0126 <extsb>:
 126:	97 89       	extsb r9

0+0128 <extub>:
 128:	97 9a       	extub r10

0+012a <extsh>:
 12a:	97 ab       	extsh r11

0+012c <extuh>:
 12c:	97 bc       	extuh r12

0+012e <enter>:
 12e:	0f ff       	enter 0x3fc

0+0130 <leave>:
 130:	9f 90       	leave

0+0132 <xchb>:
 132:	8a ef       	xchb @r14,r15

0+0134 <ldi_32>:
 134:	9f 80 12 34 	ldi:32 0x12345678,r0
 138:	56 78 

0+013a <copop>:
 13a:	9f cf 01 34 	copop 0xf,0x1,cr3,cr4
 13e:	9f cf 04 56 	copop 0xf,0x4,cr5,cr6
 142:	9f cf ff 70 	copop 0xf,0xff,cr7,cr0

0+0146 <copld>:
 146:	9f d0 00 40 	copld 0x0,0x0,r4,cr0

0+014a <copst>:
 14a:	9f e7 02 15 	copst 0x7,0x2,cr1,r5

0+014e <copsv>:
 14e:	9f f8 03 26 	copsv 0x8,0x3,cr2,r6

0+0152 <ldm0>:
 152:	8c 8d       	ldm0 \(r0,r2,r3,r7\)

0+0154 <ldm1>:
 154:	8d 89       	ldm1 \(r8,r11,r15\)

0+0156 <stm0>:
 156:	8e 30       	stm0 \(r2,r3\)

0+0158 <stm1>:
 158:	8f 06       	stm1 \(r13,r14\)

0+015a <call>:
 15a:	d7 52       	call 0 \<add\>
 15c:	97 1a       	call @r10

0+015e <call_d>:
 15e:	df 50       	call:d 0 \<add\>
 160:	9f a0       	nop
 162:	9f 1c       	call:d @r12
 164:	9f a0       	nop

0+0166 <dmov>:
 166:	08 22       	dmov @0x88,r13
 168:	18 15       	dmov r13,@0x54
 16a:	0c 11       	dmov @0x44,@r13\+
 16c:	1c 00       	dmov @r13\+,@0x0
 16e:	0b 0b       	dmov @0x2c,@-r15
 170:	1b 09       	dmov @r15\+,@0x24

0+0172 <dmovh>:
 172:	09 44       	dmovh @0x88,r13
 174:	19 29       	dmovh r13,@0x52
 176:	0d 1a       	dmovh @0x34,@r13\+
 178:	1d 29       	dmovh @r13\+,@0x52

0+017a <dmovb>:
 17a:	0a 91       	dmovb @0x91,r13
 17c:	1a 53       	dmovb r13,@0x53
 17e:	0e 47       	dmovb @0x47,@r13\+
 180:	1e 00       	dmovb @r13\+,@0x0

0+0182 <ldi_20>:
 182:	9b f1 ff ff 	ldi:20 0xfffff,r1

0+0186 <finish>:
 186:	9f 80 00 00 	ldi:32 0x8000,r0
 18a:	80 00 
 18c:	b3 20       	mov r0,ssp
 18e:	9f 80 00 00 	ldi:32 0x1,r0
 192:	00 01 
 194:	1f 0a       	int 0xa

0+0196 <inte>:
 196:	9f 30       	inte

0+0198 <reti>:
 198:	97 30       	reti
