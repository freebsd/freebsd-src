#as:
#objdump: -dr
#name: relax-1

.*: +file format .*

Disassembly of section .text:

0* <DoesNotWork>:
 *0:	70 00 70 00 *	nop -> nop

0*4 <Work>:
 *4:	70 00 70 00 *	nop -> nop
Disassembly of section .branch:

0* <branch>:
 *0:	ff 00 00 00 	bra 0 <branch>
[ 	]*0: R_M32R_26_PCREL_RELA	.text\+0x4
