#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <add>:
   0:	00 00 00 20 	add r0,r0,r0

00000004 <addi>:
   4:	20 00 ff fc 	addi r0,r0,0xfffc

00000008 <addiu>:
   8:	24 00 00 04 	addiu r0,r0,0x4

0000000c <addu>:
   c:	00 00 00 21 	addu r0,r0,r0

00000010 <ado16>:
  10:	00 00 00 29 	ado16 r0,r0,r0

00000014 <and>:
  14:	00 00 00 24 	and r0,r0,r0

00000018 <andi>:
  18:	30 00 de ad 	andi r0,r0,0xdead

0000001c <andoi>:
  1c:	b0 00 00 00 	andoi r0,r0,0x0

00000020 <andoui>:
  20:	fc 00 00 00 	andoui r0,r0,0x0

00000024 <mrgb>:
  24:	00 00 00 2d 	mrgb r0,r0,r0,0x0

00000028 <nor>:
  28:	00 00 00 27 	nor r0,r0,r0

0000002c <or>:
  2c:	00 00 00 25 	or r0,r0,r0

00000030 <ori>:
  30:	34 00 ff ff 	ori r0,r0,0xffff

00000034 <orui>:
  34:	bc 00 00 00 	orui r0,r0,0x0

00000038 <ram>:
  38:	9c 00 00 00 	ram r0,r0,0x0,0x0,0x0

0000003c <sll>:
  3c:	00 00 00 00 	nop

00000040 <sllv>:
  40:	00 00 00 04 	sllv r0,r0,r0

00000044 <slmv>:
  44:	00 00 00 01 	slmv r0,r0,r0,0x0

00000048 <slt>:
  48:	00 00 00 2a 	slt r0,r0,r0

0000004c <slti>:
  4c:	28 00 00 00 	slti r0,r0,0x0

00000050 <sltiu>:
  50:	2c 00 00 00 	sltiu r0,r0,0x0

00000054 <sltu>:
  54:	00 00 00 2b 	sltu r0,r0,r0

00000058 <sra>:
  58:	00 00 00 03 	sra r0,r0,0x0

0000005c <srav>:
  5c:	00 00 00 07 	srav r0,r0,r0

00000060 <srl>:
  60:	00 00 00 02 	srl r0,r0,0x0

00000064 <srlv>:
  64:	00 00 00 06 	srlv r0,r0,r0

00000068 <srmv>:
  68:	00 00 00 05 	srmv r0,r0,r0,0x0

0000006c <sub>:
  6c:	00 00 00 22 	sub r0,r0,r0

00000070 <subu>:
  70:	00 00 00 23 	subu r0,r0,r0

00000074 <xor>:
  74:	00 00 00 26 	xor r0,r0,r0

00000078 <xori>:
  78:	38 00 00 00 	xori r0,r0,0x0

0000007c <bbi>:
  7c:	70 00 ff e0 	bbi r0\(0x0\),0 <add>

00000080 <bbin>:
  80:	78 00 ff df 	bbin r0\(0x0\),0 <add>

00000084 <bbv>:
  84:	74 00 ff de 	bbv r0,r0,0 <add>

00000088 <bbvn>:
  88:	7c 00 ff dd 	bbvn r0,r0,0 <add>

0000008c <beq>:
  8c:	10 00 ff dc 	beq r0,r0,0 <add>

00000090 <beql>:
  90:	50 00 ff db 	beql r0,r0,0 <add>

00000094 <bgez>:
  94:	04 01 ff da 	bgez r0,0 <add>

00000098 <bgezal>:
  98:	04 11 ff d9 	bgezal r0,0 <add>

0000009c <bgezall>:
  9c:	04 13 ff d8 	bgezall r0,0 <add>

000000a0 <bgezl>:
  a0:	04 03 ff d7 	bgezl r0,0 <add>

000000a4 <bgtz>:
  a4:	1c 00 ff d6 	bgtz r0,0 <add>

000000a8 <bgtzl>:
  a8:	5c 00 ff d5 	bgtzl r0,0 <add>

000000ac <blez>:
  ac:	18 00 ff d4 	blez r0,0 <add>

000000b0 <blezl>:
  b0:	58 00 ff d3 	blezl r0,0 <add>

000000b4 <bltz>:
  b4:	04 00 ff d2 	bltz r0,0 <add>

000000b8 <bltzl>:
  b8:	04 02 ff d1 	bltzl r0,0 <add>

000000bc <bltzal>:
  bc:	04 10 ff d0 	bltzal r0,0 <add>

000000c0 <bltzall>:
  c0:	04 12 ff cf 	bltzall r0,0 <add>

