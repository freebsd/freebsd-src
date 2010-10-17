#as: --isa=shmedia --abi=32
#objdump: -dr
#source: movi-2.s
#name: MOVI expansion of local symbols with relocs, 32-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+cc000210[ 	]+movi	0,r33
[ 	]+0:[ 	]+R_SH_IMM_MEDLOW16	\.text\+0x39
[ 	]+4:[ 	]+c8000210[ 	]+shori	0,r33
[ 	]+4:[ 	]+R_SH_IMM_LOW16	\.text\+0x39
[ 	]+8:[ 	]+cc000360[ 	]+movi	0,r54
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x2c
[ 	]+c:[ 	]+c8000360[ 	]+shori	0,r54
[ 	]+c:[ 	]+R_SH_IMM_LOW16	\.data\+0x2c
[ 	]+10:[ 	]+cc0000f0[ 	]+movi	0,r15
[ 	]+10:[ 	]+R_SH_IMM_MEDLOW16	\.text\.other\+0x35
[ 	]+14:[ 	]+c80000f0[ 	]+shori	0,r15
[ 	]+14:[ 	]+R_SH_IMM_LOW16	\.text\.other\+0x35

0+18 <forw>:
[ 	]+18:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+18:[ 	]+R_SH_IMM_MEDLOW16	\.data\.other\+0x38
[ 	]+1c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+1c:[ 	]+R_SH_IMM_LOW16	\.data\.other\+0x38
Disassembly of section \.text\.other:

0+ <forwdummylabel>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

0+8 <forwothertext>:
[ 	]+8:[ 	]+6ff0fff0[ 	]+nop	
