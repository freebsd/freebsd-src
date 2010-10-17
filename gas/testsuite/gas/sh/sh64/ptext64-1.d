#as: --isa=shmedia --abi=64
#source: ptext-1.s
#objdump: -dr
#name: PT, PTA, PTB expansion for external symbols, 64-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+0:[ 	]+R_SH_IMM_HI16_PCREL	externalsym1\+0x18
[ 	]+4:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+4:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym1\+0x1c
[ 	]+8:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+8:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym1\+0x20
[ 	]+c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+c:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym1\+0x24
[ 	]+10:[ 	]+6bf56650[ 	]+ptrel/l	r25,tr5
[ 	]+14:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+14:[ 	]+R_SH_IMM_HI16_PCREL	externalsym2\+0x1c
[ 	]+18:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+18:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym2\+0x20
[ 	]+1c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+1c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym2\+0x24
[ 	]+20:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+20:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym2\+0x28
[ 	]+24:[ 	]+6bf56640[ 	]+ptrel/l	r25,tr4
[ 	]+28:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+28:[ 	]+R_SH_IMM_HI16_PCREL	externalsym3\+0x20
[ 	]+2c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+2c:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym3\+0x24
[ 	]+30:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+30:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym3\+0x28
[ 	]+34:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+34:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym3\+0x2c
[ 	]+38:[ 	]+6bf56630[ 	]+ptrel/l	r25,tr3
[ 	]+3c:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+3c:[ 	]+R_SH_IMM_HI16_PCREL	externalsym4\+0x24
[ 	]+40:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+40:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym4\+0x28
[ 	]+44:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+44:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym4\+0x2c
[ 	]+48:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+48:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym4\+0x30
[ 	]+4c:[ 	]+6bf56450[ 	]+ptrel/u	r25,tr5
[ 	]+50:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+50:[ 	]+R_SH_IMM_HI16_PCREL	externalsym5\+0x28
[ 	]+54:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+54:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym5\+0x2c
[ 	]+58:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+58:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym5\+0x30
[ 	]+5c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+5c:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym5\+0x34
[ 	]+60:[ 	]+6bf56440[ 	]+ptrel/u	r25,tr4
[ 	]+64:[ 	]+cc000190[ 	]+movi	0,r25
[ 	]+64:[ 	]+R_SH_IMM_HI16_PCREL	externalsym6\+0x2c
[ 	]+68:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+68:[ 	]+R_SH_IMM_MEDHI16_PCREL	externalsym6\+0x30
[ 	]+6c:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+6c:[ 	]+R_SH_IMM_MEDLOW16_PCREL	externalsym6\+0x34
[ 	]+70:[ 	]+c8000190[ 	]+shori	0,r25
[ 	]+70:[ 	]+R_SH_IMM_LOW16_PCREL	externalsym6\+0x38
[ 	]+74:[ 	]+6bf56430[ 	]+ptrel/u	r25,tr3