000000c4 <bmb>:
  c4:	b4 00 ff ce 	bmb r0,r0,0 <add>

000000c8 <bmb0>:
  c8:	60 00 ff cd 	bmb0 r0,r0,0 <add>

000000cc <bmb1>:
  cc:	64 00 ff cc 	bmb1 r0,r0,0 <add>

000000d0 <bmb2>:
  d0:	68 00 ff cb 	bmb2 r0,r0,0 <add>

000000d4 <bmb3>:
  d4:	6c 00 ff ca 	bmb3 r0,r0,0 <add>

000000d8 <bne>:
  d8:	14 00 ff c9 	bne r0,r0,0 <add>

000000dc <bnel>:
  dc:	54 00 ff c8 	bnel r0,r0,0 <add>

000000e0 <bctxt>:
  e0:	04 06 ff c7 	bctxt r0,0 <add>

000000e4 <bc0f>:
  e4:	41 00 ff c6 	bc0f 0 <add>

000000e8 <bc0fl>:
  e8:	41 02 ff c5 	bc0fl 0 <add>

000000ec <bc3f>:
  ec:	4d 00 ff c4 	bc3f 0 <add>

000000f0 <bc3fl>:
  f0:	4d 02 ff c3 	bc3fl 0 <add>

000000f4 <bc0t>:
  f4:	41 01 ff c2 	bc0t 0 <add>

000000f8 <bc0tl>:
  f8:	41 03 ff c1 	bc0tl 0 <add>

000000fc <bc3t>:
  fc:	4d 01 ff c0 	bc3t 0 <add>

00000100 <bc3tl>:
 100:	4d 03 ff bf 	bc3tl 0 <add>

00000104 <break>:
 104:	00 00 00 0d 	break

00000108 <cfc0>:
 108:	40 40 00 00 	cfc0 r0,r0

0000010c <cfc1>:
 10c:	44 40 00 00 	cfc1 r0,r0

00000110 <cfc2>:
 110:	48 40 00 00 	cfc2 r0,r0

00000114 <cfc3>:
 114:	4c 40 00 00 	cfc3 r0,r0

00000118 <chkhdr>:
 118:	4d 20 00 00 	chkhdr r0,r0

0000011c <ctc0>:
 11c:	40 c0 00 00 	ctc0 r0,r0

00000120 <ctc1>:
 120:	44 c0 00 00 	ctc1 r0,r0

00000124 <ctc2>:
 124:	48 c0 00 00 	ctc2 r0,r0

00000128 <ctc3>:
 128:	4c c0 00 00 	ctc3 r0,r0

0000012c <jcr>:
 12c:	00 00 00 0a 	jcr r0
 130:	00 00 00 00 	nop

00000134 <luc32>:
 134:	48 20 00 03 	luc32 r0,r0

00000138 <luc32l>:
 138:	48 20 00 07 	luc32l r0,r0

0000013c <luc64>:
 13c:	48 20 00 0b 	luc64 r0,r0

00000140 <luc64l>:
 140:	48 20 00 0f 	luc64l r0,r0

00000144 <luk>:
 144:	48 20 00 08 	luk r0,r0

00000148 <lulck>:
 148:	48 20 00 04 	lulck r0

0000014c <lum32>:
 14c:	48 20 00 02 	lum32 r0,r0

00000150 <lum32l>:
 150:	48 20 00 06 	lum32l r0,r0

00000154 <lum64>:
 154:	48 20 00 0a 	lum64 r0,r0

00000158 <lum64l>:
 158:	48 20 00 0e 	lum64l r0,r0

0000015c <lur>:
 15c:	48 20 00 01 	lur r0,r0

00000160 <lurl>:
 160:	48 20 00 05 	lurl r0,r0

00000164 <luulck>:
 164:	48 20 00 00 	luulck r0

00000168 <mfc0>:
 168:	40 00 00 00 	mfc0 r0,r0

0000016c <mfc1>:
 16c:	44 00 00 00 	mfc1 r0,r0

00000170 <mfc2>:
 170:	48 00 00 00 	mfc2 r0,r0

00000174 <mfc3>:
 174:	4c 00 00 00 	mfc3 r0,r0

00000178 <mtc0>:
 178:	40 80 00 00 	mtc0 r0,r0

0000017c <mtc1>:
 17c:	44 80 00 00 	mtc1 r0,r0

00000180 <mtc2>:
 180:	48 80 00 00 	mtc2 r0,r0

00000184 <mtc3>:
 184:	4c 80 00 00 	mtc3 r0,r0

00000188 <rb>:
 188:	4c 20 00 04 	rb r0,r0

