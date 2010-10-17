#objdump: -dr --prefix-addresses -mmips:4100
#name: MIPS 4100
#as: -march=4100


.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <stuff> hibernate
0+0004 <stuff\+0x4> standby
0+0008 <stuff\+0x8> suspend
0+000c <stuff\+0xc> nop
