#as: -m10
#objdump: -drz
#name: q10allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <add>:
   0:	03 be 00 20 	add r0,r29,r30

00000004 <addi>:
   4:	20 00 ff fc 	addi r0,r0,0xfffc

00000008 <addiu>:
   8:	24 00 00 04 	addiu r0,r0,0x4

0000000c <addu>:
   c:	03 be 00 21 	addu r0,r29,r30

00000010 <ado16>:
  10:	03 be 00 29 	ado16 r0,r29,r30

00000014 <and>:
  14:	03 be 00 24 	and r0,r29,r30

00000018 <andi>:
  18:	30 00 de ad 	andi r0,r0,0xdead

0000001c <andoi>:
  1c:	b0 00 00 00 	andoi r0,r0,0x0

00000020 <andoui>:
  20:	bc 00 00 00 	andoui r0,r0,0x0

00000024 <mrgb>:
  24:	03 a0 00 2d 	mrgb r0,r29,r0,0x0

00000028 <nor>:
  28:	03 be 00 27 	nor r0,r29,r30

0000002c <or>:
  2c:	03 be 00 25 	or r0,r29,r30
  30:	03 be 08 25 	or r1,r29,r30

00000034 <ori>:
  34:	34 00 ff ff 	ori r0,r0,0xffff

00000038 <orui>:
  38:	3c 20 00 00 	orui r0,r1,0x0

0000003c <ram>:
  3c:	9c 00 00 00 	ram r0,r0,0x0,0x0,0x0

00000040 <sll>:
  40:	00 00 00 00 	nop
  44:	00 02 08 00 	sll r1,r2,0x0

00000048 <sllv>:
  48:	03 dd 00 04 	sllv r0,r29,r30

0000004c <slmv>:
  4c:	00 00 00 01 	slmv r0,r0,r0,0x0

00000050 <slt>:
  50:	03 be 00 2a 	slt r0,r29,r30

00000054 <slti>:
  54:	28 00 00 00 	slti r0,r0,0x0

00000058 <sltiu>:
  58:	2c 00 00 00 	sltiu r0,r0,0x0

0000005c <sltu>:
  5c:	03 be 00 2b 	sltu r0,r29,r30

00000060 <sra>:
  60:	00 00 00 03 	sra r0,r0,0x0

00000064 <srav>:
  64:	03 dd 00 07 	srav r0,r29,r30

00000068 <srl>:
  68:	00 00 00 02 	srl r0,r0,0x0

0000006c <srlv>:
  6c:	03 dd 00 06 	srlv r0,r29,r30

00000070 <srmv>:
  70:	00 00 00 05 	srmv r0,r0,r0,0x0

00000074 <sub>:
  74:	03 be 00 22 	sub r0,r29,r30

00000078 <subu>:
  78:	03 be 00 23 	subu r0,r29,r30

0000007c <xor>:
  7c:	00 00 00 26 	xor r0,r0,r0

00000080 <xori>:
  80:	38 00 00 00 	xori r0,r0,0x0

00000084 <bbi>:
  84:	70 00 ff ff 	bbi r0\(0x0\),84 <bbi>

00000088 <bbil>:
  88:	f0 00 ff fe 	bbil r0\(0x0\),84 <bbi>

0000008c <bbinl>:
  8c:	f8 00 ff fd 	bbinl r0\(0x0\),84 <bbi>

00000090 <bbin>:
  90:	78 00 ff fc 	bbin r0\(0x0\),84 <bbi>

00000094 <bbv>:
  94:	74 00 ff fb 	bbv r0,r0,84 <bbi>

00000098 <bbvl>:
  98:	f4 00 ff fa 	bbvl r0,r0,84 <bbi>

0000009c <bbvn>:
  9c:	7c 00 ff f9 	bbvn r0,r0,84 <bbi>

000000a0 <bbvnl>:
  a0:	fc 00 ff f8 	bbvnl r0,r0,84 <bbi>

000000a4 <beq>:
  a4:	10 00 ff f7 	beq r0,r0,84 <bbi>

000000a8 <beql>:
  a8:	50 00 ff f6 	beql r0,r0,84 <bbi>

