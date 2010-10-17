#objdump: -dr --prefix-addresses --show-raw-insn
#name: sync instructions
#as: -32 -mips2

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <foo> 0000000f[ 	]*sync
0+0004 <foo\+0x4> 0000040f[ 	]*sync.p
0+0008 <foo\+0x8> 0000000f[ 	]*sync
	...