0000018c <rbr1>:
 18c:	4f 00 00 00 	rbr1 r0,0x0,0x0

00000190 <rbr30>:
 190:	4f 40 00 00 	rbr30 r0,0x0,0x0

00000194 <rfe>:
 194:	42 00 00 10 	rfe

00000198 <rx>:
 198:	4c 20 00 06 	rx r0,r0

0000019c <rxr1>:
 19c:	4f 80 00 00 	rxr1 r0,0x0,0x0

000001a0 <rxr30>:
 1a0:	4f c0 00 00 	rxr30 r0,0x0,0x0

000001a4 <sleep>:
 1a4:	00 00 00 0e 	sleep

000001a8 <srrd>:
 1a8:	48 20 00 10 	srrd r0

000001ac <srrdl>:
 1ac:	48 20 00 14 	srrdl r0

000001b0 <srulck>:
 1b0:	48 20 00 16 	srulck r0

000001b4 <srwr>:
 1b4:	48 20 00 11 	srwr r0,r0

000001b8 <srwru>:
 1b8:	48 20 00 15 	srwru r0,r0

000001bc <syscall>:
 1bc:	00 00 00 0c 	syscall

000001c0 <trapqfl>:
 1c0:	4c 20 00 08 	trapqfl

000001c4 <trapqne>:
 1c4:	4c 20 00 09 	trapqne

000001c8 <wb>:
 1c8:	4c 20 00 00 	wb r0,r0

000001cc <wbu>:
 1cc:	4c 20 00 01 	wbu r0,r0

000001d0 <wbr1>:
 1d0:	4e 03 00 00 	wbr1 r3,0x0,0x0

000001d4 <wbr1u>:
 1d4:	4e 20 00 00 	wbr1u r0,0x0,0x0

000001d8 <wbr30>:
 1d8:	4e 40 00 00 	wbr30 r0,0x0,0x0

000001dc <wbr30u>:
 1dc:	4e 60 00 00 	wbr30u r0,0x0,0x0

000001e0 <wx>:
 1e0:	4c 20 00 02 	wx r0,r0

000001e4 <wxu>:
 1e4:	4c 20 00 03 	wxu r0,r0

000001e8 <wxr1>:
 1e8:	4e 80 00 00 	wxr1 r0,0x0,0x0

000001ec <wxr1u>:
 1ec:	4e a0 00 00 	wxr1u r0,0x0,0x0

000001f0 <wxr30>:
 1f0:	4e c0 00 00 	wxr30 r0,0x0,0x0

000001f4 <wxr30u>:
 1f4:	4e e0 00 00 	wxr30u r0,0x0,0x0

000001f8 <j>:
 1f8:	08 00 00 00 	j 0 <add>
			1f8: R_IQ2000_OFFSET_16	.text

000001fc <jal>:
 1fc:	0c 00 00 00 	jal 0 <add>
			1fc: R_IQ2000_OFFSET_16	.text

00000200 <jalr>:
 200:	00 00 00 09 	jalr r0,r0

00000204 <jr>:
 204:	00 00 00 08 	jr r0

00000208 <lb>:
 208:	80 00 10 24 	lb r0,0x1024\(r0\)

0000020c <lbu>:
 20c:	90 00 10 24 	lbu r0,0x1024\(r0\)

00000210 <ldw>:
 210:	c0 00 10 24 	ldw r0,0x1024\(r0\)

00000214 <lh>:
 214:	84 00 10 24 	lh r0,0x1024\(r0\)

00000218 <lhu>:
 218:	94 00 10 24 	lhu r0,0x1024\(r0\)

0000021c <lui>:
 21c:	3c 00 ff ff 	lui r0,0xffff

00000220 <lw>:
 220:	8c 00 10 24 	lw r0,0x1024\(r0\)

00000224 <sb>:
 224:	a0 00 10 24 	sb r0,0x1024\(r0\)

00000228 <sdw>:
 228:	e0 00 10 24 	sdw r0,0x1024\(r0\)

0000022c <sh>:
 22c:	a4 00 10 24 	sh r0,0x1024\(r0\)

00000230 <sw>:
 230:	ac 00 10 24 	sw r0,0x1024\(r0\)

00000234 <traprel>:
 234:	4c 20 00 0a 	traprel r0

00000238 <pkrl>:
 238:	4c 21 00 07 	pkrl r0,r1

0000023c <pkrlr1>:
 23c:	4f a0 00 00 	pkrlr1 r0,0x0,0x0

00000240 <pkrlr30>:
 240:	4f e0 00 00 	pkrlr30 r0,0x0,0x0
