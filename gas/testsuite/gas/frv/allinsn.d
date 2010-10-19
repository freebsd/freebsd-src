#as:
#objdump: -dr
#name: 

.*: +file format .*

Disassembly of section .text:

00000000 <add>:
   0:	82 00 10 01 	add sp,sp,sp

00000004 <sub>:
   4:	82 00 11 01 	sub sp,sp,sp

00000008 <udiv>:
   8:	82 00 13 c1 	udiv sp,sp,sp

0000000c <and>:
   c:	82 04 10 01 	and sp,sp,sp

00000010 <or>:
  10:	82 04 10 81 	or sp,sp,sp

00000014 <xor>:
  14:	82 04 11 01 	xor sp,sp,sp

00000018 <not>:
  18:	82 04 01 81 	not sp,sp

0000001c <sdiv>:
  1c:	82 00 13 81 	sdiv sp,sp,sp

00000020 <nsdiv>:
  20:	82 04 13 81 	nsdiv sp,sp,sp

00000024 <nudiv>:
  24:	82 04 13 c1 	nudiv sp,sp,sp

00000028 <smul>:
  28:	84 00 22 02 	smul fp,fp,fp

0000002c <umul>:
  2c:	84 00 22 82 	umul fp,fp,fp

00000030 <sll>:
  30:	82 04 12 01 	sll sp,sp,sp

00000034 <srl>:
  34:	82 04 12 81 	srl sp,sp,sp

00000038 <sra>:
  38:	82 04 13 01 	sra sp,sp,sp

0000003c <scan>:
  3c:	82 2c 10 01 	scan sp,sp,sp

00000040 <cadd>:
  40:	83 60 10 01 	cadd sp,sp,sp,cc0,0x0

00000044 <csub>:
  44:	83 60 10 41 	csub sp,sp,sp,cc0,0x0

00000048 <cudiv>:
  48:	83 64 10 c1 	cudiv sp,sp,sp,cc0,0x0

0000004c <cand>:
  4c:	83 68 10 01 	cand sp,sp,sp,cc0,0x0

00000050 <cor>:
  50:	83 68 10 41 	cor sp,sp,sp,cc0,0x0

00000054 <cxor>:
  54:	83 68 10 81 	cxor sp,sp,sp,cc0,0x0

00000058 <cnot>:
  58:	83 68 00 c1 	cnot sp,sp,cc0,0x0

0000005c <csmul>:
  5c:	85 60 20 82 	csmul fp,fp,fp,cc0,0x0

00000060 <csdiv>:
  60:	83 60 10 c1 	csdiv sp,sp,sp,cc0,0x0

00000064 <csll>:
  64:	83 70 10 01 	csll sp,sp,sp,cc0,0x0

00000068 <csrl>:
  68:	83 70 10 41 	csrl sp,sp,sp,cc0,0x0

0000006c <csra>:
  6c:	83 70 10 81 	csra sp,sp,sp,cc0,0x0

00000070 <cscan>:
  70:	83 94 10 c1 	cscan sp,sp,sp,cc0,0x0

00000074 <addcc>:
  74:	82 00 10 41 	addcc sp,sp,sp,icc0

00000078 <subcc>:
  78:	82 00 11 41 	subcc sp,sp,sp,icc0

0000007c <andcc>:
  7c:	82 04 10 41 	andcc sp,sp,sp,icc0

00000080 <orcc>:
  80:	82 04 10 c1 	orcc sp,sp,sp,icc0

00000084 <xorcc>:
  84:	82 04 11 41 	xorcc sp,sp,sp,icc0

00000088 <sllcc>:
  88:	82 04 12 41 	sllcc sp,sp,sp,icc0

0000008c <srlcc>:
  8c:	82 04 12 c1 	srlcc sp,sp,sp,icc0

00000090 <sracc>:
  90:	82 04 13 41 	sracc sp,sp,sp,icc0

00000094 <smulcc>:
  94:	84 00 22 42 	smulcc fp,fp,fp,icc0

00000098 <umulcc>:
  98:	84 00 22 c2 	umulcc fp,fp,fp,icc0

0000009c <caddcc>:
  9c:	83 64 10 01 	caddcc sp,sp,sp,cc0,0x0

000000a0 <csubcc>:
  a0:	83 64 10 41 	csubcc sp,sp,sp,cc0,0x0

000000a4 <csmulcc>:
  a4:	85 64 20 82 	csmulcc fp,fp,fp,cc0,0x0

000000a8 <candcc>:
  a8:	83 6c 10 01 	candcc sp,sp,sp,cc0,0x0

000000ac <corcc>:
  ac:	83 6c 10 41 	corcc sp,sp,sp,cc0,0x0

000000b0 <cxorcc>:
  b0:	83 6c 10 81 	cxorcc sp,sp,sp,cc0,0x0

000000b4 <csllcc>:
  b4:	83 74 10 01 	csllcc sp,sp,sp,cc0,0x0

000000b8 <csrlcc>:
  b8:	83 74 10 41 	csrlcc sp,sp,sp,cc0,0x0

000000bc <csracc>:
  bc:	83 74 10 81 	csracc sp,sp,sp,cc0,0x0

000000c0 <addx>:
  c0:	82 00 10 81 	addx sp,sp,sp,icc0

000000c4 <subx>:
  c4:	82 00 11 81 	subx sp,sp,sp,icc0

000000c8 <addxcc>:
  c8:	82 00 10 c1 	addxcc sp,sp,sp,icc0

000000cc <subxcc>:
  cc:	82 00 11 c1 	subxcc sp,sp,sp,icc0

000000d0 <addi>:
  d0:	82 40 10 00 	addi sp,0,sp

000000d4 <subi>:
  d4:	82 50 10 00 	subi sp,0,sp

000000d8 <udivi>:
  d8:	82 7c 10 00 	udivi sp,0,sp

000000dc <andi>:
  dc:	82 80 10 00 	andi sp,0,sp

000000e0 <ori>:
  e0:	82 88 10 00 	ori sp,0,sp

000000e4 <xori>:
  e4:	82 90 10 00 	xori sp,0,sp

000000e8 <sdivi>:
  e8:	82 78 10 00 	sdivi sp,0,sp

000000ec <nsdivi>:
  ec:	82 b8 10 00 	nsdivi sp,0,sp

000000f0 <nudivi>:
  f0:	82 bc 10 00 	nudivi sp,0,sp

000000f4 <smuli>:
  f4:	84 60 20 00 	smuli fp,0,fp

000000f8 <umuli>:
  f8:	84 68 20 00 	umuli fp,0,fp

000000fc <slli>:
  fc:	82 a0 10 00 	slli sp,0,sp

00000100 <srli>:
 100:	82 a8 10 00 	srli sp,0,sp

00000104 <srai>:
 104:	82 b0 10 00 	srai sp,0,sp

00000108 <scani>:
 108:	83 1c 10 00 	scani sp,0,sp

0000010c <addicc>:
 10c:	82 44 10 00 	addicc sp,0,sp,icc0

00000110 <subicc>:
 110:	82 54 10 00 	subicc sp,0,sp,icc0

00000114 <andicc>:
 114:	82 84 10 00 	andicc sp,0,sp,icc0

