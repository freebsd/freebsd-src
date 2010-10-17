#as: --abi=64 -no-expand
#objdump: -dr
#source: ptc-1.s
#name: PT constant, 64-bit ABI with -no-expand.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+e8000610[ 	]+pta/l	4 <start\+0x4>,tr1
[ 	]+0:[ 	]+R_SH_PT_16	\*ABS\*\+0x100

