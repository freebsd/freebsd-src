#as: --isa=shmedia -no-expand
#objdump: -dr
#source: pt-1.s
#name: Basic SHmedia PT and PTA instructions with -no-expand.

.*:     file format .*-sh64.*

Disassembly of section \.text:
[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

[0]+4 <start1>:
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

[0]+8 <start4>:
[ 	]+8:[ 	]+ebfffe50[ 	]+pta/l	4 <start1>,tr5
[ 	]+c:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+10:[ 	]+e8000a70[ 	]+pta/l	18 <start2>,tr7
[ 	]+14:[ 	]+6ff0fff0[ 	]+nop	

[0]+18 <start2>:
[ 	]+18:[ 	]+e8000a40[ 	]+pta/l	20 <start3>,tr4
[ 	]+1c:[ 	]+6ff0fff0[ 	]+nop	

[0]+20 <start3>:
[ 	]+20:[ 	]+ebffea30[ 	]+pta/l	8 <start4>,tr3
[ 	]+24:[ 	]+6ff0fff0[ 	]+nop	
