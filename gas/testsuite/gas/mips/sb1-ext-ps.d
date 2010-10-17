#objdump: -dr --prefix-addresses --show-raw-insn -mmips:sb1
#name: SB-1 paired single extensions
#as: -march=sb1

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 46c31043 	div.ps	\$f1,\$f2,\$f3
0+0004 <[^>]*> 46c01055 	recip.ps	\$f1,\$f2
0+0008 <[^>]*> 46c01056 	rsqrt.ps	\$f1,\$f2
0+000c <[^>]*> 46c01044 	sqrt.ps	\$f1,\$f2
	...
