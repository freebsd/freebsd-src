#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS beq
#as: -32

# Test the beq macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> beq	a0,a1,0+0000 <text_label>
0+0004 <[^>]*> nop
0+0008 <[^>]*> beqz	a0,0+0000 <text_label>
0+000c <[^>]*> nop
0+0010 <[^>]*> li	at,1
0+0014 <[^>]*> beq	a0,at,0+0000 <text_label>
0+0018 <[^>]*> nop
0+001c <[^>]*> li	at,0x8000
0+0020 <[^>]*> beq	a0,at,0+0000 <text_label>
0+0024 <[^>]*> nop
0+0028 <[^>]*> li	at,-32768
0+002c <[^>]*> beq	a0,at,0+0000 <text_label>
0+0030 <[^>]*> nop
0+0034 <[^>]*> lui	at,0x1
0+0038 <[^>]*> beq	a0,at,0+0000 <text_label>
0+003c <[^>]*> nop
0+0040 <[^>]*> lui	at,0x1
0+0044 <[^>]*> ori	at,at,0xa5a5
0+0048 <[^>]*> beq	a0,at,0+0000 <text_label>
0+004c <[^>]*> nop
0+0050 <[^>]*> bnez	a0,0+0000 <text_label>
0+0054 <[^>]*> nop
0+0058 <[^>]*> beqzl	a0,0+0000 <text_label>
0+005c <[^>]*> nop
0+0060 <[^>]*> bnezl	a0,0+0000 <text_label>
0+0064 <[^>]*> nop
	...
0+20068 <[^>]*> j	0+0000 <text_label>
[ 	]*20068: (MIPS_JMP|JMPADDR|R_MIPS_26)	.text
0+2006c <[^>]*> nop
0+20070 <[^>]*> jal	0+0000 <text_label>
[ 	]*20070: (MIPS_JMP|JMPADDR|R_MIPS_26)	.text
0+20074 <[^>]*> nop
#0+20078 <[^>]*> b	0+20078 <text_label\+0x20078>
#[ 	]*20078: R_MIPS_PC16	external_label
#0+2007c <[^>]*> nop
#0+20080 <[^>]*> bal	0+20080 <text_label\+0x20080>
#[ 	]*20080: R_MIPS_PC16	external_label
#0+20084 <[^>]*> nop
	...