00000118 <oricc>:
 118:	82 8c 10 00 	oricc sp,0,sp,icc0

0000011c <xoricc>:
 11c:	82 94 10 00 	xoricc sp,0,sp,icc0

00000120 <smulicc>:
 120:	84 64 20 00 	smulicc fp,0,fp,icc0

00000124 <umulicc>:
 124:	84 6c 20 00 	umulicc fp,0,fp,icc0

00000128 <sllicc>:
 128:	82 a4 10 00 	sllicc sp,0,sp,icc0

0000012c <srlicc>:
 12c:	82 ac 10 00 	srlicc sp,0,sp,icc0

00000130 <sraicc>:
 130:	82 b4 10 00 	sraicc sp,0,sp,icc0

00000134 <addxi>:
 134:	82 48 10 00 	addxi sp,0,sp,icc0

00000138 <subxi>:
 138:	82 58 10 00 	subxi sp,0,sp,icc0

0000013c <addxicc>:
 13c:	82 4c 10 00 	addxicc sp,0,sp,icc0

00000140 <subxicc>:
 140:	82 5c 10 00 	subxicc sp,0,sp,icc0

00000144 <setlo>:
 144:	82 f4 00 00 	setlo lo\(0x0\),sp

00000148 <sethi>:
 148:	82 f8 00 00 	sethi hi\(0x0\),sp

0000014c <setlos>:
 14c:	82 fc 00 00 	setlos lo\(0x0\),sp

00000150 <ldsb>:
 150:	82 08 10 01 	ldsb @\(sp,sp\),sp

00000154 <ldub>:
 154:	82 08 10 41 	ldub @\(sp,sp\),sp

00000158 <ldsh>:
 158:	82 08 10 81 	ldsh @\(sp,sp\),sp

0000015c <lduh>:
 15c:	82 08 10 c1 	lduh @\(sp,sp\),sp

00000160 <ld>:
 160:	82 08 11 01 	ld @\(sp,sp\),sp

00000164 <ldbf>:
 164:	80 08 12 01 	ldbf @\(sp,sp\),fr0

00000168 <ldhf>:
 168:	80 08 12 41 	ldhf @\(sp,sp\),fr0

0000016c <ldf>:
 16c:	80 08 12 81 	ldf @\(sp,sp\),fr0

00000170 <ldc>:
 170:	80 08 13 41 	ldc @\(sp,sp\),cpr0

00000174 <nldsb>:
 174:	82 08 18 01 	nldsb @\(sp,sp\),sp

00000178 <nldub>:
 178:	82 08 18 41 	nldub @\(sp,sp\),sp

0000017c <nldsh>:
 17c:	82 08 18 81 	nldsh @\(sp,sp\),sp

00000180 <nlduh>:
 180:	82 08 18 c1 	nlduh @\(sp,sp\),sp

00000184 <nld>:
 184:	82 08 19 01 	nld @\(sp,sp\),sp

00000188 <nldbf>:
 188:	80 08 1a 01 	nldbf @\(sp,sp\),fr0

0000018c <nldhf>:
 18c:	80 08 1a 41 	nldhf @\(sp,sp\),fr0

00000190 <nldf>:
 190:	80 08 1a 81 	nldf @\(sp,sp\),fr0

00000194 <ldd>:
 194:	84 08 11 41 	ldd @\(sp,sp\),fp

00000198 <lddf>:
 198:	80 08 12 c1 	lddf @\(sp,sp\),fr0

0000019c <lddc>:
 19c:	80 08 13 81 	lddc @\(sp,sp\),cpr0

000001a0 <nldd>:
 1a0:	84 08 19 41 	nldd @\(sp,sp\),fp

000001a4 <nlddf>:
 1a4:	80 08 1a c1 	nlddf @\(sp,sp\),fr0

000001a8 <ldq>:
 1a8:	82 08 11 81 	ldq @\(sp,sp\),sp

000001ac <ldqf>:
 1ac:	80 08 13 01 	ldqf @\(sp,sp\),fr0

000001b0 <ldqc>:
 1b0:	80 08 13 c1 	ldqc @\(sp,sp\),cpr0

000001b4 <nldq>:
 1b4:	82 08 19 81 	nldq @\(sp,sp\),sp

000001b8 <nldqf>:
 1b8:	80 08 1b 01 	nldqf @\(sp,sp\),fr0

000001bc <ldsbu>:
 1bc:	82 08 14 01 	ldsbu @\(sp,sp\),sp

000001c0 <ldubu>:
 1c0:	82 08 14 41 	ldubu @\(sp,sp\),sp

000001c4 <ldshu>:
 1c4:	82 08 14 81 	ldshu @\(sp,sp\),sp

000001c8 <lduhu>:
 1c8:	82 08 14 c1 	lduhu @\(sp,sp\),sp

000001cc <ldu>:
 1cc:	82 08 15 01 	ldu @\(sp,sp\),sp

000001d0 <nldsbu>:
 1d0:	82 08 1c 01 	nldsbu @\(sp,sp\),sp

000001d4 <nldubu>:
 1d4:	82 08 1c 41 	nldubu @\(sp,sp\),sp

000001d8 <nldshu>:
 1d8:	82 08 1c 81 	nldshu @\(sp,sp\),sp

000001dc <nlduhu>:
 1dc:	82 08 1c c1 	nlduhu @\(sp,sp\),sp

000001e0 <nldu>:
 1e0:	82 08 1d 01 	nldu @\(sp,sp\),sp

000001e4 <ldbfu>:
 1e4:	80 08 16 01 	ldbfu @\(sp,sp\),fr0

000001e8 <ldhfu>:
 1e8:	80 08 16 41 	ldhfu @\(sp,sp\),fr0

000001ec <ldfu>:
 1ec:	80 08 16 81 	ldfu @\(sp,sp\),fr0

000001f0 <ldcu>:
 1f0:	80 08 17 41 	ldcu @\(sp,sp\),cpr0

000001f4 <nldbfu>:
 1f4:	80 08 1e 01 	nldbfu @\(sp,sp\),fr0

000001f8 <nldhfu>:
 1f8:	80 08 1e 41 	nldhfu @\(sp,sp\),fr0

000001fc <nldfu>:
 1fc:	80 08 1e 81 	nldfu @\(sp,sp\),fr0

00000200 <lddu>:
 200:	84 08 15 41 	lddu @\(sp,sp\),fp

00000204 <nlddu>:
 204:	84 08 1d 41 	nlddu @\(sp,sp\),fp

00000208 <lddfu>:
 208:	80 08 16 c1 	lddfu @\(sp,sp\),fr0

0000020c <lddcu>:
 20c:	80 08 17 81 	lddcu @\(sp,sp\),cpr0

00000210 <nlddfu>:
 210:	80 08 1e c1 	nlddfu @\(sp,sp\),fr0

00000214 <ldqu>:
 214:	82 08 15 81 	ldqu @\(sp,sp\),sp

00000218 <nldqu>:
 218:	82 08 1d 81 	nldqu @\(sp,sp\),sp

0000021c <ldqfu>:
 21c:	80 08 17 01 	ldqfu @\(sp,sp\),fr0

00000220 <ldqcu>:
 220:	80 08 17 c1 	ldqcu @\(sp,sp\),cpr0

