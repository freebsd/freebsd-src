#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

0+0000 <add>:
   0:	0d ad f0 00 	add fp,fp \|\| nop

0+0004 <add3>:
   4:	8d ad 00 00 	add3 fp,fp,[#]*0

0+0008 <and>:
   8:	0d cd f0 00 	and fp,fp \|\| nop

0+000c <and3>:
   c:	8d cd 00 00 	and3 fp,fp,[#]*0x0

0+0010 <or>:
  10:	0d ed f0 00 	or fp,fp \|\| nop

0+0014 <or3>:
  14:	8d ed 00 00 	or3 fp,fp,[#]*0x0

0+0018 <xor>:
  18:	0d dd f0 00 	xor fp,fp \|\| nop

0+001c <xor3>:
  1c:	8d dd 00 00 	xor3 fp,fp,[#]*0x0

0+0020 <addi>:
  20:	4d 00 f0 00 	addi fp,[#]*0 \|\| nop

0+0024 <addv>:
  24:	0d 8d f0 00 	addv fp,fp \|\| nop

0+0028 <addv3>:
  28:	8d 8d 00 00 	addv3 fp,fp,[#]*0

0+002c <addx>:
  2c:	0d 9d f0 00 	addx fp,fp \|\| nop

0+0030 <bc8>:
  30:	7c f4 f0 00 	bc 0 <add> \|\| nop

0+0034 <bc8_s>:
  34:	7c f3 f0 00 	bc 0 <add> \|\| nop

0+0038 <bc24>:
  38:	7c f2 f0 00 	bc 0 <add> \|\| nop

0+003c <bc24_l>:
  3c:	fc ff ff f1 	bc 0 <add>

0+0040 <beq>:
  40:	bd 0d ff f0 	beq fp,fp,0 <add>

0+0044 <beqz>:
  44:	b0 8d ff ef 	beqz fp,0 <add>

0+0048 <bgez>:
  48:	b0 bd ff ee 	bgez fp,0 <add>

0+004c <bgtz>:
  4c:	b0 dd ff ed 	bgtz fp,0 <add>

0+0050 <blez>:
  50:	b0 cd ff ec 	blez fp,0 <add>

0+0054 <bltz>:
  54:	b0 ad ff eb 	bltz fp,0 <add>

0+0058 <bnez>:
  58:	b0 9d ff ea 	bnez fp,0 <add>

0+005c <bl8>:
  5c:	7e e9 f0 00 	bl 0 <add> \|\| nop

0+0060 <bl8_s>:
  60:	7e e8 f0 00 	bl 0 <add> \|\| nop

0+0064 <bl24>:
  64:	7e e7 f0 00 	bl 0 <add> \|\| nop

0+0068 <bl24_l>:
  68:	fe ff ff e6 	bl 0 <add>

0+006c <bnc8>:
  6c:	7d e5 f0 00 	bnc 0 <add> \|\| nop

0+0070 <bnc8_s>:
  70:	7d e4 f0 00 	bnc 0 <add> \|\| nop

0+0074 <bnc24>:
  74:	7d e3 f0 00 	bnc 0 <add> \|\| nop

0+0078 <bnc24_l>:
  78:	fd ff ff e2 	bnc 0 <add>

0+007c <bne>:
  7c:	bd 1d ff e1 	bne fp,fp,0 <add>

0+0080 <bra8>:
  80:	7f e0 f0 00 	bra 0 <add> \|\| nop

0+0084 <bra8_s>:
  84:	7f df f0 00 	bra 0 <add> \|\| nop

0+0088 <bra24>:
  88:	7f de f0 00 	bra 0 <add> \|\| nop

0+008c <bra24_l>:
  8c:	ff ff ff dd 	bra 0 <add>

0+0090 <cmp>:
  90:	0d 4d f0 00 	cmp fp,fp \|\| nop

0+0094 <cmpi>:
  94:	80 4d 00 00 	cmpi fp,[#]*0

0+0098 <cmpu>:
  98:	0d 5d f0 00 	cmpu fp,fp \|\| nop

0+009c <cmpui>:
  9c:	80 5d 00 00 	cmpui fp,[#]*0

0+00a0 <div>:
  a0:	9d 0d 00 00 	div fp,fp

0+00a4 <divu>:
  a4:	9d 1d 00 00 	divu fp,fp

0+00a8 <rem>:
  a8:	9d 2d 00 00 	rem fp,fp

0+00ac <remu>:
  ac:	9d 3d 00 00 	remu fp,fp

0+00b0 <jl>:
  b0:	1e cd f0 00 	jl fp \|\| nop

0+00b4 <jmp>:
  b4:	1f cd f0 00 	jmp fp \|\| nop

0+00b8 <ld>:
  b8:	2d cd f0 00 	ld fp,@fp \|\| nop

0+00bc <ld_2>:
  bc:	2d cd f0 00 	ld fp,@fp \|\| nop

0+00c0 <ld_d>:
  c0:	ad cd 00 00 	ld fp,@\(0,fp\)

0+00c4 <ld_d2>:
  c4:	ad cd 00 00 	ld fp,@\(0,fp\)

0+00c8 <ldb>:
  c8:	2d 8d f0 00 	ldb fp,@fp \|\| nop

0+00cc <ldb_2>:
  cc:	2d 8d f0 00 	ldb fp,@fp \|\| nop

0+00d0 <ldb_d>:
  d0:	ad 8d 00 00 	ldb fp,@\(0,fp\)

0+00d4 <ldb_d2>:
  d4:	ad 8d 00 00 	ldb fp,@\(0,fp\)

0+00d8 <ldh>:
  d8:	2d ad f0 00 	ldh fp,@fp \|\| nop

0+00dc <ldh_2>:
  dc:	2d ad f0 00 	ldh fp,@fp \|\| nop

0+00e0 <ldh_d>:
  e0:	ad ad 00 00 	ldh fp,@\(0,fp\)

0+00e4 <ldh_d2>:
  e4:	ad ad 00 00 	ldh fp,@\(0,fp\)

0+00e8 <ldub>:
  e8:	2d 9d f0 00 	ldub fp,@fp \|\| nop

0+00ec <ldub_2>:
  ec:	2d 9d f0 00 	ldub fp,@fp \|\| nop

0+00f0 <ldub_d>:
  f0:	ad 9d 00 00 	ldub fp,@\(0,fp\)

0+00f4 <ldub_d2>:
  f4:	ad 9d 00 00 	ldub fp,@\(0,fp\)

0+00f8 <lduh>:
  f8:	2d bd f0 00 	lduh fp,@fp \|\| nop

0+00fc <lduh_2>:
  fc:	2d bd f0 00 	lduh fp,@fp \|\| nop

0+0100 <lduh_d>:
 100:	ad bd 00 00 	lduh fp,@\(0,fp\)

0+0104 <lduh_d2>:
 104:	ad bd 00 00 	lduh fp,@\(0,fp\)

0+0108 <ld_plus>:
 108:	2d ed f0 00 	ld fp,@fp\+ \|\| nop

0+010c <ld24>:
 10c:	ed 00 00 00 	ld24 fp,[#]*0 <add>
			10c: R_M32R_24_RELA	.data

0+0110 <ldi8>:
 110:	6d 00 f0 00 	ldi fp,[#]*0 \|\| nop

0+0114 <ldi16>:
 114:	9d f0 01 00 	ldi fp,[#]*256

0+0118 <lock>:
 118:	2d dd f0 00 	lock fp,@fp \|\| nop

0+011c <machi>:
 11c:	3d 4d f0 00 	machi fp,fp \|\| nop

0+0120 <maclo>:
 120:	3d 5d f0 00 	maclo fp,fp \|\| nop

0+0124 <macwhi>:
 124:	3d 6d f0 00 	macwhi fp,fp \|\| nop

0+0128 <macwlo>:
 128:	3d 7d f0 00 	macwlo fp,fp \|\| nop

0+012c <mul>:
 12c:	1d 6d f0 00 	mul fp,fp \|\| nop

0+0130 <mulhi>:
 130:	3d 0d f0 00 	mulhi fp,fp \|\| nop

0+0134 <mullo>:
 134:	3d 1d f0 00 	mullo fp,fp \|\| nop

0+0138 <mulwhi>:
 138:	3d 2d f0 00 	mulwhi fp,fp \|\| nop

0+013c <mulwlo>:
 13c:	3d 3d f0 00 	mulwlo fp,fp \|\| nop

0+0140 <mv>:
 140:	1d 8d f0 00 	mv fp,fp \|\| nop

0+0144 <mvfachi>:
 144:	5d f0 f0 00 	mvfachi fp \|\| nop

0+0148 <mvfaclo>:
 148:	5d f1 f0 00 	mvfaclo fp \|\| nop

0+014c <mvfacmi>:
 14c:	5d f2 f0 00 	mvfacmi fp \|\| nop

0+0150 <mvfc>:
 150:	1d 90 f0 00 	mvfc fp,psw \|\| nop

0+0154 <mvtachi>:
 154:	5d 70 f0 00 	mvtachi fp \|\| nop

0+0158 <mvtaclo>:
 158:	5d 71 f0 00 	mvtaclo fp \|\| nop

0+015c <mvtc>:
 15c:	10 ad f0 00 	mvtc fp,psw \|\| nop

0+0160 <neg>:
 160:	0d 3d f0 00 	neg fp,fp \|\| nop

0+0164 <nop>:
 164:	70 00 f0 00 	nop \|\| nop

0+0168 <not>:
 168:	0d bd f0 00 	not fp,fp \|\| nop

0+016c <rac>:
 16c:	dd c0 00 00 	seth fp,[#]*0x0

0+0170 <sll>:
 170:	1d 4d f0 00 	sll fp,fp \|\| nop

0+0174 <sll3>:
 174:	9d cd 00 00 	sll3 fp,fp,[#]*0

0+0178 <slli>:
 178:	5d 40 f0 00 	slli fp,[#]*0x0 \|\| nop

0+017c <sra>:
 17c:	1d 2d f0 00 	sra fp,fp \|\| nop

0+0180 <sra3>:
 180:	9d ad 00 00 	sra3 fp,fp,[#]*0

0+0184 <srai>:
 184:	5d 20 f0 00 	srai fp,[#]*0x0 \|\| nop

0+0188 <srl>:
 188:	1d 0d f0 00 	srl fp,fp \|\| nop

0+018c <srl3>:
 18c:	9d 8d 00 00 	srl3 fp,fp,[#]*0

0+0190 <srli>:
 190:	5d 00 f0 00 	srli fp,[#]*0x0 \|\| nop

0+0194 <st>:
 194:	2d 4d f0 00 	st fp,@fp \|\| nop

0+0198 <st_2>:
 198:	2d 4d f0 00 	st fp,@fp \|\| nop

0+019c <st_d>:
 19c:	ad 4d 00 00 	st fp,@\(0,fp\)

0+01a0 <st_d2>:
 1a0:	ad 4d 00 00 	st fp,@\(0,fp\)

0+01a4 <stb>:
 1a4:	2d 0d f0 00 	stb fp,@fp \|\| nop

0+01a8 <stb_2>:
 1a8:	2d 0d f0 00 	stb fp,@fp \|\| nop

0+01ac <stb_d>:
 1ac:	ad 0d 00 00 	stb fp,@\(0,fp\)

0+01b0 <stb_d2>:
 1b0:	ad 0d 00 00 	stb fp,@\(0,fp\)

0+01b4 <sth>:
 1b4:	2d 2d f0 00 	sth fp,@fp \|\| nop

0+01b8 <sth_2>:
 1b8:	2d 2d f0 00 	sth fp,@fp \|\| nop

0+01bc <sth_d>:
 1bc:	ad 2d 00 00 	sth fp,@\(0,fp\)

0+01c0 <sth_d2>:
 1c0:	ad 2d 00 00 	sth fp,@\(0,fp\)

0+01c4 <st_plus>:
 1c4:	2d 6d f0 00 	st fp,@\+fp \|\| nop

0+01c8 <st_minus>:
 1c8:	2d 7d f0 00 	st fp,@-fp \|\| nop

0+01cc <sub>:
 1cc:	0d 2d f0 00 	sub fp,fp \|\| nop

0+01d0 <subv>:
 1d0:	0d 0d f0 00 	subv fp,fp \|\| nop

0+01d4 <subx>:
 1d4:	0d 1d f0 00 	subx fp,fp \|\| nop

0+01d8 <trap>:
 1d8:	10 f0 f0 00 	trap [#]*0x0 \|\| nop

0+01dc <unlock>:
 1dc:	2d 5d f0 00 	unlock fp,@fp \|\| nop

0+01e0 <push>:
 1e0:	2d 7f f0 00 	push fp \|\| nop

0+01e4 <pop>:
 1e4:	2d ef f0 00 	pop fp \|\| nop
