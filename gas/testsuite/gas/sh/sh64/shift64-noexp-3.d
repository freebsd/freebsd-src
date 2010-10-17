#as: --abi=64 -no-expand
#objdump: -dr
#source: shift-3.s
#name: Shift expression, local but undefined symbol, 64-bit ABI with -no-expand.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000010[ 	]+movi	0,r1
[ 	]+0:[ 	]+R_SH_IMM_LOW16	\.LC0
[ 	]+4:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+4:[ 	]+R_SH_IMM_MEDLOW16	\.LC0