00000224 <nldqfu>:
 224:	80 08 1f 01 	nldqfu @\(sp,sp\),fr0

00000228 <ldsbi>:
 228:	82 c0 10 00 	ldsbi @\(sp,0\),sp

0000022c <ldshi>:
 22c:	82 c4 10 00 	ldshi @\(sp,0\),sp

00000230 <ldi>:
 230:	82 c8 10 00 	ldi @\(sp,0\),sp

00000234 <ldubi>:
 234:	82 d4 10 00 	ldubi @\(sp,0\),sp

00000238 <lduhi>:
 238:	82 d8 10 00 	lduhi @\(sp,0\),sp

0000023c <ldbfi>:
 23c:	80 e0 10 00 	ldbfi @\(sp,0\),fr0

00000240 <ldhfi>:
 240:	80 e4 10 00 	ldhfi @\(sp,0\),fr0

00000244 <ldfi>:
 244:	80 e8 10 00 	ldfi @\(sp,0\),fr0

00000248 <nldsbi>:
 248:	83 00 10 00 	nldsbi @\(sp,0\),sp

0000024c <nldubi>:
 24c:	83 04 10 00 	nldubi @\(sp,0\),sp

00000250 <nldshi>:
 250:	83 08 10 00 	nldshi @\(sp,0\),sp

00000254 <nlduhi>:
 254:	83 0c 10 00 	nlduhi @\(sp,0\),sp

00000258 <nldi>:
 258:	83 10 10 00 	nldi @\(sp,0\),sp

0000025c <nldbfi>:
 25c:	81 20 10 00 	nldbfi @\(sp,0\),fr0

00000260 <nldhfi>:
 260:	81 24 10 00 	nldhfi @\(sp,0\),fr0

00000264 <nldfi>:
 264:	81 28 10 00 	nldfi @\(sp,0\),fr0

00000268 <lddi>:
 268:	84 cc 10 00 	lddi @\(sp,0\),fp

0000026c <lddfi>:
 26c:	80 ec 10 00 	lddfi @\(sp,0\),fr0

00000270 <nlddi>:
 270:	85 14 10 00 	nlddi @\(sp,0\),fp

00000274 <nlddfi>:
 274:	81 2c 10 00 	nlddfi @\(sp,0\),fr0

00000278 <ldqi>:
 278:	82 d0 10 00 	ldqi @\(sp,0\),sp

0000027c <ldqfi>:
 27c:	80 f0 10 00 	ldqfi @\(sp,0\),fr0

00000280 <nop>:
 280:	80 88 00 00 	nop

00000284 <nldqfi>:
 284:	81 30 10 00 	nldqfi @\(sp,0\),fr0

00000288 <stb>:
 288:	82 0c 10 01 	stb sp,@\(sp,sp\)

0000028c <sth>:
 28c:	82 0c 10 41 	sth sp,@\(sp,sp\)

00000290 <st>:
 290:	82 0c 10 81 	st sp,@\(sp,sp\)

00000294 <stbf>:
 294:	80 0c 12 01 	stbf fr0,@\(sp,sp\)

00000298 <sthf>:
 298:	80 0c 12 41 	sthf fr0,@\(sp,sp\)

0000029c <stf>:
 29c:	80 0c 12 81 	stf fr0,@\(sp,sp\)

000002a0 <stc>:
 2a0:	80 0c 19 41 	stc cpr0,@\(sp,sp\)

000002a4 <rstb>:
 2a4:	80 88 00 00 	nop

000002a8 <rsth>:
 2a8:	80 88 00 00 	nop

000002ac <rst>:
 2ac:	80 88 00 00 	nop

000002b0 <rstbf>:
 2b0:	80 88 00 00 	nop

000002b4 <rsthf>:
 2b4:	80 88 00 00 	nop

000002b8 <rstf>:
 2b8:	80 88 00 00 	nop

000002bc <std>:
 2bc:	84 0c 10 c1 	std fp,@\(sp,sp\)

000002c0 <stdf>:
 2c0:	80 0c 12 c1 	stdf fr0,@\(sp,sp\)

000002c4 <stdc>:
 2c4:	80 0c 19 81 	stdc cpr0,@\(sp,sp\)

000002c8 <rstd>:
 2c8:	80 88 00 00 	nop

000002cc <rstdf>:
 2cc:	80 88 00 00 	nop

000002d0 <stq>:
 2d0:	82 0c 11 01 	stq sp,@\(sp,sp\)

000002d4 <stqf>:
 2d4:	80 0c 13 01 	stqf fr0,@\(sp,sp\)

000002d8 <stqc>:
 2d8:	80 0c 19 c1 	stqc cpr0,@\(sp,sp\)

000002dc <rstq>:
 2dc:	80 88 00 00 	nop

000002e0 <rstqf>:
 2e0:	80 88 00 00 	nop

000002e4 <stbu>:
 2e4:	82 0c 14 01 	stbu sp,@\(sp,sp\)

000002e8 <sthu>:
 2e8:	82 0c 14 41 	sthu sp,@\(sp,sp\)

000002ec <stu>:
 2ec:	82 0c 14 81 	stu sp,@\(sp,sp\)

000002f0 <stbfu>:
 2f0:	80 0c 16 01 	stbfu fr0,@\(sp,sp\)

000002f4 <sthfu>:
 2f4:	80 0c 16 41 	sthfu fr0,@\(sp,sp\)

000002f8 <stfu>:
 2f8:	80 0c 16 81 	stfu fr0,@\(sp,sp\)

000002fc <stcu>:
 2fc:	80 0c 1b 41 	stcu cpr0,@\(sp,sp\)

00000300 <stdu>:
 300:	84 0c 14 c1 	stdu fp,@\(sp,sp\)

00000304 <stdfu>:
 304:	80 0c 16 c1 	stdfu fr0,@\(sp,sp\)

00000308 <stdcu>:
 308:	80 0c 1b 81 	stdcu cpr0,@\(sp,sp\)

0000030c <stqu>:
 30c:	82 0c 15 01 	stqu sp,@\(sp,sp\)

00000310 <stqfu>:
 310:	80 0c 17 01 	stqfu fr0,@\(sp,sp\)

00000314 <stqcu>:
 314:	80 0c 1b c1 	stqcu cpr0,@\(sp,sp\)

00000318 <cldsb>:
 318:	83 78 10 01 	cldsb @\(sp,sp\),sp,cc0,0x0

0000031c <cldub>:
 31c:	83 78 10 41 	cldub @\(sp,sp\),sp,cc0,0x0

00000320 <cldsh>:
 320:	83 78 10 81 	cldsh @\(sp,sp\),sp,cc0,0x0

00000324 <clduh>:
 324:	83 78 10 c1 	clduh @\(sp,sp\),sp,cc0,0x0

00000328 <cld>:
 328:	83 7c 10 01 	cld @\(sp,sp\),sp,cc0,0x0

0000032c <cldbf>:
 32c:	81 80 10 01 	cldbf @\(sp,sp\),fr0,cc0,0x0

00000330 <cldhf>:
 330:	81 80 10 41 	cldhf @\(sp,sp\),fr0,cc0,0x0

00000334 <cldf>:
 334:	81 80 10 81 	cldf @\(sp,sp\),fr0,cc0,0x0

