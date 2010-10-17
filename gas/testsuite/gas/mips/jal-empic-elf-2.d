#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS jal-empic-elf-2
#as: -32 -membedded-pic

# Test the jal macro harder with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
	\.\.\.
	\.\.\.
0+0018 <[^>]*> 04110002 	bal	0+0024 <g1\+0x18>
[ 	]*18: R_MIPS_GNU_REL16_S2	.text
0+001c <[^>]*> 00000000 	nop
0+0020 <[^>]*> 04110002 	bal	0+002c <g1\+0x20>
[ 	]*20: R_MIPS_GNU_REL16_S2	.text
0+0024 <[^>]*> 00000000 	nop
0+0028 <[^>]*> 0411ffff 	bal	0+0028 <g1\+0x1c>
[ 	]*28: R_MIPS_GNU_REL16_S2	e1
0+002c <[^>]*> 00000000 	nop
0+0030 <[^>]*> 10000002 	b	0+003c <g1\+0x30>
[ 	]*30: R_MIPS_GNU_REL16_S2	.text
0+0034 <[^>]*> 00000000 	nop
0+0038 <[^>]*> 10000002 	b	0+0044 <g1\+0x38>
[ 	]*38: R_MIPS_GNU_REL16_S2	.text
0+003c <[^>]*> 00000000 	nop
0+0040 <[^>]*> 1000ffff 	b	0+0040 <g1\+0x34>
[ 	]*40: R_MIPS_GNU_REL16_S2	e1
0+0044 <[^>]*> 00000000 	nop
0+0048 <[^>]*> 0411ffff 	bal	0+0048 <g1\+0x3c>
[ 	]*48: R_MIPS_GNU_REL16_S2	.text
0+004c <[^>]*> 00000000 	nop
0+0050 <[^>]*> 0411ffff 	bal	0+0050 <g1\+0x44>
[ 	]*50: R_MIPS_GNU_REL16_S2	.text
0+0054 <[^>]*> 00000000 	nop
0+0058 <[^>]*> 0411fffc 	bal	0+004c <g1\+0x40>
[ 	]*58: R_MIPS_GNU_REL16_S2	e1
0+005c <[^>]*> 00000000 	nop
0+0060 <[^>]*> 04110005 	bal	0+0078 <g1\+0x6c>
[ 	]*60: R_MIPS_GNU_REL16_S2	.text
0+0064 <[^>]*> 00000000 	nop
0+0068 <[^>]*> 04110005 	bal	0+0080 <g1\+0x74>
[ 	]*68: R_MIPS_GNU_REL16_S2	.text
0+006c <[^>]*> 00000000 	nop
0+0070 <[^>]*> 04110002 	bal	0+007c <g1\+0x70>
[ 	]*70: R_MIPS_GNU_REL16_S2	e1
0+0074 <[^>]*> 00000000 	nop
	\.\.\.
