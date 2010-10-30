#objdump: -dzr

.*: *file format elf32-mep

Disassembly of section \.text:

.* <.*>:
  .*:	00 00  *	nop
  .*:	e4 51 00 04 *	beq \$4,\$5,.* <foo>
  .*:	00 00  *	nop
  .*:	00 00  *	nop

.* <foo>:
  .*:	00 00  *	nop
