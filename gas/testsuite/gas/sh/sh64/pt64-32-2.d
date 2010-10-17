#as: --isa=shmedia -abi=64 -expand-pt32
#objdump: -dr
#source: pt-2.s
#name: Inter-segment PT, 64-bit with -expand-pt32.

.*:     file format .*-sh64.*

Disassembly of section \.text:
0+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start1>:
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

0+8 <start4>:
[ 	]+8:[ 	]+ebfffe50[ 	]+pta/l	4 <start1>,tr5
[ 	]+c:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+10:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+10:[ 	]+R_SH_IMM_MEDLOW16_PCREL	\.text\.other\+0xfffffffffffffffd
[ 	]+14:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+14:[ 	]+R_SH_IMM_LOW16_PCREL	\.text\.other\+0x1
[ 	]+18:[ 	]+6bf56670[ 	]+ptrel/l	r25,tr7
[ 	]+1c:[ 	]+6ff0fff0[ 	]+nop	
Disassembly of section \.text\.other:

0+ <dummylabel>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start2>:
[ 	]+4:[ 	]+e8000a40[ 	]+pta/l	c <start3>,tr4
[ 	]+8:[ 	]+6ff0fff0[ 	]+nop	

0+c <start3>:
[ 	]+c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	\.text\+0x1
[ 	]+10:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+10:[ 	]+R_SH_IMM_LOW16_PCREL	\.text\+0x5
[ 	]+14:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3
[ 	]+18:[ 	]+6ff0fff0[ 	]+nop	
