#as: --abi=64
#objdump: -dr
#source: relax-2.s
#name: Assembler PTB relaxation limit, from first to second state.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start2>:
[ 	]+4:[ 	]+cc000990[ 	]+movi	2,r25
[ 	]+8:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+c:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3

0+10 <[ax]1>:
[ 	]+10:[ 	]+edfffe40[ 	]+ptb/l	2000c <[ax]0>,tr4
[ 	]+\.\.\.

0+2000c <[ax]0>:
[ 	]+2000c:[ 	]+ee000650[ 	]+ptb/l	10 <[ax]1>,tr5
[ 	]+20010:[ 	]+ee000260[ 	]+ptb/l	10 <[ax]1>,tr6
[ 	]+20014:[ 	]+cffff590[ 	]+movi	-3,r25
[ 	]+20018:[ 	]+cbffd190[ 	]+shori	65524,r25
[ 	]+2001c:[ 	]+6bf56660[ 	]+ptrel/l	r25,tr6
[ 	]+20020:[ 	]+cffff590[ 	]+movi	-3,r25
[ 	]+20024:[ 	]+cbffa190[ 	]+shori	65512,r25
[ 	]+20028:[ 	]+6bf56670[ 	]+ptrel/l	r25,tr7
