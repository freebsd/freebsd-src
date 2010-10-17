#objdump: -dr --prefix-addresses -mmips:5000
#name: MIPS nodelay
#as: -mips4 -mtune=r8000
#source: delay.s

# For -mips4 
# Gas should *not* produce nop's after mtc1 and related 
# insn's if the target fpr is used in the immediatly 
# following insn.  See also delay.d.
#

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> mtc1	zero,\$f0
0+0004 <[^>]*> cvt.d.w	\$f0,\$f0
0+0008 <[^>]*> mtc1	zero,\$f2
0+000c <[^>]*> cvt.d.w	\$f2,\$f2
	...
