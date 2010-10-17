#as: --abi=32
#objdump: -dr
#source: crange2.s
#name: PT to SHcompact

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <shmedia>:
[ 	]+0:[ 	]+e8000a30[ 	]+pta/l	8 <shmedia1>,tr3
[ 	]+4:[ 	]+ec001240[ 	]+ptb/l	14 <shcompact1>,tr4

0+8 <shmedia1>:
[ 	]+8:[ 	]+ec001250[ 	]+ptb/l	18 <shcompact2>,tr5

0+c <shmedia2>:
[ 	]+c:[ 	]+6ff0fff0[ 	]+nop	

0+10[ 	]+<shcompact>:
[ 	]+10:[ 	]+00[ 	]+09[ 	]+nop	
[ 	]+12:[ 	]+00[ 	]+09[ 	]+nop	

0+14 <shcompact1>:
[ 	]+14:[ 	]+00[ 	]+09[ 	]+nop	
[ 	]+16:[ 	]+00[ 	]+09[ 	]+nop	

0+18 <shcompact2>:
[ 	]+18:[ 	]+00[ 	]+09[ 	]+nop	
[ 	]+1a:[ 	]+00[ 	]+09[ 	]+nop	

0+1c <shcompact3>:
[ 	]+1c:[ 	]+00[ 	]+09[ 	]+nop	
[ 	]+1e:[ 	]+00[ 	]+09[ 	]+nop	

0+20[ 	]+<shcompact4>:
[ 	]+20:[ 	]+00[ 	]+09[ 	]+nop	
[ 	]+22:[ 	]+00[ 	]+09[ 	]+nop	

0+24 <shmedia3>:
[ 	]+24:[ 	]+effffa60[ 	]+ptb/l	1c <shcompact3>,tr6
[ 	]+28:[ 	]+effffa70[ 	]+ptb/l	20[ 	]+<shcompact4>,tr7
[ 	]+2c:[ 	]+ebffe200[ 	]+pta/l	c <shmedia2>,tr0
