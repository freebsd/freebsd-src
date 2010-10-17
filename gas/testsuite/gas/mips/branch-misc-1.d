#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS branch-misc-1
#as: -32

# Test the branches to local symbols in current file.

.*: +file format .*mips.*

Disassembly of section .text:
	\.\.\.
	\.\.\.
	\.\.\.
0+003c <[^>]*> 0411fff0 	bal	00000000 <l1>
0+0040 <[^>]*> 00000000 	nop
0+0044 <[^>]*> 0411fff3 	bal	00000014 <l2>
0+0048 <[^>]*> 00000000 	nop
0+004c <[^>]*> 0411fff6 	bal	00000028 <l3>
0+0050 <[^>]*> 00000000 	nop
0+0054 <[^>]*> 0411000a 	bal	00000080 <l4>
0+0058 <[^>]*> 00000000 	nop
0+005c <[^>]*> 0411000d 	bal	00000094 <l5>
0+0060 <[^>]*> 00000000 	nop
0+0064 <[^>]*> 04110010 	bal	000000a8 <l6>
0+0068 <[^>]*> 00000000 	nop
	\.\.\.
	\.\.\.
	\.\.\.
	\.\.\.
