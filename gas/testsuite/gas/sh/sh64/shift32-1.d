#as: --abi=32
#objdump: -dr
#source: shift-1.s
#name: Shift expressions, 32-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+0:[ 	]+R_SH_IMM_LOW16	\.data\+0x4
[ 	]+4:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+4:[ 	]+R_SH_IMM_LOW16	\.data\+0x4
[ 	]+8:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x4
[ 	]+c:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+c:[ 	]+R_SH_IMM_LOW16	externsym
[ 	]+10:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+10:[ 	]+R_SH_IMM_LOW16	externsym
[ 	]+14:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+14:[ 	]+R_SH_IMM_MEDLOW16	externsym
[ 	]+18:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+18:[ 	]+R_SH_IMM_LOW16	\.data\+0x4
[ 	]+1c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+1c:[ 	]+R_SH_IMM_LOW16	\.data\+0x4
[ 	]+20:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+20:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x4
[ 	]+24:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+24:[ 	]+R_SH_IMM_LOW16	externsym
[ 	]+28:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+28:[ 	]+R_SH_IMM_LOW16	externsym
[ 	]+2c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+2c:[ 	]+R_SH_IMM_MEDLOW16	externsym
[ 	]+30:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+30:[ 	]+R_SH_IMM_LOW16	\.data\+0x2e
[ 	]+34:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+34:[ 	]+R_SH_IMM_LOW16	\.data\+0x2f
[ 	]+38:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+38:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x30
[ 	]+3c:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+3c:[ 	]+R_SH_IMM_LOW16	externsym\+0x2d
[ 	]+40:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+40:[ 	]+R_SH_IMM_LOW16	externsym\+0x2e
[ 	]+44:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+44:[ 	]+R_SH_IMM_MEDLOW16	externsym\+0x2f
[ 	]+48:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+48:[ 	]+R_SH_IMM_LOW16	\.data\+0x2e
[ 	]+4c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+4c:[ 	]+R_SH_IMM_LOW16	\.data\+0x2f
[ 	]+50:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+50:[ 	]+R_SH_IMM_MEDLOW16	\.data\+0x30
[ 	]+54:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+54:[ 	]+R_SH_IMM_LOW16	externsym\+0x2d
[ 	]+58:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+58:[ 	]+R_SH_IMM_LOW16	externsym\+0x2e
[ 	]+5c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+5c:[ 	]+R_SH_IMM_MEDLOW16	externsym\+0x2f