000000ac <bgez>:
  ac:	04 01 ff f5 	bgez r0,84 <bbi>

000000b0 <bgezal>:
  b0:	04 11 ff f4 	bgezal r0,84 <bbi>

000000b4 <bgezall>:
  b4:	04 13 ff f3 	bgezall r0,84 <bbi>

000000b8 <bgezl>:
  b8:	04 03 ff f2 	bgezl r0,84 <bbi>

000000bc <bgtz>:
  bc:	04 05 ff f1 	bgtz r0,84 <bbi>

000000c0 <bgtzal>:
  c0:	04 15 ff f0 	bgtzal r0,84 <bbi>

000000c4 <bgtzall>:
  c4:	04 17 ff ef 	bgtzall r0,84 <bbi>

000000c8 <bgtzl>:
  c8:	04 07 ff ee 	bgtzl r0,84 <bbi>

000000cc <blez>:
  cc:	04 04 ff ed 	blez r0,84 <bbi>

000000d0 <blezal>:
  d0:	04 14 ff ec 	blezal r0,84 <bbi>

000000d4 <blezall>:
  d4:	04 16 ff eb 	blezall r0,84 <bbi>

000000d8 <blezl>:
  d8:	04 06 ff ea 	blezl r0,84 <bbi>

000000dc <bltz>:
  dc:	04 00 ff e9 	bltz r0,84 <bbi>

000000e0 <bltzl>:
  e0:	04 02 ff e8 	bltzl r0,84 <bbi>

000000e4 <bltzal>:
  e4:	04 10 ff e7 	bltzal r0,84 <bbi>

000000e8 <bltzall>:
  e8:	04 12 ff e6 	bltzall r0,84 <bbi>

000000ec <bmb>:
  ec:	18 00 ff e5 	bmb r0,r0,84 <bbi>

000000f0 <bmb0>:
  f0:	60 00 ff e4 	bmb0 r0,r0,84 <bbi>

000000f4 <bmb1>:
  f4:	64 00 ff e3 	bmb1 r0,r0,84 <bbi>

000000f8 <bmb2>:
  f8:	68 00 ff e2 	bmb2 r0,r0,84 <bbi>

000000fc <bmb3>:
  fc:	6c 00 ff e1 	bmb3 r0,r0,84 <bbi>

00000100 <bmbl>:
 100:	58 00 ff e0 	bmbl r0,r0,84 <bbi>

00000104 <bne>:
 104:	14 00 ff df 	bne r0,r0,84 <bbi>

00000108 <bnel>:
 108:	54 00 ff de 	bnel r0,r0,84 <bbi>

0000010c <break>:
 10c:	00 00 00 0d 	break

00000110 <bri>:
 110:	04 08 ff dc 	bri r0,84 <bbi>

00000114 <brv>:
 114:	04 09 ff db 	brv r0,84 <bbi>

00000118 <chkhdr>:
 118:	4c 00 00 26 	chkhdr r0,r0

0000011c <j>:
 11c:	08 00 00 00 	j 0 <add>
			11c: R_IQ2000_OFFSET_16	.text\+0x124

00000120 <jal>:
 120:	0c 00 00 00 	jal r0,0 <add>
			120: R_IQ2000_OFFSET_16	.text\+0x124

00000124 <jalr>:
 124:	00 00 00 09 	jalr r0,r0

00000128 <jr>:
 128:	00 00 00 08 	jr r0

0000012c <lb>:
 12c:	80 00 10 24 	lb r0,0x1024\(r0\)

00000130 <lbu>:
 130:	90 00 10 24 	lbu r0,0x1024\(r0\)

00000134 <lh>:
 134:	84 00 10 24 	lh r0,0x1024\(r0\)

00000138 <lhu>:
 138:	94 00 10 24 	lhu r0,0x1024\(r0\)

0000013c <lui>:
 13c:	3c 00 ff ff 	lui r0,0xffff
 140:	3c 1d 00 00 	lui r29,0x0
			140: R_IQ2000_HI16	foodata
 144:	37 bd 00 00 	ori r29,r29,0x0
			144: R_IQ2000_LO16	foodata

