#objdump: -dr --prefix-addresses --show-raw-insn -M reg-names=numeric -mmips:16
#as: -march=mips64
#name: MIPS16e-64
#source: mips16e-64.s

# Test the 64bit instructions of mips16e.

.*: +file format .*mips.*

Disassembly of section .text:

0x00000000 ecd1      	sew	\$4
0x00000002 ec51      	zew	\$4
0x00000004 6500      	nop
0x00000006 6500      	nop
0x00000008 6500      	nop
0x0000000a 6500      	nop
0x0000000c 6500      	nop
0x0000000e 6500      	nop