00000338 <cldd>:
 338:	85 7c 10 41 	cldd @\(sp,sp\),fp,cc0,0x0

0000033c <clddf>:
 33c:	81 80 10 c1 	clddf @\(sp,sp\),fr0,cc0,0x0

00000340 <cldq>:
 340:	83 7c 10 81 	cldq @\(sp,sp\),sp,cc0,0x0

00000344 <cldsbu>:
 344:	83 84 10 01 	cldsbu @\(sp,sp\),sp,cc0,0x0

00000348 <cldubu>:
 348:	83 84 10 41 	cldubu @\(sp,sp\),sp,cc0,0x0

0000034c <cldshu>:
 34c:	83 84 10 81 	cldshu @\(sp,sp\),sp,cc0,0x0

00000350 <clduhu>:
 350:	83 84 10 c1 	clduhu @\(sp,sp\),sp,cc0,0x0

00000354 <cldu>:
 354:	83 88 10 01 	cldu @\(sp,sp\),sp,cc0,0x0

00000358 <cldbfu>:
 358:	81 8c 10 01 	cldbfu @\(sp,sp\),fr0,cc0,0x0

0000035c <cldhfu>:
 35c:	81 8c 10 41 	cldhfu @\(sp,sp\),fr0,cc0,0x0

00000360 <cldfu>:
 360:	81 8c 10 81 	cldfu @\(sp,sp\),fr0,cc0,0x0

00000364 <clddu>:
 364:	85 88 10 41 	clddu @\(sp,sp\),fp,cc0,0x0

00000368 <clddfu>:
 368:	81 8c 10 c1 	clddfu @\(sp,sp\),fr0,cc0,0x0

0000036c <cldqu>:
 36c:	83 88 10 81 	cldqu @\(sp,sp\),sp,cc0,0x0

00000370 <cstb>:
 370:	83 90 10 01 	cstb sp,@\(sp,sp\),cc0,0x0

00000374 <csth>:
 374:	83 90 10 41 	csth sp,@\(sp,sp\),cc0,0x0

00000378 <cst>:
 378:	83 90 10 81 	cst sp,@\(sp,sp\),cc0,0x0

0000037c <cstbf>:
 37c:	81 98 10 01 	cstbf fr0,@\(sp,sp\),cc0,0x0

00000380 <csthf>:
 380:	81 98 10 41 	csthf fr0,@\(sp,sp\),cc0,0x0

00000384 <cstf>:
 384:	81 98 10 81 	cstf fr0,@\(sp,sp\),cc0,0x0

00000388 <cstd>:
 388:	85 90 10 c1 	cstd fp,@\(sp,sp\),cc0,0x0

0000038c <cstdf>:
 38c:	81 98 10 c1 	cstdf fr0,@\(sp,sp\),cc0,0x0

00000390 <cstq>:
 390:	83 94 10 01 	cstq sp,@\(sp,sp\),cc0,0x0

00000394 <cstbu>:
 394:	83 9c 10 01 	cstbu sp,@\(sp,sp\),cc0,0x0

00000398 <csthu>:
 398:	83 9c 10 41 	csthu sp,@\(sp,sp\),cc0,0x0

0000039c <cstu>:
 39c:	83 9c 10 81 	cstu sp,@\(sp,sp\),cc0,0x0

000003a0 <cstbfu>:
 3a0:	81 a0 10 01 	cstbfu fr0,@\(sp,sp\),cc0,0x0

000003a4 <csthfu>:
 3a4:	81 a0 10 41 	csthfu fr0,@\(sp,sp\),cc0,0x0

000003a8 <cstfu>:
 3a8:	81 a0 10 81 	cstfu fr0,@\(sp,sp\),cc0,0x0

000003ac <cstdu>:
 3ac:	85 9c 10 c1 	cstdu fp,@\(sp,sp\),cc0,0x0

000003b0 <cstdfu>:
 3b0:	81 a0 10 c1 	cstdfu fr0,@\(sp,sp\),cc0,0x0

000003b4 <stbi>:
 3b4:	83 40 10 00 	stbi sp,@\(sp,0\)

000003b8 <sthi>:
 3b8:	83 44 10 00 	sthi sp,@\(sp,0\)

000003bc <sti>:
 3bc:	83 48 10 00 	sti sp,@\(sp,0\)

000003c0 <stbfi>:
 3c0:	81 38 10 00 	stbfi fr0,@\(sp,0\)

000003c4 <sthfi>:
 3c4:	81 3c 10 00 	sthfi fr0,@\(sp,0\)

000003c8 <stfi>:
 3c8:	81 54 10 00 	stfi fr0,@\(sp,0\)

000003cc <stdi>:
 3cc:	85 4c 10 00 	stdi fp,@\(sp,0\)

000003d0 <stdfi>:
 3d0:	81 58 10 00 	stdfi fr0,@\(sp,0\)

000003d4 <stqi>:
 3d4:	83 50 10 00 	stqi sp,@\(sp,0\)

000003d8 <stqfi>:
 3d8:	81 5c 10 00 	stqfi fr0,@\(sp,0\)

000003dc <swap>:
 3dc:	82 0c 11 41 	swap @\(sp,sp\),sp

000003e0 <swapi>:
 3e0:	83 34 10 00 	swapi @\(sp,0\),sp

000003e4 <cswap>:
 3e4:	83 94 10 81 	cswap @\(sp,sp\),sp,cc0,0x0

000003e8 <movgf>:
 3e8:	80 0c 05 41 	movgf sp,fr0

000003ec <movfg>:
 3ec:	80 0c 03 41 	movfg fr0,sp

000003f0 <movgfd>:
 3f0:	80 0c 05 81 	movgfd sp,fr0

000003f4 <movfgd>:
 3f4:	80 0c 03 81 	movfgd fr0,sp

000003f8 <movgfq>:
 3f8:	80 0c 05 c1 	movgfq sp,fr0

000003fc <movfgq>:
 3fc:	80 0c 03 c1 	movfgq fr0,sp

00000400 <cmovgf>:
 400:	81 a4 00 01 	cmovgf sp,fr0,cc0,0x0

00000404 <cmovfg>:
 404:	81 a4 00 81 	cmovfg fr0,sp,cc0,0x0

00000408 <cmovgfd>:
 408:	81 a4 00 41 	cmovgfd sp,fr0,cc0,0x0

0000040c <cmovfgd>:
 40c:	81 a4 00 c1 	cmovfgd fr0,sp,cc0,0x0

00000410 <movgs>:
 410:	80 0c 01 81 	movgs sp,psr

00000414 <movsg>:
 414:	80 0c 01 c1 	movsg psr,sp

00000418 <bno>:
 418:	80 18 00 00 	bno

0000041c <bra>:
 41c:	c0 1a fe f9 	bra 0 <add>

00000420 <beq>:
 420:	a0 18 fe f8 	beq icc0,0x0,0 <add>

00000424 <bne>:
 424:	e0 18 fe f7 	bne icc0,0x0,0 <add>

00000428 <ble>:
 428:	b8 18 fe f6 	ble icc0,0x0,0 <add>

0000042c <bgt>:
 42c:	f8 18 fe f5 	bgt icc0,0x0,0 <add>

