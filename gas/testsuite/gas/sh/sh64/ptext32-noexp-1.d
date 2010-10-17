#as: --isa=shmedia --abi=32 -no-expand
#source: ptext-1.s
#objdump: -dr
#name: PT, PTA, PTB non-expansion for external symbols, 32-bit ABI.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+e8000250[ 	]+pta/l	0 <start>,tr5
[ 	]+0:[ 	]+R_SH_PT_16	externalsym1\+0x28
[ 	]+4:[ 	]+e8000640[ 	]+pta/l	8 <start\+0x8>,tr4
[ 	]+4:[ 	]+R_SH_PT_16	externalsym2\+0x2c
[ 	]+8:[ 	]+ec000630[ 	]+ptb/l	c <start\+0xc>,tr3
[ 	]+8:[ 	]+R_SH_PT_16	externalsym3\+0x30
[ 	]+c:[ 	]+e8000050[ 	]+pta/u	c <start\+0xc>,tr5
[ 	]+c:[ 	]+R_SH_PT_16	externalsym4\+0x34
[ 	]+10:[ 	]+e8000440[ 	]+pta/u	14 <start\+0x14>,tr4
[ 	]+10:[ 	]+R_SH_PT_16	externalsym5\+0x38
[ 	]+14:[ 	]+ec000430[ 	]+ptb/u	18 <start\+0x18>,tr3
[ 	]+14:[ 	]+R_SH_PT_16	externalsym6\+0x3c
