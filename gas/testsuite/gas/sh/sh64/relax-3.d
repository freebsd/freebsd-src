#as: --abi=64
#objdump: -dr
#source: relax-3.s
#name: Assembler PC-rel MOVI relaxation limit, from first to second state.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	

0+4 <start2>:
[ 	]+4:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+8:[ 	]+ca001030[ 	]+shori	32772,r3

0+c <x1>:
[ 	]+c:[ 	]+cdfffc40[ 	]+movi	32767,r4
[ 	]+\.\.\.

0+800c <x0>:
[ 	]+800c:[ 	]+ce000050[ 	]+movi	-32768,r5
[ 	]+8010:[ 	]+cffffc60[ 	]+movi	-1,r6
[ 	]+8014:[ 	]+c9fffc60[ 	]+shori	32767,r6
[ 	]+8018:[ 	]+cffffc70[ 	]+movi	-1,r7
[ 	]+801c:[ 	]+cbfffc70[ 	]+shori	65535,r7
[ 	]+8020:[ 	]+cbfffc70[ 	]+shori	65535,r7
[ 	]+8024:[ 	]+ca000070[ 	]+shori	32768,r7
[ 	]+8028:[ 	]+cc000080[ 	]+movi	0,r8
[ 	]+802c:[ 	]+c8000080[ 	]+shori	0,r8
[ 	]+8030:[ 	]+c8000080[ 	]+shori	0,r8
[ 	]+8034:[ 	]+c9fffc80[ 	]+shori	32767,r8
[ 	]+8038:[ 	]+cc000080[ 	]+movi	0,r8
[ 	]+803c:[ 	]+c8000080[ 	]+shori	0,r8
[ 	]+8040:[ 	]+c8000080[ 	]+shori	0,r8
[ 	]+8044:[ 	]+c8004080[ 	]+shori	16,r8
Disassembly of section \.text\.another:

0+ <y0>:
[ 	]+0:[ 	]+cc000090[ 	]+movi	0,r9
[ 	]+4:[ 	]+c8000090[ 	]+shori	0,r9
[ 	]+8:[ 	]+c8000090[ 	]+shori	0,r9
[ 	]+c:[ 	]+c8002090[ 	]+shori	8,r9
