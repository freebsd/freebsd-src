#as: --isa=shmedia --abi=32
#objdump: -dr
#source: movi-1.s
#name: MOVI expansion, 32-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+0:[ 	]+R_SH_IMM_MEDLOW16	externalsym\+0x7b
[ 	]+4:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+4:[ 	]+R_SH_IMM_LOW16	externalsym\+0x7b
[ 	]+8:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+c:[ 	]+cbfffc30[ 	]+shori	65535,r3
[ 	]+10:[ 	]+cc000430[ 	]+movi	1,r3
[ 	]+14:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+18:[ 	]+cffffc30[ 	]+movi	-1,r3
[ 	]+1c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+20:[ 	]+cdfffc30[ 	]+movi	32767,r3
[ 	]+24:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+28:[ 	]+ca000030[ 	]+shori	32768,r3
[ 	]+2c:[ 	]+cdfffc30[ 	]+movi	32767,r3
[ 	]+30:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+34:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+38:[ 	]+cffffc30[ 	]+movi	-1,r3
[ 	]+3c:[ 	]+c9fffc30[ 	]+shori	32767,r3
[ 	]+40:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+44:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+48:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+48:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x49
[ 	]+4c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+4c:[ 	]+R_SH_IMM_LOW16	\.data\+0x49
[ 	]+50:[ 	]+cc001440[ 	]+movi	5,r4
