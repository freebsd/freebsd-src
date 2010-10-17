#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS R3000 jal-svr4pic
#as: -32 -mips1 -KPIC -mtune=r3000

# Test the jal macro with -KPIC.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lui	gp,0x0
[ 	]*0: R_MIPS_HI16	_gp_disp
0+0004 <[^>]*> addiu	gp,gp,0
[ 	]*4: R_MIPS_LO16	_gp_disp
0+0008 <[^>]*> addu	gp,gp,t9
0+000c <[^>]*> sw	gp,0\(sp\)
0+0010 <[^>]*> jalr	t9
0+0014 <[^>]*> nop
0+0018 <[^>]*> lw	gp,0\(sp\)
0+001c <[^>]*> jalr	a0,t9
0+0020 <[^>]*> nop
0+0024 <[^>]*> lw	gp,0\(sp\)
0+0028 <[^>]*> nop
0+002c <[^>]*> lw	t9,0\(gp\)
[ 	]*2c: R_MIPS_GOT16	.text
0+0030 <[^>]*> nop
0+0034 <[^>]*> addiu	t9,t9,0
[ 	]*34: R_MIPS_LO16	.text
0+0038 <[^>]*> jalr	t9
0+003c <[^>]*> nop
0+0040 <[^>]*> lw	gp,0\(sp\)
0+0044 <[^>]*> nop
0+0048 <[^>]*> lw	t9,0\(gp\)
[ 	]*48: R_MIPS_CALL16	weak_text_label
0+004c <[^>]*> nop
0+0050 <[^>]*> jalr	t9
0+0054 <[^>]*> nop
0+0058 <[^>]*> lw	gp,0\(sp\)
0+005c <[^>]*> nop
0+0060 <[^>]*> lw	t9,0\(gp\)
[ 	]*60: R_MIPS_CALL16	external_text_label
0+0064 <[^>]*> nop
0+0068 <[^>]*> jalr	t9
0+006c <[^>]*> nop
0+0070 <[^>]*> lw	gp,0\(sp\)
0+0074 <[^>]*> b	0+0000 <text_label>
0+0078 <[^>]*> nop
	...
