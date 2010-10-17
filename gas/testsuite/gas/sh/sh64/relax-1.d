#as: --abi=64
#objdump: -dr
#source: relax-1.s
#name: Assembler PT relaxation limit, from first to second state.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start2>:
[ 	]+4:[ 	]+cc000990[ 	]+movi	2,r25
[ 	]+8:[ 	]+c8000590[ 	]+shori	1,r25
[ 	]+c:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3

0+10 <x1>:
[ 	]+10:[ 	]+e9fffe40[ 	]+pta/l	2000c <x0>,tr4
[ 	]+\.\.\.

0+2000c <x0>:
[ 	]+2000c:[ 	]+ea000650[ 	]+pta/l	10 <x1>,tr5
[ 	]+20010:[ 	]+ea000260[ 	]+pta/l	10 <x1>,tr6
[ 	]+20014:[ 	]+cffff590[ 	]+movi	-3,r25
[ 	]+20018:[ 	]+cbffd590[ 	]+shori	65525,r25
[ 	]+2001c:[ 	]+6bf56660[ 	]+ptrel/l	r25,tr6
[ 	]+20020:[ 	]+cffff590[ 	]+movi	-3,r25
[ 	]+20024:[ 	]+cbffa590[ 	]+shori	65513,r25
[ 	]+20028:[ 	]+6bf56670[ 	]+ptrel/l	r25,tr7
