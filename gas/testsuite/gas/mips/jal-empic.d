#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS jal-empic
#as: -mips1 -membedded-pic
#source: jal.s

# Test the jal macro with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> jalr	t9
0+0004 <[^>]*> nop
0+0008 <[^>]*> jalr	a0,t9
0+000c <[^>]*> nop
0+0010 <[^>]*> bal	0+0000 <text_label>
[ 	]*10: PCREL16	.text
0+0014 <[^>]*> nop
0+0018 <[^>]*> bal	0+0018 <text_label\+(0x|)18>
[ 	]*18: PCREL16	external_text_label
0+001c <[^>]*> nop
0+0020 <[^>]*> b	0+0000 <text_label>
[ 	]*20: PCREL16	.text
0+0024 <[^>]*> nop
0+0028 <[^>]*> b	0+0028 <text_label\+(0x|)28>
[ 	]*28: PCREL16	external_text_label
0+002c <[^>]*> nop
