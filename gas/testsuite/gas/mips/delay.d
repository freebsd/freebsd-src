#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS delay
#as: -mips3 -mtune=r4000

# 
# Gas should produce nop's after mtc1 and related 
# insn's if the target fpr is used in the 
# immediatly following insn.  See also nodelay.d.
#

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> mtc1	zero,\$f0
0+0004 <[^>]*> nop
0+0008 <[^>]*> cvt.d.w	\$f0,\$f0
0+000c <[^>]*> mtc1	zero,\$f2
0+0010 <[^>]*> nop
0+0014 <[^>]*> cvt.d.w	\$f2,\$f2
	...
