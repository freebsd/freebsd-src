# as: --no-target-align
# objdump: -d

# Test that a short branch with a target just barely out of range does
# not crash the assembler.

.*: +file format elf32-xtensa-.*

Disassembly of section .text:

00000000 <.text>:
   0:	.*        	bnez	a2, 0x45
   3:	.*        	nop
   6:	.*        	nop
   9:	.*        	nop
   c:	.*        	nop
   f:	.*        	nop
  12:	.*        	nop
  15:	.*        	nop
  18:	.*        	nop
  1b:	.*        	nop
  1e:	.*        	nop
  21:	.*        	nop
  24:	.*        	nop
  27:	.*        	nop
  2a:	.*        	nop
  2d:	.*        	nop
  30:	.*        	nop
  33:	.*        	nop
  36:	.*        	nop
  39:	.*        	nop
  3c:	.*        	nop
  3f:	.*        	nop
  42:	.*        	nop
