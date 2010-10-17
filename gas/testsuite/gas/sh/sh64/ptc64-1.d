#as: --abi=64
#objdump: -dr
#source: ptc-1.s
#name: PT constant, 64-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+0:[ 	]+R_SH_IMM_HI16_PCREL	\*ABS\*\+0xf0
[ 	]+4:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+4:[ 	]+R_SH_IMM_MEDHI16_PCREL	\*ABS\*\+0xf4
[ 	]+8:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16_PCREL	\*ABS\*\+0xf8
[ 	]+c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+c:[ 	]+R_SH_IMM_LOW16_PCREL	\*ABS\*\+0xfc
[ 	]+10:[ 	]+6bf56610[ 	]+ptrel/l	r25,tr1
