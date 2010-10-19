#as:
#objdump: -dr
#name: GAS segmentation fault

.*: +file format .*

Disassembly of section .text:

00000000 <__Z1flllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllc>:
   0:	ee ba       	jump	r14
	...

00000004 <_main>:
   4:	6f 34 00 40 	push	r15, {r14}
   8:	7e 30 00 00 	bal	r14, 0x8 <_main\+0x4>
			8: R_CRX_REL16	__Z1flllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllc
   c:	6f 32 00 40 	popret	r15, {r14}
