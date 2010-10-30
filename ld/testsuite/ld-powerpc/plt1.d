#source: plt1.s
#as: -a32
#objdump: -dr
#target: powerpc*-*-*

.*:     file format elf32-powerpc

Disassembly of section .text:

0+ <_start>:
   0:	42 9f 00 05 	bcl-    20,4\*cr7\+so,4 .*
   4:	7f c8 02 a6 	mflr    r30
   8:	3f de 00 00 	addis   r30,r30,0
			a: R_PPC_REL16_HA	_GLOBAL_OFFSET_TABLE_\+0x6
   c:	3b de 00 0a 	addi    r30,r30,10
			e: R_PPC_REL16_LO	_GLOBAL_OFFSET_TABLE_\+0xa
  10:	48 00 00 01 	bl      10 .*
			10: R_PPC_PLTREL24	_exit
  14:	48 00 00 00 	b       14 .*
			14: R_PPC_REL24	_start
