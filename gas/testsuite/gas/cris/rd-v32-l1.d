#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
   0:	7f5d 0000 0000      	lapc 0 <a>,r5
			2: R_CRIS_32_PCREL	\*ABS\*\+0x7
   6:	7f6d faff ffff      	lapc 0 <a>,r6
   c:	7f7d 0000 0000      	lapc c <a\+0xc>,r7
			e: R_CRIS_32_PCREL	\*ABS\*\+0xa
	\.\.\.