00000430 <blt>:
 430:	98 18 fe f4 	blt icc0,0x0,0 <add>

00000434 <bge>:
 434:	d8 18 fe f3 	bge icc0,0x0,0 <add>

00000438 <bls>:
 438:	a8 18 fe f2 	bls icc0,0x0,0 <add>

0000043c <bhi>:
 43c:	e8 18 fe f1 	bhi icc0,0x0,0 <add>

00000440 <bc>:
 440:	88 18 fe f0 	bc icc0,0x0,0 <add>

00000444 <bnc>:
 444:	c8 18 fe ef 	bnc icc0,0x0,0 <add>

00000448 <bn>:
 448:	b0 18 fe ee 	bn icc0,0x0,0 <add>

0000044c <bp>:
 44c:	f0 18 fe ed 	bp icc0,0x0,0 <add>

00000450 <bv>:
 450:	90 18 fe ec 	bv icc0,0x0,0 <add>

00000454 <bnv>:
 454:	d0 18 fe eb 	bnv icc0,0x0,0 <add>

00000458 <fbno>:
 458:	80 1c 00 00 	fbno

0000045c <fbra>:
 45c:	f8 1e fe e9 	fbra 0 <add>

00000460 <fbne>:
 460:	b8 1c fe e8 	fbne fcc0,0x0,0 <add>

00000464 <fbeq>:
 464:	c0 1c fe e7 	fbeq fcc0,0x0,0 <add>

00000468 <fblg>:
 468:	b0 1c fe e6 	fblg fcc0,0x0,0 <add>

0000046c <fbue>:
 46c:	c8 1c fe e5 	fbue fcc0,0x0,0 <add>

00000470 <fbul>:
 470:	a8 1c fe e4 	fbul fcc0,0x0,0 <add>

00000474 <fbge>:
 474:	d0 1c fe e3 	fbge fcc0,0x0,0 <add>

00000478 <fblt>:
 478:	a0 1c fe e2 	fblt fcc0,0x0,0 <add>

0000047c <fbuge>:
 47c:	d8 1c fe e1 	fbuge fcc0,0x0,0 <add>

00000480 <fbug>:
 480:	98 1c fe e0 	fbug fcc0,0x0,0 <add>

00000484 <fble>:
 484:	e0 1c fe df 	fble fcc0,0x0,0 <add>

00000488 <fbgt>:
 488:	90 1c fe de 	fbgt fcc0,0x0,0 <add>

0000048c <fbule>:
 48c:	e8 1c fe dd 	fbule fcc0,0x0,0 <add>

00000490 <fbu>:
 490:	88 1c fe dc 	fbu fcc0,0x0,0 <add>

00000494 <fbo>:
 494:	f0 1c fe db 	fbo fcc0,0x0,0 <add>

00000498 <bctrlr>:
 498:	80 38 20 00 	bctrlr 0x0,0x0

0000049c <bnolr>:
 49c:	80 38 40 00 	bnolr

000004a0 <bralr>:
 4a0:	c0 3a 40 00 	bralr

000004a4 <beqlr>:
 4a4:	a0 38 40 00 	beqlr icc0,0x0

000004a8 <bnelr>:
 4a8:	e0 38 40 00 	bnelr icc0,0x0

000004ac <blelr>:
 4ac:	b8 38 40 00 	blelr icc0,0x0

000004b0 <bgtlr>:
 4b0:	f8 38 40 00 	bgtlr icc0,0x0

000004b4 <bltlr>:
 4b4:	98 38 40 00 	bltlr icc0,0x0

000004b8 <bgelr>:
 4b8:	d8 38 40 00 	bgelr icc0,0x0

000004bc <blslr>:
 4bc:	a8 38 40 00 	blslr icc0,0x0

000004c0 <bhilr>:
 4c0:	e8 38 40 00 	bhilr icc0,0x0

000004c4 <bclr>:
 4c4:	88 38 40 00 	bclr icc0,0x0

000004c8 <bnclr>:
 4c8:	c8 38 40 00 	bnclr icc0,0x0

000004cc <bnlr>:
 4cc:	b0 38 40 00 	bnlr icc0,0x0

000004d0 <bplr>:
 4d0:	f0 38 40 00 	bplr icc0,0x0

000004d4 <bvlr>:
 4d4:	90 38 40 00 	bvlr icc0,0x0

000004d8 <bnvlr>:
 4d8:	d0 38 40 00 	bnvlr icc0,0x0

000004dc <fbnolr>:
 4dc:	80 38 c0 00 	fbnolr

000004e0 <fbralr>:
 4e0:	f8 3a c0 00 	fbralr

000004e4 <fbeqlr>:
 4e4:	c0 38 c0 00 	fbeqlr fcc0,0x0

000004e8 <fbnelr>:
 4e8:	b8 38 c0 00 	fbnelr fcc0,0x0

000004ec <fblglr>:
 4ec:	b0 38 c0 00 	fblglr fcc0,0x0

000004f0 <fbuelr>:
 4f0:	c8 38 c0 00 	fbuelr fcc0,0x0

000004f4 <fbullr>:
 4f4:	a8 38 c0 00 	fbullr fcc0,0x0

000004f8 <fbgelr>:
 4f8:	d0 38 c0 00 	fbgelr fcc0,0x0

000004fc <fbltlr>:
 4fc:	a0 38 c0 00 	fbltlr fcc0,0x0

00000500 <fbugelr>:
 500:	d8 38 c0 00 	fbugelr fcc0,0x0

00000504 <fbuglr>:
 504:	98 38 c0 00 	fbuglr fcc0,0x0

00000508 <fblelr>:
 508:	e0 38 c0 00 	fblelr fcc0,0x0

0000050c <fbgtlr>:
 50c:	90 38 c0 00 	fbgtlr fcc0,0x0

00000510 <fbulelr>:
 510:	e8 38 c0 00 	fbulelr fcc0,0x0

00000514 <fbulr>:
 514:	88 38 c0 00 	fbulr fcc0,0x0

00000518 <fbolr>:
 518:	f0 38 c0 00 	fbolr fcc0,0x0

0000051c <bcnolr>:
 51c:	80 38 60 00 	bcnolr

00000520 <bcralr>:
 520:	c0 3a 60 00 	bcralr 0x0

00000524 <bceqlr>:
 524:	a0 38 60 00 	bceqlr icc0,0x0,0x0

00000528 <bcnelr>:
 528:	e0 38 60 00 	bcnelr icc0,0x0,0x0

0000052c <bclelr>:
 52c:	b8 38 60 00 	bclelr icc0,0x0,0x0

00000530 <bcgtlr>:
 530:	f8 38 60 00 	bcgtlr icc0,0x0,0x0

00000534 <bcltlr>:
 534:	98 38 60 00 	bcltlr icc0,0x0,0x0

00000538 <bcgelr>:
 538:	d8 38 60 00 	bcgelr icc0,0x0,0x0

0000053c <bclslr>:
 53c:	a8 38 60 00 	bclslr icc0,0x0,0x0

00000540 <bchilr>:
 540:	e8 38 60 00 	bchilr icc0,0x0,0x0

00000544 <bcclr>:
 544:	88 38 60 00 	bcclr icc0,0x0,0x0

