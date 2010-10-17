#objdump: -dr --prefix-addresses --show-raw-insn
#name: ARM Architecture v5TEJ instructions
#as: -march=armv5tej

# Test the ARM Architecture v5TEJ instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]*> e12fff20 ?	bxj	r0
0+04 <[^>]*> e12fff21 ?	bxj	r1
0+08 <[^>]*> e12fff2e ?	bxj	lr
0+0c <[^>]*> 012fff20 ?	bxjeq	r0
0+10 <[^>]*> 412fff20 ?	bxjmi	r0
0+14 <[^>]*> 512fff27 ?	bxjpl	r7
0+18 <[^>]*> e1200070 ?	bkpt	0x0000
0+1c <[^>]*> e120007a ?	bkpt	0x000a
