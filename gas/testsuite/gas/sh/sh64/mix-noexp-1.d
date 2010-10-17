#as: --abi=32 -no-expand
#objdump: -dr
#source: mix-1.s
#name: Mixed-ISA objects with -no-expand.

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
[ 	]+0:[ 	]+ec000640[ 	]+ptb/l	4 <mediacode\+0x4>,tr4
[ 	]+0:[ 	]+R_SH_PT_16[ 	]+\.text\+0x6
[ 	]+4:[ 	]+e8000250[ 	]+pta/l	4 <mediacode\+0x4>,tr5
[ 	]+4:[ 	]+R_SH_PT_16[ 	]+\.text\+0x4

0+8 <mediacode2>:
[ 	]+8:[ 	]+cc000360[ 	]+movi	0,r54
[ 	]+8:[ 	]+R_SH_IMMS16[ 	]+\.text\+0x4
[ 	]+c:[ 	]+cc0002d0[ 	]+movi	0,r45
[ 	]+c:[ 	]+R_SH_IMMS16[ 	]+\.text\.media\+0x9
[ 	]+10:[ 	]+ebfffa70[ 	]+pta/l	8 <mediacode2>,tr7
[ 	]+14:[ 	]+6ff0fff0[ 	]+nop	
