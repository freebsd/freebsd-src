#as: --isa=shmedia --abi=64
#objdump: -dr
#name: MOVI expansion, 64-bit ABI, 64-bit subset.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000430[ 	]+movi	1,r3
[ 	]+4:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+8:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+c:[ 	]+cffffc30[ 	]+movi	-1,r3
[ 	]+10:[ 	]+c9fffc30[ 	]+shori	32767,r3
[ 	]+14:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+18:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+1c:[ 	]+ca000030[ 	]+shori	32768,r3
[ 	]+20:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+24:[ 	]+cdfffc30[ 	]+movi	32767,r3
[ 	]+28:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+2c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+30:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+34:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+38:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+3c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+40:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+44:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+48:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+4c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+50:[ 	]+c8000030[ 	]+shori	0,r3
