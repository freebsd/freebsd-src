#as: --abi=64
#objdump: -dr
#source: shift-2.s
#name: Shift expressions, 64-bit ABI, 64-bit subset.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+0:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x4
[ 	]+4:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+4:[ 	]+R_SH_IMM_HI16	\.data\+0x4
[ 	]+8:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+8:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x30
[ 	]+c:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+c:[ 	]+R_SH_IMM_HI16	\.data\+0x2f
[ 	]+10:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+10:[ 	]+R_SH_IMM_MEDHI16	externsym
[ 	]+14:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+14:[ 	]+R_SH_IMM_HI16	externsym
[ 	]+18:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+18:[ 	]+R_SH_IMM_MEDHI16	externsym\+0x29
[ 	]+1c:[ 	]+cc000040[ 	]+movi	0,r4
[ 	]+1c:[ 	]+R_SH_IMM_HI16	externsym\+0x2a
[ 	]+20:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+20:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x4
[ 	]+24:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+24:[ 	]+R_SH_IMM_HI16	\.data\+0x4
[ 	]+28:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+28:[ 	]+R_SH_IMM_MEDHI16	\.data\+0x30
[ 	]+2c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+2c:[ 	]+R_SH_IMM_HI16	\.data\+0x2f
[ 	]+30:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+30:[ 	]+R_SH_IMM_MEDHI16	externsym
[ 	]+34:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+34:[ 	]+R_SH_IMM_HI16	externsym
[ 	]+38:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+38:[ 	]+R_SH_IMM_MEDHI16	externsym\+0x29
[ 	]+3c:[ 	]+c8000040[ 	]+shori	0,r4
[ 	]+3c:[ 	]+R_SH_IMM_HI16	externsym\+0x2a
