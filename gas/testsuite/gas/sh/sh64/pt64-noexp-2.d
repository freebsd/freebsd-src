#as: --isa=shmedia -abi=64 -no-expand
#objdump: -dr
#source: pt-2.s
#name: Inter-segment PT, 64-bit with -no-expand.

.*:     file format .*-sh64.*

Disassembly of section \.text:
0+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start1>:
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

0+8 <start4>:
[ 	]+8:[ 	]+ebfffe50[ 	]+pta/l	4 <start1>,tr5
[ 	]+c:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+10:[ 	]+e8000270[ 	]+pta/l	10 <start4\+0x8>,tr7
[ 	]+10:[ 	]+R_SH_PT_16	\.text\.other\+0x5
[ 	]+14:[ 	]+6ff0fff0[ 	]+nop	

Disassembly of section \.text\.other:

0+ <dummylabel>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start2>:
[ 	]+4:[ 	]+e8000a40[ 	]+pta/l	c <start3>,tr4
[ 	]+8:[ 	]+6ff0fff0[ 	]+nop	

0+c <start3>:
[ 	]+c:[ 	]+e8000630[ 	]+pta/l	10 <start3\+0x4>,tr3
[ 	]+c:[ 	]R_SH_PT_16	\.text\+0x9
[ 	]+10:[ 	]+6ff0fff0[ 	]+nop	
