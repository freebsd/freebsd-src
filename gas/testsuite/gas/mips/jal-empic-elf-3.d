#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS jal-empic-elf-3
#as: -32 -membedded-pic

# Test the jal macro harder with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
	\.\.\.
	\.\.\.
0+0018 <[^>]*> 0411fffa 	bal	0+0004 <g1\-0x8>
[ 	]*18: R_MIPS_GNU_REL16_S2	.text
0+001c <[^>]*> 00000000 	nop
0+0020 <[^>]*> 0411fff8 	bal	0+0004 <g1\-0x8>
[ 	]*20: R_MIPS_GNU_REL16_S2	.text
0+0024 <[^>]*> 00000000 	nop
0+0028 <[^>]*> 0411fff6 	bal	0+0004 <g1\-0x8>
[ 	]*28: R_MIPS_GNU_REL16_S2	e1
0+002c <[^>]*> 00000000 	nop
0+0030 <[^>]*> 0411fff4 	bal	0+0004 <g1\-0x8>
[ 	]*30: R_MIPS_GNU_REL16_S2	e2
0+0034 <[^>]*> 00000000 	nop
	\.\.\.
