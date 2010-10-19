#as: --underscore --em=criself --march=v32
#objdump: -dr

# Check expansion of "ba" into dword operands for different segment.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a-0x2>:
   0:	7fa2                	moveq -1,r10

00000002 <a>:
   2:	bf0e 0000 0000      	ba 2 <a>
			4: R_CRIS_32_PCREL	\.text\.2\+0x8
   8:	4152                	moveq 1,r5
	\.\.\.
Disassembly of section \.text\.2:

00000000 <b-0x2>:
   0:	4822                	moveq 8,r2

00000002 <b>:
   2:	4232                	moveq 2,r3
   4:	bf0e 0000 0000      	ba 4 <b\+0x2>
			6: R_CRIS_32_PCREL	\.text\+0x8
   a:	4472                	moveq 4,r7
