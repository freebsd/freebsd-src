#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS bgeu
#as: -32

# Test the bgeu macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> sltu	at,a0,a1
0+0004 <[^>]*> beqz	at,0+0000 <text_label>
0+0008 <[^>]*> nop
0+000c <[^>]*> beq	zero,a1,0+0000 <text_label>
0+0010 <[^>]*> nop
0+0014 <[^>]*> bnez	a0,0+0000 <text_label>
0+0018 <[^>]*> nop
0+001c <[^>]*> sltiu	at,a0,2
0+0020 <[^>]*> beqz	at,0+0000 <text_label>
0+0024 <[^>]*> nop
0+0028 <[^>]*> li	at,0x8000
0+002c <[^>]*> sltu	at,a0,at
0+0030 <[^>]*> beqz	at,0+0000 <text_label>
0+0034 <[^>]*> nop
0+0038 <[^>]*> sltiu	at,a0,-32768
0+003c <[^>]*> beqz	at,0+0000 <text_label>
0+0040 <[^>]*> nop
0+0044 <[^>]*> lui	at,0x1
0+0048 <[^>]*> sltu	at,a0,at
0+004c <[^>]*> beqz	at,0+0000 <text_label>
0+0050 <[^>]*> nop
0+0054 <[^>]*> lui	at,0x1
0+0058 <[^>]*> ori	at,at,0xa5a5
0+005c <[^>]*> sltu	at,a0,at
0+0060 <[^>]*> beqz	at,0+0000 <text_label>
0+0064 <[^>]*> nop
0+0068 <[^>]*> sltu	at,a1,a0
0+006c <[^>]*> bnez	at,0+0000 <text_label>
0+0070 <[^>]*> nop
0+0074 <[^>]*> bnez	a0,0+0000 <text_label>
0+0078 <[^>]*> nop
0+007c <[^>]*> bnez	a0,0+0000 <text_label>
0+0080 <[^>]*> nop
0+0084 <[^>]*> sltu	at,a0,a1
0+0088 <[^>]*> beqzl	at,0+0000 <text_label>
0+008c <[^>]*> nop
0+0090 <[^>]*> sltu	at,a1,a0
0+0094 <[^>]*> bnezl	at,0+0000 <text_label>
0+0098 <[^>]*> nop
#0+009c <[^>]*> sltu	at,a0,a1
#0+00a0 <[^>]*> beqz	at,000000a0 <text_label\+0xa0>
#[ 	]*a0: R_MIPS_PC16	external_label
#0+00a4 <[^>]*> nop
#0+00a8 <[^>]*> sltu	at,a1,a0
#0+00ac <[^>]*> bnez	at,000000ac <text_label\+0xac>
#[ 	]*ac: R_MIPS_PC16	external_label
#0+00b0 <[^>]*> nop
#0+00b4 <[^>]*> sltu	at,a0,a1
#0+00b8 <[^>]*> beqzl	at,000000b8 <text_label\+0xb8>
#[ 	]*b8: R_MIPS_PC16	external_label
#0+00bc <[^>]*> nop
#0+00c0 <[^>]*> sltu	at,a1,a0
#0+00c4 <[^>]*> bnezl	at,000000c4 <text_label\+0xc4>
#[ 	]*c4: R_MIPS_PC16	external_label
#0+00c8 <[^>]*> nop
	...
