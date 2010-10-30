#objdump: --syms --special-syms -d
#name: ARM Mapping Symbols for .short (EABI version)
# This test is only valid on EABI based ports.
#target: *-*-*eabi *-*-symbianelf
#source: mapshort.s

# Test the generation and use of ARM ELF Mapping Symbols

.*: +file format .*arm.*

SYMBOL TABLE:
0+00 l    d  .text	00000000 .text
0+00 l    d  .data	00000000 .data
0+00 l    d  .bss	00000000 .bss
0+00 l     F .text	00000000 foo
0+00 l       .text	00000000 \$a
0+04 l       .text	00000000 \$t
0+08 l       .text	00000000 \$d
0+12 l       .text	00000000 \$t
0+16 l       .text	00000000 \$d
0+18 l       .text	00000000 \$a
0+1c l       .text	00000000 \$d
0+1f l       .text	00000000 bar
0+00 l       .data	00000000 wibble
0+00 l       .data	00000000 \$d
0+00 l    d  .ARM.attributes	00000000 .ARM.attributes


Disassembly of section .text:

0+00 <foo>:
   0:	e1a00000 	nop			\(mov r0,r0\)
   4:	46c0      	nop			\(mov r8, r8\)
   6:	46c0      	nop			\(mov r8, r8\)
   8:	00000002 	.word	0x00000002
   c:	00010001 	.word	0x00010001
  10:	0003      	.short	0x0003
  12:	46c0      	nop			\(mov r8, r8\)
  14:	46c0      	nop			\(mov r8, r8\)
  16:	0001      	.short	0x0001
  18:	ebfffff8 	bl	0 <foo>
  1c:	0008      	.short	0x0008
  1e:	09          	.byte	0x09
0+1f <bar>:
  1f:	0a          	.byte	0x0a
