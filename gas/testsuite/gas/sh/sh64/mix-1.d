#as: --abi=32
#objdump: -dr
#name: Mixed-ISA objects.

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+89 01       	bt	6 <forw>
[ 	]+2:[ 	]+c7 00[ 	]+mova	4 <start2>,r0

0+4 <start2>:
[ 	]+4:[ 	]+00[ 	]+09       	nop	

0+6 <forw>:
[ 	]+6:[ 	]+00[ 	]+09       	nop	
Disassembly of section \.text\.media:

0+ <mediacode>:
[ 	]+0:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+0:[ 	]+R_SH_IMM_MEDLOW16_PCREL	\.text\+0xf*fffffffe
[ 	]+4:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+4:[ 	]+R_SH_IMM_LOW16_PCREL	\.text\+0x2
[ 	]+8:[ 	]+6bf56640[ 	]+ptrel/l	r25,tr4
[ 	]+c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	\.text\+0xf*fffffffc
[ 	]+10:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+10:[ 	]+R_SH_IMM_LOW16_PCREL	\.text
[ 	]+14:[ 	]+6bf56650[ 	]+ptrel/l	r25,tr5

0+18 <mediacode2>:
[ 	]+18:[ 	]+cc000360[ 	]+movi	0,r54
[ 	]+18:[ 	]+R_SH_IMM_MEDLOW16	\.text\+0x4
[ 	]+1c:[ 	]+c8000360[ 	]+shori	0,r54
[ 	]+1c:[ 	]+R_SH_IMM_LOW16	\.text\+0x4
[ 	]+20:[ 	]+cc0002d0[ 	]+movi	0,r45
[ 	]+20:[ 	]+R_SH_IMM_MEDLOW16	\.text\.media\+0x19
[ 	]+24:[ 	]+c80002d0[ 	]+shori	0,r45
[ 	]+24:[ 	]+R_SH_IMM_LOW16	\.text\.media\+0x19
[ 	]+28:[ 	]+ebfff270[ 	]+pta/l	18 <mediacode2>,tr7
[ 	]+2c:[ 	]+6ff0fff0[ 	]+nop	
