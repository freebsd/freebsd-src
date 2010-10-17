#as: --abi=32
#objdump: -dr
#name: Case-insensitive registers and opcodes.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+8:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16_PCREL	foo\+0xf*ff8
[ 	]+c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+c:[ 	]+R_SH_IMM_LOW16_PCREL	foo\+0xf*ffc
[ 	]+10:[ 	]+6bf56440[ 	]+ptrel/u	r25,tr4
[ 	]+14:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+14:[ 	]+R_SH_IMM_MEDLOW16_PCREL	bar\+0xf*ff8
[ 	]+18:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+18:[ 	]+R_SH_IMM_LOW16_PCREL	bar\+0xf*ffc
[ 	]+1c:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3
[ 	]+20:[ 	]+cc00a820[ 	]+movi	42,r2
[ 	]+24:[ 	]+ebffde20[ 	]+pta/l	0 <start>,tr2
