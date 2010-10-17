#as: --isa=shmedia --abi=64 -no-expand
#objdump: -dr
#source: movi-2.s
#name: MOVI non-expansion of local symbols with relocs, 64-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+cc000210[ 	]+movi	0,r33
[ 	]+0:[ 	]+R_SH_IMMS16	\.text\+0x2d
[ 	]+4:[ 	]+cc000360[ 	]+movi	0,r54
[ 	]+4:[ 	]+R_SH_IMMS16	\.data\+0x2c
[ 	]+8:[ 	]+cc0000f0[ 	]+movi	0,r15
[ 	]+8:[ 	]+R_SH_IMMS16	\.text\.other\+0x35

0+c <forw>:
[ 	]+c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+c:[ 	]+R_SH_IMMS16	\.data\.other\+0x38
Disassembly of section \.text\.other:

0+ <forwdummylabel>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

0+8 <forwothertext>:
[ 	]+8:[ 	]+6ff0fff0[ 	]+nop	
