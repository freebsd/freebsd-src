#as: --abi=32 -no-expand
#objdump: -dr
#source: case-1.s
#name: Case-insensitive registers and opcodes with -no-expand.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+8:[ 	]+e8000040[ 	]+pta/u	8 <start\+0x8>,tr4
[ 	]+8:[ 	]+R_SH_PT_16	foo
[ 	]+c:[ 	]+e8000630[ 	]+pta/l	10 <start\+0x10>,tr3
[ 	]+c:[ 	]+R_SH_PT_16	bar
[ 	]+10:[ 	]+cc00a820[ 	]+movi	42,r2
[ 	]+14:[ 	]+ebffee20[ 	]+pta/l	0 <start>,tr2