00000548 <bcnclr>:
 548:	c8 38 60 00 	bcnclr icc0,0x0,0x0

0000054c <bcnlr>:
 54c:	b0 38 60 00 	bcnlr icc0,0x0,0x0

00000550 <bcplr>:
 550:	f0 38 60 00 	bcplr icc0,0x0,0x0

00000554 <bcvlr>:
 554:	90 38 60 00 	bcvlr icc0,0x0,0x0

00000558 <bcnvlr>:
 558:	d0 38 60 00 	bcnvlr icc0,0x0,0x0

0000055c <fcbnolr>:
 55c:	80 38 e0 00 	fcbnolr

00000560 <fcbralr>:
 560:	f8 3a e0 00 	fcbralr 0x0

00000564 <fcbeqlr>:
 564:	c0 38 e0 00 	fcbeqlr fcc0,0x0,0x0

00000568 <fcbnelr>:
 568:	b8 38 e0 00 	fcbnelr fcc0,0x0,0x0

0000056c <fcblglr>:
 56c:	b0 38 e0 00 	fcblglr fcc0,0x0,0x0

00000570 <fcbuelr>:
 570:	c8 38 e0 00 	fcbuelr fcc0,0x0,0x0

00000574 <fcbullr>:
 574:	a8 38 e0 00 	fcbullr fcc0,0x0,0x0

00000578 <fcbgelr>:
 578:	d0 38 e0 00 	fcbgelr fcc0,0x0,0x0

0000057c <fcbltlr>:
 57c:	a0 38 e0 00 	fcbltlr fcc0,0x0,0x0

00000580 <fcbugelr>:
 580:	d8 38 e0 00 	fcbugelr fcc0,0x0,0x0

00000584 <fcbuglr>:
 584:	98 38 e0 00 	fcbuglr fcc0,0x0,0x0

00000588 <fcblelr>:
 588:	e0 38 e0 00 	fcblelr fcc0,0x0,0x0

0000058c <fcbgtlr>:
 58c:	90 38 e0 00 	fcbgtlr fcc0,0x0,0x0

00000590 <fcbulelr>:
 590:	e8 38 e0 00 	fcbulelr fcc0,0x0,0x0

00000594 <fcbulr>:
 594:	88 38 e0 00 	fcbulr fcc0,0x0,0x0

00000598 <fcbolr>:
 598:	f0 38 e0 00 	fcbolr fcc0,0x0,0x0

0000059c <jmpl>:
 59c:	80 30 10 01 	jmpl @\(sp,sp\)

000005a0 <jmpil>:
 5a0:	80 34 10 00 	jmpil @\(sp,0\)

000005a4 <call>:
 5a4:	fe 3f fe 97 	call 0 <add>

000005a8 <rei>:
 5a8:	80 dc 00 00 	rei 0x0

000005ac <tno>:
 5ac:	80 10 00 00 	tno

000005b0 <tra>:
 5b0:	c0 10 10 01 	tra sp,sp

000005b4 <teq>:
 5b4:	a0 10 10 01 	teq icc0,sp,sp

000005b8 <tne>:
 5b8:	e0 10 10 01 	tne icc0,sp,sp

000005bc <tle>:
 5bc:	b8 10 10 01 	tle icc0,sp,sp

000005c0 <tgt>:
 5c0:	f8 10 10 01 	tgt icc0,sp,sp

000005c4 <tlt>:
 5c4:	98 10 10 01 	tlt icc0,sp,sp

000005c8 <tge>:
 5c8:	d8 10 10 01 	tge icc0,sp,sp

000005cc <tls>:
 5cc:	a8 10 10 01 	tls icc0,sp,sp

000005d0 <thi>:
 5d0:	e8 10 10 01 	thi icc0,sp,sp

000005d4 <tc>:
 5d4:	88 10 10 01 	tc icc0,sp,sp

000005d8 <tnc>:
 5d8:	c8 10 10 01 	tnc icc0,sp,sp

000005dc <tn>:
 5dc:	b0 10 10 01 	tn icc0,sp,sp

000005e0 <tp>:
 5e0:	f0 10 10 01 	tp icc0,sp,sp

000005e4 <tv>:
 5e4:	90 10 10 01 	tv icc0,sp,sp

000005e8 <tnv>:
 5e8:	d0 10 10 01 	tnv icc0,sp,sp

000005ec <ftno>:
 5ec:	80 10 00 40 	ftno

000005f0 <ftra>:
 5f0:	f8 10 10 41 	ftra sp,sp

000005f4 <ftne>:
 5f4:	b8 10 10 41 	ftne fcc0,sp,sp

000005f8 <fteq>:
 5f8:	c0 10 10 41 	fteq fcc0,sp,sp

000005fc <ftlg>:
 5fc:	b0 10 10 41 	ftlg fcc0,sp,sp

00000600 <ftue>:
 600:	c8 10 10 41 	ftue fcc0,sp,sp

00000604 <ftul>:
 604:	a8 10 10 41 	ftul fcc0,sp,sp

00000608 <ftge>:
 608:	d0 10 10 41 	ftge fcc0,sp,sp

0000060c <ftlt>:
 60c:	a0 10 10 41 	ftlt fcc0,sp,sp

00000610 <ftuge>:
 610:	d8 10 10 41 	ftuge fcc0,sp,sp

00000614 <ftug>:
 614:	98 10 10 41 	ftug fcc0,sp,sp

00000618 <ftle>:
 618:	e0 10 10 41 	ftle fcc0,sp,sp

0000061c <ftgt>:
 61c:	90 10 10 41 	ftgt fcc0,sp,sp

00000620 <ftule>:
 620:	e8 10 10 41 	ftule fcc0,sp,sp

00000624 <ftu>:
 624:	88 10 10 41 	ftu fcc0,sp,sp

00000628 <fto>:
 628:	f0 10 10 41 	fto fcc0,sp,sp

0000062c <tino>:
 62c:	80 70 00 00 	tino

00000630 <tira>:
 630:	c0 70 10 00 	tira sp,0

00000634 <tieq>:
 634:	a0 70 10 00 	tieq icc0,sp,0

00000638 <tine>:
 638:	e0 70 10 00 	tine icc0,sp,0

0000063c <tile>:
 63c:	b8 70 10 00 	tile icc0,sp,0

00000640 <tigt>:
 640:	f8 70 10 00 	tigt icc0,sp,0

00000644 <tilt>:
 644:	98 70 10 00 	tilt icc0,sp,0

00000648 <tige>:
 648:	d8 70 10 00 	tige icc0,sp,0

0000064c <tils>:
 64c:	a8 70 10 00 	tils icc0,sp,0

00000650 <tihi>:
 650:	e8 70 10 00 	tihi icc0,sp,0

00000654 <tic>:
 654:	88 70 10 00 	tic icc0,sp,0

00000658 <tinc>:
 658:	c8 70 10 00 	tinc icc0,sp,0

0000065c <tin>:
 65c:	b0 70 10 00 	tin icc0,sp,0

00000660 <tip>:
 660:	f0 70 10 00 	tip icc0,sp,0

00000664 <tiv>:
 664:	90 70 10 00 	tiv icc0,sp,0

00000668 <tinv>:
 668:	d0 70 10 00 	tinv icc0,sp,0

