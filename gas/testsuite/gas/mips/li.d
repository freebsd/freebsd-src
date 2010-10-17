#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS li
#as: -32

# Test the li macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> li	a0,0
0+0004 <[^>]*> li	a0,1
0+0008 <[^>]*> li	a0,0x8000
0+000c <[^>]*> li	a0,-32768
0+0010 <[^>]*> lui	a0,0x1
0+0014 <[^>]*> lui	a0,0x1
0+0018 <[^>]*> ori	a0,a0,0xa5a5
0+001c <[^>]*> nop
