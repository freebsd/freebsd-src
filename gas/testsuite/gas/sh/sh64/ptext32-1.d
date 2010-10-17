#as: --isa=shmedia --abi=32
#source: ptext-1.s
#objdump: -dr
#name: PT, PTA, PTB expansion for external symbols, 32-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+0:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym1\+0x20
[ 	]+4:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+4:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym1\+0x24
[ 	]+8:[ 	]+6bf56650[ 	]+ptrel/l	r25,tr5
[ 	]+c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym2\+0x24
[ 	]+10:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+10:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym2\+0x28
[ 	]+14:[ 	]+6bf56640[ 	]+ptrel/l	r25,tr4
[ 	]+18:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+18:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym3\+0x28
[ 	]+1c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+1c:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym3\+0x2c
[ 	]+20:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3
[ 	]+24:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+24:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym4\+0x2c
[ 	]+28:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+28:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym4\+0x30
[ 	]+2c:[ 	]+6bf56450[ 	]+ptrel/u	r25,tr5
[ 	]+30:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+30:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym5\+0x30
[ 	]+34:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+34:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym5\+0x34
[ 	]+38:[ 	]+6bf56440[ 	]+ptrel/u	r25,tr4
[ 	]+3c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+3c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym6\+0x34
[ 	]+40:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+40:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym6\+0x38
[ 	]+44:[ 	]+6bf56430[ 	]+ptrel/u	r25,tr3