0000066c <ftino>:
 66c:	80 74 00 00 	ftino

00000670 <ftira>:
 670:	f8 74 10 00 	ftira sp,0

00000674 <ftine>:
 674:	b8 74 10 00 	ftine fcc0,sp,0

00000678 <ftieq>:
 678:	c0 74 10 00 	ftieq fcc0,sp,0

0000067c <ftilg>:
 67c:	b0 74 10 00 	ftilg fcc0,sp,0

00000680 <ftiue>:
 680:	c8 74 10 00 	ftiue fcc0,sp,0

00000684 <ftiul>:
 684:	a8 74 10 00 	ftiul fcc0,sp,0

00000688 <ftige>:
 688:	d0 74 10 00 	ftige fcc0,sp,0

0000068c <ftilt>:
 68c:	a0 74 10 00 	ftilt fcc0,sp,0

00000690 <ftiuge>:
 690:	d8 74 10 00 	ftiuge fcc0,sp,0

00000694 <ftiug>:
 694:	98 74 10 00 	ftiug fcc0,sp,0

00000698 <ftile>:
 698:	e0 74 10 00 	ftile fcc0,sp,0

0000069c <ftigt>:
 69c:	90 74 10 00 	ftigt fcc0,sp,0

000006a0 <ftiule>:
 6a0:	e8 74 10 00 	ftiule fcc0,sp,0

000006a4 <ftiu>:
 6a4:	88 74 10 00 	ftiu fcc0,sp,0

000006a8 <ftio>:
 6a8:	f0 74 10 00 	ftio fcc0,sp,0

000006ac <andcr>:
 6ac:	80 28 02 00 	andcr cc0,cc0,cc0

000006b0 <orcr>:
 6b0:	80 28 02 40 	orcr cc0,cc0,cc0

000006b4 <xorcr>:
 6b4:	80 28 02 80 	xorcr cc0,cc0,cc0

000006b8 <nandcr>:
 6b8:	80 28 03 00 	nandcr cc0,cc0,cc0

000006bc <norcr>:
 6bc:	80 28 03 40 	norcr cc0,cc0,cc0

000006c0 <andncr>:
 6c0:	80 28 04 00 	andncr cc0,cc0,cc0

000006c4 <orncr>:
 6c4:	80 28 04 40 	orncr cc0,cc0,cc0

000006c8 <nandncr>:
 6c8:	80 28 05 00 	nandncr cc0,cc0,cc0

000006cc <norncr>:
 6cc:	80 28 05 40 	norncr cc0,cc0,cc0

000006d0 <notcr>:
 6d0:	80 28 02 c0 	notcr cc0,cc0

000006d4 <ckno>:
 6d4:	86 20 00 00 	ckno cc7

000006d8 <ckra>:
 6d8:	c6 20 00 00 	ckra cc7

000006dc <ckeq>:
 6dc:	a6 20 00 00 	ckeq icc0,cc7

000006e0 <ckne>:
 6e0:	e6 20 00 00 	ckne icc0,cc7

000006e4 <ckle>:
 6e4:	be 20 00 00 	ckle icc0,cc7

000006e8 <ckgt>:
 6e8:	fe 20 00 00 	ckgt icc0,cc7

000006ec <cklt>:
 6ec:	9e 20 00 00 	cklt icc0,cc7

000006f0 <ckge>:
 6f0:	de 20 00 00 	ckge icc0,cc7

000006f4 <ckls>:
 6f4:	ae 20 00 00 	ckls icc0,cc7

000006f8 <ckhi>:
 6f8:	ee 20 00 00 	ckhi icc0,cc7

000006fc <ckc>:
 6fc:	8e 20 00 00 	ckc icc0,cc7

00000700 <cknc>:
 700:	ce 20 00 00 	cknc icc0,cc7

00000704 <ckn>:
 704:	b6 20 00 00 	ckn icc0,cc7

00000708 <ckp>:
 708:	f6 20 00 00 	ckp icc0,cc7

0000070c <ckv>:
 70c:	96 20 00 00 	ckv icc0,cc7

00000710 <cknv>:
 710:	d6 20 00 00 	cknv icc0,cc7

00000714 <fckno>:
 714:	80 24 00 00 	fckno cc0

00000718 <fckra>:
 718:	f8 24 00 00 	fckra cc0

0000071c <fckne>:
 71c:	b8 24 00 00 	fckne fcc0,cc0

00000720 <fckeq>:
 720:	c0 24 00 00 	fckeq fcc0,cc0

00000724 <fcklg>:
 724:	b0 24 00 00 	fcklg fcc0,cc0

00000728 <fckue>:
 728:	c8 24 00 00 	fckue fcc0,cc0

0000072c <fckul>:
 72c:	a8 24 00 00 	fckul fcc0,cc0

00000730 <fckge>:
 730:	d0 24 00 00 	fckge fcc0,cc0

00000734 <fcklt>:
 734:	a0 24 00 00 	fcklt fcc0,cc0

00000738 <fckuge>:
 738:	d8 24 00 00 	fckuge fcc0,cc0

0000073c <fckug>:
 73c:	98 24 00 00 	fckug fcc0,cc0

00000740 <fckle>:
 740:	e0 24 00 00 	fckle fcc0,cc0

00000744 <fckgt>:
 744:	90 24 00 00 	fckgt fcc0,cc0

00000748 <fckule>:
 748:	e8 24 00 00 	fckule fcc0,cc0

0000074c <fcku>:
 74c:	88 24 00 00 	fcku fcc0,cc0

00000750 <fcko>:
 750:	f0 24 00 00 	fcko fcc0,cc0

00000754 <cckno>:
 754:	87 a8 06 00 	cckno cc7,cc3,0x0

00000758 <cckra>:
 758:	c7 a8 06 00 	cckra cc7,cc3,0x0

0000075c <cckeq>:
 75c:	a7 a8 06 00 	cckeq icc0,cc7,cc3,0x0

00000760 <cckne>:
 760:	e7 a8 06 00 	cckne icc0,cc7,cc3,0x0

00000764 <cckle>:
 764:	bf a8 06 00 	cckle icc0,cc7,cc3,0x0

00000768 <cckgt>:
 768:	ff a8 06 00 	cckgt icc0,cc7,cc3,0x0

0000076c <ccklt>:
 76c:	9f a8 06 00 	ccklt icc0,cc7,cc3,0x0

00000770 <cckge>:
 770:	df a8 06 00 	cckge icc0,cc7,cc3,0x0

00000774 <cckls>:
 774:	af a8 06 00 	cckls icc0,cc7,cc3,0x0

00000778 <cckhi>:
 778:	ef a8 06 00 	cckhi icc0,cc7,cc3,0x0

0000077c <cckc>:
 77c:	8f a8 06 00 	cckc icc0,cc7,cc3,0x0

00000780 <ccknc>:
 780:	cf a8 06 00 	ccknc icc0,cc7,cc3,0x0

00000784 <cckn>:
 784:	b7 a8 06 00 	cckn icc0,cc7,cc3,0x0

00000788 <cckp>:
 788:	f7 a8 06 00 	cckp icc0,cc7,cc3,0x0

0000078c <cckv>:
 78c:	97 a8 06 00 	cckv icc0,cc7,cc3,0x0

