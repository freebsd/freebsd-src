#as: --isa=shmedia --abi=64
#objdump: -dr
#source: movi-1.s
#name: MOVI expansion, 64-bit ABI, 32-bit subset.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+0:[ 	]+R_SH_IMM_HI16	externalsym\+0x7b
[ 	]+4:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+4:[ 	]+R_SH_IMM_MEDHI16	externalsym\+0x7b
[ 	]+8:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16	externalsym\+0x7b
[ 	]+c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+c:[ 	]+R_SH_IMM_LOW16	externalsym\+0x7b
[ 	]+10:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+14:[ 	]+cbfffc30[ 	]+shori	65535,r3
[ 	]+18:[ 	]+cc000430[ 	]+movi	1,r3
[ 	]+1c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+20:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+24:[ 	]+cbfffc30[ 	]+shori	65535,r3
[ 	]+28:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+2c:[ 	]+cdfffc30[ 	]+movi	32767,r3
[ 	]+30:[ 	]+cc000030[ 	]+movi	0,r3
[ 	]+34:[ 	]+ca000030[ 	]+shori	32768,r3
[ 	]+38:[ 	]+cdfffc30[ 	]+movi	32767,r3
[ 	]+3c:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+40:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+44:[ 	]+cffffc30[ 	]+movi	-1,r3
[ 	]+48:[ 	]+c9fffc30[ 	]+shori	32767,r3
[ 	]+4c:[ 	]+ce000030[ 	]+movi	-32768,r3
[ 	]+50:[ 	]+c8000030[ 	]+shori	0,r3
[ 	]+54:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+54:[ 	]+R_SH_IMM_HI16	\.data\+0x49
[ 	]+58:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+58:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x49
[ 	]+5c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+5c:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x49
[ 	]+60:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+60:[ 	]+R_SH_IMM_LOW16	\.data\+0x49
[ 	]+64:[ 	]+cc001440[ 	]+movi	5,r4
