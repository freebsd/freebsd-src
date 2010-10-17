#as: --isa=shmedia --abi=64
#objdump: -dr
#source: movi-2.s
#name: MOVI expansion of local symbols with relocs, 64-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

0+ <start>:
[ 	]+0:[ 	]+cc000210[ 	]+movi	0,r33
[ 	]+0:[ 	]+R_SH_IMM_HI16	\.text\+0x51
[ 	]+4:[ 	]+c8000210[ 	]+shori	0,r33
[ 	]+4:[ 	]+R_SH_IMM_MEDHI16	\.text\+0x51
[ 	]+8:[ 	]+c8000210[ 	]+shori	0,r33
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16	\.text\+0x51
[ 	]+c:[ 	]+c8000210[ 	]+shori	0,r33
[ 	]+c:[ 	]+R_SH_IMM_LOW16	\.text\+0x51
[ 	]+10:[ 	]+cc000360[ 	]+movi	0,r54
[ 	]+10:[ 	]+R_SH_IMM_HI16	\.data\+0x2c
[ 	]+14:[ 	]+c8000360[ 	]+shori	0,r54
[ 	]+14:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x2c
[ 	]+18:[ 	]+c8000360[ 	]+shori	0,r54
[ 	]+18:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x2c
[ 	]+1c:[ 	]+c8000360[ 	]+shori	0,r54
[ 	]+1c:[ 	]+R_SH_IMM_LOW16	\.data\+0x2c
[ 	]+20:[ 	]+cc0000f0[ 	]+movi	0,r15
[ 	]+20:[ 	]+R_SH_IMM_HI16	\.text\.other\+0x35
[ 	]+24:[ 	]+c80000f0[ 	]+shori	0,r15
[ 	]+24:[ 	]+R_SH_IMM_MEDHI16	\.text\.other\+0x35
[ 	]+28:[ 	]+c80000f0[ 	]+shori	0,r15
[ 	]+28:[ 	]+R_SH_IMM_MEDLOW16	\.text\.other\+0x35
[ 	]+2c:[ 	]+c80000f0[ 	]+shori	0,r15
[ 	]+2c:[ 	]+R_SH_IMM_LOW16	\.text\.other\+0x35

0+30 <forw>:
[ 	]+30:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+30:[ 	]+R_SH_IMM_HI16	\.data\.other\+0x38
[ 	]+34:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+34:[ 	]+R_SH_IMM_MEDHI16	\.data\.other\+0x38
[ 	]+38:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+38:[ 	]+R_SH_IMM_MEDLOW16	\.data\.other\+0x38
[ 	]+3c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+3c:[ 	]+R_SH_IMM_LOW16	\.data\.other\+0x38
Disassembly of section \.text\.other:

0+ <forwdummylabel>:
[ 	]+0:[ 	]+6ff0fff0[ 	]+nop	
[ 	]+4:[ 	]+6ff0fff0[ 	]+nop	

0+8 <forwothertext>:
[ 	]+8:[ 	]+6ff0fff0[ 	]+nop	
