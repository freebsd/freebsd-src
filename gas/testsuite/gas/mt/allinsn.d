#as: -nosched
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <add>:
   0:	00 00 00 00 	add R0,R0,R0

00000004 <addu>:
   4:	02 00 00 00 	addu R0,R0,R0

00000008 <addi>:
   8:	01 00 00 00 	addi R0,R0,#\$0

0000000c <addui>:
   c:	03 00 00 00 	addui R0,R0,#\$0

00000010 <sub>:
  10:	04 00 00 00 	sub R0,R0,R0

00000014 <subu>:
  14:	06 00 00 00 	subu R0,R0,R0

00000018 <subi>:
  18:	05 00 00 00 	subi R0,R0,#\$0

0000001c <subui>:
  1c:	07 00 00 00 	subui R0,R0,#\$0

00000020 <and>:
  20:	10 00 00 00 	and R0,R0,R0

00000024 <andi>:
  24:	11 00 00 00 	andi R0,R0,#\$0

00000028 <or>:
  28:	12 01 00 00 	or R0,R0,R1

0000002c <ori>:
  2c:	13 00 00 00 	ori R0,R0,#\$0

00000030 <xor>:
  30:	14 00 00 00 	xor R0,R0,R0

00000034 <xori>:
  34:	15 00 00 00 	xori R0,R0,#\$0

00000038 <nand>:
  38:	16 00 00 00 	nand R0,R0,R0

0000003c <nandi>:
  3c:	17 00 00 00 	nandi R0,R0,#\$0

00000040 <nor>:
  40:	18 00 00 00 	nor R0,R0,R0

00000044 <nori>:
  44:	19 00 00 00 	nori R0,R0,#\$0

00000048 <xnor>:
  48:	1a 00 00 00 	xnor R0,R0,R0

0000004c <xnori>:
  4c:	1b 00 00 00 	xnori R0,R0,#\$0

00000050 <ldui>:
  50:	1d 00 00 00 	ldui R0,#\$0

00000054 <lsl>:
  54:	20 00 00 00 	lsl R0,R0,R0

00000058 <lsli>:
  58:	21 00 00 00 	lsli R0,R0,#\$0

0000005c <lsr>:
  5c:	22 00 00 00 	lsr R0,R0,R0

00000060 <lsri>:
  60:	23 00 00 00 	lsri R0,R0,#\$0

00000064 <asr>:
  64:	24 00 00 00 	asr R0,R0,R0

00000068 <asri>:
  68:	25 00 00 00 	asri R0,R0,#\$0

0000006c <brlt>:
  6c:	31 00 00 00 	brlt R0,R0,6c <brlt>

00000070 <brle>:
  70:	33 00 00 00 	brle R0,R0,70 <brle>

00000074 <breq>:
  74:	35 00 00 00 	breq R0,R0,74 <breq>

00000078 <jmp>:
  78:	37 00 00 00 	jmp 78 <jmp>

0000007c <jal>:
  7c:	38 00 00 00 	jal R0,R0

00000080 <ei>:
  80:	60 00 00 00 	ei

00000084 <di>:
  84:	62 00 00 00 	di

00000088 <reti>:
  88:	66 00 00 00 	reti R0

0000008c <ldw>:
  8c:	41 00 00 00 	ldw R0,R0,#\$0

00000090 <stw>:
  90:	43 00 00 00 	stw R0,R0,#\$0

00000094 <si>:
  94:	64 00 00 00 	si R0

00000098 <brne>:
  98:	3b 00 00 00 	brne R0,R0,98 <brne>

0000009c <break>:
  9c:	68 00 00 00 	break

000000a0 <nop>:
  a0:	12 00 00 00 	nop
