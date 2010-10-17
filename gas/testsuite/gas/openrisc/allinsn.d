#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <l_j>:
   0:	00 00 00 00 	l.j 0 <l_j>
			0: R_OPENRISC_INSN_ABS_26	.text

00000004 <l_jal>:
   4:	04 00 00 00 	l.jal 0 <l_j>
			4: R_OPENRISC_INSN_ABS_26	.text

00000008 <l_jr>:
   8:	14 00 00 00 	l.jr r0

0000000c <l_jalr>:
   c:	14 20 00 00 	l.jalr r0

00000010 <l_bal>:
  10:	0b ff ff fc 	l.bal 0 <l_j>

00000014 <l_bnf>:
  14:	0f ff ff fb 	l.bnf 0 <l_j>

00000018 <l_bf>:
  18:	13 ff ff fa 	l.bf 0 <l_j>

0000001c <l_brk>:
  1c:	17 00 00 00 	l.brk 0x0

00000020 <l_rfe>:
  20:	14 40 00 00 	l.rfe r0

00000024 <l_sys>:
  24:	16 00 00 00 	l.sys 0x0

00000028 <l_nop>:
  28:	15 00 00 00 	l.nop

0000002c <l_movhi>:
  2c:	18 00 00 00 	l.movhi r0,0

00000030 <l_mfsr>:
  30:	1c 00 00 00 	l.mfsr r0,r0

00000034 <l_mtsr>:
  34:	40 00 00 00 	l.mtsr r0,r0

00000038 <l_lw>:
  38:	80 00 00 00 	l.lw r0,0\(r0\)

0000003c <l_lbz>:
  3c:	84 00 00 00 	l.lbz r0,0\(r0\)

00000040 <l_lbs>:
  40:	88 00 00 00 	l.lbs r0,0\(r0\)

00000044 <l_lhz>:
  44:	8c 00 00 00 	l.lhz r0,0\(r0\)

00000048 <l_lhs>:
  48:	90 00 00 00 	l.lhs r0,0\(r0\)

0000004c <l_sw>:
  4c:	d4 00 00 00 	l.sw 0\(r0\),r0

00000050 <l_sb>:
  50:	d8 00 00 00 	l.sb 0\(r0\),r0

00000054 <l_sh>:
  54:	dc 00 00 00 	l.sh 0\(r0\),r0

00000058 <l_sll>:
  58:	e0 00 00 08 	l.sll r0,r0,r0

0000005c <l_slli>:
  5c:	b4 00 00 00 	l.slli r0,r0,0x0

00000060 <l_srl>:
  60:	e0 00 00 28 	l.srl r0,r0,r0

00000064 <l_srli>:
  64:	b4 00 00 20 	l.srli r0,r0,0x0

00000068 <l_sra>:
  68:	e0 00 00 48 	l.sra r0,r0,r0

0000006c <l_srai>:
  6c:	b4 00 00 40 	l.srai r0,r0,0x0

00000070 <l_ror>:
  70:	e0 00 00 88 	l.ror r0,r0,r0

00000074 <l_rori>:
  74:	b4 00 00 80 	l.rori r0,r0,0x0

00000078 <l_add>:
  78:	e0 00 00 00 	l.add r0,r0,r0

0000007c <l_addi>:
  7c:	94 00 00 00 	l.addi r0,r0,0

00000080 <l_sub>:
  80:	e0 00 00 02 	l.sub r0,r0,r0

00000084 <l_subi>:
  84:	9c 00 00 00 	l.subi r0,r0,0

00000088 <l_and>:
  88:	e0 00 00 03 	l.and r0,r0,r0

0000008c <l_andi>:
  8c:	a0 00 00 00 	l.andi r0,r0,0

00000090 <l_or>:
  90:	e0 00 00 04 	l.or r0,r0,r0

00000094 <l_ori>:
  94:	a4 00 00 00 	l.ori r0,r0,0

00000098 <l_xor>:
  98:	e0 00 00 05 	l.xor r0,r0,r0

0000009c <l_xori>:
  9c:	a8 00 00 00 	l.xori r0,r0,0

000000a0 <l_mul>:
  a0:	e0 00 00 06 	l.mul r0,r0,r0

000000a4 <l_muli>:
  a4:	ac 00 00 00 	l.muli r0,r0,0

000000a8 <l_div>:
  a8:	e0 00 00 09 	l.div r0,r0,r0

000000ac <l_divu>:
  ac:	e0 00 00 0a 	l.divu r0,r0,r0

000000b0 <l_sfgts>:
  b0:	e4 c0 00 00 	l.sfgts r0,r0

000000b4 <l_sfgtu>:
  b4:	e4 40 00 00 	l.sfgtu r0,r0

000000b8 <l_sfges>:
  b8:	e4 e0 00 00 	l.sfges r0,r0

000000bc <l_sfgeu>:
  bc:	e4 60 00 00 	l.sfgeu r0,r0

000000c0 <l_sflts>:
  c0:	e5 00 00 00 	l.sflts r0,r0

000000c4 <l_sfltu>:
  c4:	e4 80 00 00 	l.sfltu r0,r0

000000c8 <l_sfles>:
  c8:	e5 20 00 00 	l.sfles r0,r0

000000cc <l_sfleu>:
  cc:	e4 a0 00 00 	l.sfleu r0,r0

000000d0 <l_sfgtsi>:
  d0:	b8 c0 00 00 	l.sfgtsi r0,0

000000d4 <l_sfgtui>:
  d4:	b8 40 00 00 	l.sfgtui r0,0x0

000000d8 <l_sfgesi>:
  d8:	b8 e0 00 00 	l.sfgesi r0,0

000000dc <l_sfgeui>:
  dc:	b8 60 00 00 	l.sfgeui r0,0x0

000000e0 <l_sfltsi>:
  e0:	b9 00 00 00 	l.sfltsi r0,0

000000e4 <l_sfltui>:
  e4:	b8 80 00 00 	l.sfltui r0,0x0

000000e8 <l_sflesi>:
  e8:	b9 20 00 00 	l.sflesi r0,0

000000ec <l_sfleui>:
  ec:	b8 a0 00 00 	l.sfleui r0,0x0

000000f0 <l_sfeq>:
  f0:	e4 00 00 00 	l.sfeq r0,r0

000000f4 <l_sfeqi>:
  f4:	b8 00 00 00 	l.sfeqi r0,0

000000f8 <l_sfne>:
  f8:	e4 20 00 00 	l.sfne r0,r0

000000fc <l_sfnei>:
  fc:	b8 20 00 00 	l.sfnei r0,0