00000148 <la>:
 148:	3c 0b 00 00 	lui r11,0x0
			148: R_IQ2000_HI16	foodata
 14c:	35 6b 00 00 	ori r11,r11,0x0
			14c: R_IQ2000_LO16	foodata

00000150 <lw>:
 150:	8c 00 10 24 	lw r0,0x1024\(r0\)

00000154 <sb>:
 154:	a0 00 10 24 	sb r0,0x1024\(r0\)

00000158 <sh>:
 158:	a4 00 10 24 	sh r0,0x1024\(r0\)

0000015c <sw>:
 15c:	ac 00 10 24 	sw r0,0x1024\(r0\)

00000160 <swrd>:
 160:	4c 1e e8 04 	swrd r29,r30

00000164 <swrdl>:
 164:	4c 1e e8 05 	swrdl r29,r30

00000168 <swwr>:
 168:	4f be 00 06 	swwr r0,r29,r30

0000016c <swwru>:
 16c:	4f be 00 07 	swwru r0,r29,r30

00000170 <rba>:
 170:	4f be 00 08 	rba r0,r29,r30

00000174 <rbal>:
 174:	4f be 00 09 	rbal r0,r29,r30

00000178 <rbar>:
 178:	4f be 00 0a 	rbar r0,r29,r30

0000017c <dwrd>:
 17c:	4c 1e e0 0c 	dwrd r28,r30

00000180 <dwrdl>:
 180:	4c 1e e0 0d 	dwrdl r28,r30

00000184 <wba>:
 184:	4f be 00 10 	wba r0,r29,r30

00000188 <wbau>:
 188:	4f be 00 11 	wbau r0,r29,r30

0000018c <wbac>:
 18c:	4f be 00 12 	wbac r0,r29,r30

00000190 <crc32>:
 190:	4f be 00 14 	crc32 r0,r29,r30

00000194 <crc32b>:
 194:	4f be 00 15 	crc32b r0,r29,r30

00000198 <cfc>:
 198:	4c 1e e8 00 	cfc r29,r30

0000019c <lock>:
 19c:	4c 1c e8 01 	lock r29,r28

000001a0 <ctc>:
 1a0:	4f be 00 02 	ctc r29,r30

000001a4 <unlk>:
 1a4:	4c 1e e8 03 	unlk r29,r30

000001a8 <mcid>:
 1a8:	4c 1d 00 20 	mcid r0,r29

000001ac <dba>:
 1ac:	4c 00 f0 22 	dba r30

000001b0 <dbd>:
 1b0:	4c 1e 00 21 	dbd r0,r0,r30

000001b4 <dpwt>:
 1b4:	4f c0 00 23 	dpwt r0,r30

000001b8 <avail>:
 1b8:	4c 00 f8 24 	avail r31

000001bc <free>:
 1bc:	4f c0 00 25 	free r0,r30

000001c0 <tstod>:
 1c0:	4f c0 00 27 	tstod r0,r30

000001c4 <yield>:
 1c4:	00 00 00 0e 	yield

000001c8 <pkrla>:
 1c8:	4f be 00 28 	pkrla r0,r29,r30

000001cc <pkrlac>:
 1cc:	4f be 00 2b 	pkrlac r0,r29,r30

000001d0 <pkrlau>:
 1d0:	4f be 00 29 	pkrlau r0,r29,r30

000001d4 <pkrlah>:
 1d4:	4f be 00 2a 	pkrlah r0,r29,r30

000001d8 <cmphdr>:
 1d8:	4c 00 f8 2c 	cmphdr r31

000001dc <cam36>:
 1dc:	4c 1e ec 09 	cam36 r29,r30,0x1,0x1

000001e0 <cam72>:
 1e0:	4c 1e 04 52 	cam72 r0,r30,0x2,0x2

000001e4 <cam144>:
 1e4:	4c 1d 04 9b 	cam144 r0,r29,0x3,0x3

000001e8 <cam288>:
 1e8:	4c 1d 04 a4 	cam144 r0,r29,0x4,0x4

000001ec <cm32and>:
 1ec:	4f be 00 ab 	cm32and r0,r29,r30

