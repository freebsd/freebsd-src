#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS jal-empic-elf
#as: -32 -membedded-pic
#source: jal.s

# Test the jal macro with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 0320f809 	jalr	t9
0+0004 <[^>]*> 00000000 	nop
0+0008 <[^>]*> 03202009 	jalr	a0,t9
0+000c <[^>]*> 00000000 	nop
0+0010 <[^>]*> 0411ffff 	bal	0+0010 <text_label\+0x10>
[ 	]*10: R_MIPS_GNU_REL16_S2	.text
0+0014 <[^>]*> 00000000 	nop
0+0018 <[^>]*> 0411ffff 	bal	0+0018 <text_label\+0x18>
[ 	]*18: R_MIPS_GNU_REL16_S2	external_text_label
0+001c <[^>]*> 00000000 	nop
0+0020 <[^>]*> 1000ffff 	b	0+0020 <text_label\+0x20>
[ 	]*20: R_MIPS_GNU_REL16_S2	.text
0+0024 <[^>]*> 00000000 	nop
0+0028 <[^>]*> 1000ffff 	b	0+0028 <text_label\+0x28>
[ 	]*28: R_MIPS_GNU_REL16_S2	external_text_label
0+002c <[^>]*> 00000000 	nop
