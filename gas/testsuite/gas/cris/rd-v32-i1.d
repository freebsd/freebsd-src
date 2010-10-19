#as: --underscore --em=criself --march=v32
#objdump: -dr

# Test that addc recognizes constant operands.

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <x>:
   0:	afad ffff ffff      	addc 0xffffffff,r10
   6:	affd 4000 0000      	addc 40 <x\+0x40>,acr
   c:	af5d 0100 0000      	addc 1 <x\+0x1>,r5
  12:	af7d 0000 0000      	addc 0 <x>,r7
			14: R_CRIS_32	extsym\+0x140
  18:	af0d 0000 0000      	addc 0 <x>,r0
  1e:	af4d e782 3101      	addc 13182e7 <x\+0x13182e7>,r4
  24:	affd 0f00 0000      	addc f <x\+0xf>,acr
	\.\.\.