000001f0 <cm32andn>:
 1f0:	4f be 00 a3 	cm32andn r0,r29,r30

000001f4 <cm32or>:
 1f4:	4f be 00 aa 	cm32or r0,r29,r30

000001f8 <cm32ra>:
 1f8:	4f be 00 b0 	cm32ra r0,r29,r30

000001fc <cm32rd>:
 1fc:	4c 1e e8 a1 	cm32rd r29,r30

00000200 <cm32ri>:
 200:	4c 1d 00 a4 	cm32ri r0,r29

00000204 <cm32rs>:
 204:	4f be 00 a0 	cm32rs r0,r29,r30

00000208 <cm32sa>:
 208:	4f be 00 b8 	cm32sa r0,r29,r30

0000020c <cm32sd>:
 20c:	4c 1d 00 a9 	cm32sd r0,r29

00000210 <cm32si>:
 210:	4c 1d 00 ac 	cm32si r0,r29

00000214 <cm32ss>:
 214:	4f be 00 a8 	cm32ss r0,r29,r30

00000218 <cm32xor>:
 218:	4f be 00 a2 	cm32xor r0,r29,r30

0000021c <cm64clr>:
 21c:	4c 1c 00 85 	cm64clr r0,r28

00000220 <cm64ra>:
 220:	4f 9e 00 90 	cm64ra r0,r28,r30

00000224 <cm64rd>:
 224:	4c 1c 00 81 	cm64rd r0,r28

00000228 <cm64ri>:
 228:	4c 1c 00 84 	cm64ri r0,r28

0000022c <cm64ria2>:
 22c:	4f 9e 00 94 	cm64ria2 r0,r28,r30

00000230 <cm64rs>:
 230:	4f 9e 00 80 	cm64rs r0,r28,r30

00000234 <cm64sa>:
 234:	4f 9e 00 98 	cm64sa r0,r28,r30

00000238 <cm64sd>:
 238:	4c 1c 00 89 	cm64sd r0,r28

0000023c <cm64si>:
 23c:	4c 1c 00 8c 	cm64si r0,r28

00000240 <cm64sia2>:
 240:	4f 9e 00 9c 	cm64sia2 r0,r28,r30

00000244 <cm64ss>:
 244:	4f be 00 88 	cm64ss r0,r29,r30

00000248 <cm128ria2>:
 248:	4f be 00 95 	cm128ria2 r0,r29,r30

0000024c <cm128ria3>:
 24c:	4f be 00 90 	cm64ra r0,r29,r30

00000250 <cm128ria4>:
 250:	4f be 00 b7 	cm128ria4 r0,r29,r30,0x7

00000254 <cm128sia2>:
 254:	4f be 00 9d 	cm128sia2 r0,r29,r30

00000258 <cm128sia3>:
 258:	4f be 00 98 	cm64sa r0,r29,r30

0000025c <cm128sia4>:
 25c:	4f be 00 bf 	cm128sia4 r0,r29,r30,0x7

00000260 <cm128vsa>:
 260:	4f be 00 a6 	cm128vsa r0,r29,r30

00000264 <pkrli>:
 264:	4b fd 08 3f 	pkrli r1,r31,r29,0x3f

00000268 <pkrlic>:
 268:	4b fd 0b 3f 	pkrlic r1,r31,r29,0x3f

0000026c <pkrlih>:
 26c:	4b fd 0a 3f 	pkrlih r1,r31,r29,0x3f

00000270 <pkrliu>:
 270:	4b fd 09 3f 	pkrliu r1,r31,r29,0x3f

00000274 <rbi>:
 274:	4f bc 12 20 	rbi r2,r29,r28,0x20

00000278 <rbil>:
 278:	4f bc 13 20 	rbil r2,r29,r28,0x20

0000027c <rbir>:
 27c:	4f bc 11 20 	rbir r2,r29,r28,0x20

00000280 <wbi>:
 280:	4c 22 06 20 	wbi r0,r1,r2,0x20

00000284 <wbic>:
 284:	4c 22 05 20 	wbic r0,r1,r2,0x20

00000288 <wbiu>:
 288:	4c 22 07 20 	wbiu r0,r1,r2,0x20