00000790 <ccknv>:
 790:	d7 a8 06 00 	ccknv icc0,cc7,cc3,0x0

00000794 <cfckno>:
 794:	81 a8 00 40 	cfckno cc0,cc0,0x0

00000798 <cfckra>:
 798:	f9 a8 00 40 	cfckra cc0,cc0,0x0

0000079c <cfckne>:
 79c:	b9 a8 00 40 	cfckne fcc0,cc0,cc0,0x0

000007a0 <cfckeq>:
 7a0:	c1 a8 00 40 	cfckeq fcc0,cc0,cc0,0x0

000007a4 <cfcklg>:
 7a4:	b1 a8 00 40 	cfcklg fcc0,cc0,cc0,0x0

000007a8 <cfckue>:
 7a8:	c9 a8 00 40 	cfckue fcc0,cc0,cc0,0x0

000007ac <cfckul>:
 7ac:	a9 a8 00 40 	cfckul fcc0,cc0,cc0,0x0

000007b0 <cfckge>:
 7b0:	d1 a8 00 40 	cfckge fcc0,cc0,cc0,0x0

000007b4 <cfcklt>:
 7b4:	a1 a8 00 40 	cfcklt fcc0,cc0,cc0,0x0

000007b8 <cfckuge>:
 7b8:	d9 a8 00 40 	cfckuge fcc0,cc0,cc0,0x0

000007bc <cfckug>:
 7bc:	99 a8 00 40 	cfckug fcc0,cc0,cc0,0x0

000007c0 <cfckle>:
 7c0:	e1 a8 00 40 	cfckle fcc0,cc0,cc0,0x0

000007c4 <cfckgt>:
 7c4:	91 a8 00 40 	cfckgt fcc0,cc0,cc0,0x0

000007c8 <cfckule>:
 7c8:	e9 a8 00 40 	cfckule fcc0,cc0,cc0,0x0

000007cc <cfcku>:
 7cc:	89 a8 00 40 	cfcku fcc0,cc0,cc0,0x0

000007d0 <cfcko>:
 7d0:	f1 a8 00 40 	cfcko fcc0,cc0,cc0,0x0

000007d4 <cjmpl>:
 7d4:	81 a8 10 81 	cjmpl @\(sp,sp\),cc0,0x0

000007d8 <ici>:
 7d8:	80 0c 1e 01 	ici @\(sp,sp\)

000007dc <dci>:
 7dc:	80 0c 1f 01 	dci @\(sp,sp\)

000007e0 <dcf>:
 7e0:	80 0c 1f 41 	dcf @\(sp,sp\)

000007e4 <witlb>:
 7e4:	82 0c 1c 81 	witlb sp,@\(sp,sp\)

000007e8 <wdtlb>:
 7e8:	82 0c 1d 81 	wdtlb sp,@\(sp,sp\)

000007ec <itlbi>:
 7ec:	80 0c 1c c1 	itlbi @\(sp,sp\)

000007f0 <dtlbi>:
 7f0:	80 0c 1d c1 	dtlbi @\(sp,sp\)

000007f4 <icpl>:
 7f4:	80 0c 1c 01 	icpl sp,sp,0x0

000007f8 <dcpl>:
 7f8:	80 0c 1d 01 	dcpl sp,sp,0x0

000007fc <icul>:
 7fc:	80 0c 1c 40 	icul sp

00000800 <dcul>:
 800:	80 0c 1d 40 	dcul sp

00000804 <bar>:
 804:	82 28 00 00 	clrgr sp

00000808 <clrfr>:
 808:	80 28 00 80 	clrfr fr0

0000080c <clrfa>:
 80c:	82 28 01 00 	commitgr sp

00000810 <commitfr>:
 810:	80 28 01 80 	commitfr fr0

00000814 <commitfra>:
 814:	81 e4 00 00 	fitos fr0,fr0

00000818 <fstoi>:
 818:	81 e4 00 40 	fstoi fr0,fr0

0000081c <fitod>:
 81c:	81 e8 00 00 	fitod fr0,fr0

00000820 <fdtoi>:
 820:	81 e8 00 40 	fdtoi fr0,fr0

00000824 <fmovs>:
 824:	81 e4 00 80 	fmovs fr0,fr0

00000828 <fmovd>:
 828:	81 e8 00 80 	fmovd fr0,fr0

0000082c <fnegs>:
 82c:	81 e4 00 c0 	fnegs fr0,fr0

00000830 <fnegd>:
 830:	81 e8 00 c0 	fnegd fr0,fr0

00000834 <fabss>:
 834:	81 e4 01 00 	fabss fr0,fr0

00000838 <fabsd>:
 838:	81 e8 01 00 	fabsd fr0,fr0

0000083c <fsqrts>:
 83c:	81 e4 01 40 	fsqrts fr0,fr0

00000840 <fsqrtd>:
 840:	81 e8 01 40 	fsqrtd fr0,fr0

00000844 <fadds>:
 844:	81 e4 01 80 	fadds fr0,fr0,fr0

00000848 <fsubs>:
 848:	81 e4 01 c0 	fsubs fr0,fr0,fr0

0000084c <fmuls>:
 84c:	81 e4 02 00 	fmuls fr0,fr0,fr0

00000850 <fdivs>:
 850:	81 e4 02 40 	fdivs fr0,fr0,fr0

00000854 <faddd>:
 854:	81 e8 01 80 	faddd fr0,fr0,fr0

00000858 <fsubd>:
 858:	81 e8 01 c0 	fsubd fr0,fr0,fr0

0000085c <fmuld>:
 85c:	81 e8 02 00 	fmuld fr0,fr0,fr0

00000860 <fdivd>:
 860:	81 e8 02 40 	fdivd fr0,fr0,fr0

00000864 <fcmps>:
 864:	81 e4 02 80 	fcmps fr0,fr0,fcc0

00000868 <fcmpd>:
 868:	81 e8 02 80 	fcmpd fr0,fr0,fcc0

0000086c <fmadds>:
 86c:	81 e4 02 c0 	fmadds fr0,fr0,fr0

00000870 <fmsubs>:
 870:	81 e4 03 00 	fmsubs fr0,fr0,fr0

00000874 <fmaddd>:
 874:	81 e8 02 c0 	fmaddd fr0,fr0,fr0

00000878 <fmsubd>:
 878:	81 e8 03 00 	fmsubd fr0,fr0,fr0

0000087c <mand>:
 87c:	81 ec 00 00 	mand fr0,fr0,fr0

00000880 <mor>:
 880:	81 ec 00 40 	mor fr0,fr0,fr0

00000884 <mxor>:
 884:	81 ec 00 80 	mxor fr0,fr0,fr0

00000888 <mnot>:
 888:	81 ec 00 c0 	mnot fr0,fr0

0000088c <mrotli>:
 88c:	81 ec 01 00 	mrotli fr0,0x0,fr0

00000890 <mrotri>:
 890:	81 ec 01 40 	mrotri fr0,0x0,fr0

00000894 <mwcut>:
 894:	81 ec 01 80 	mwcut fr0,fr0,fr0

00000898 <mwcuti>:
 898:	81 ec 01 c0 	mwcuti fr0,0x0,fr0
