#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS lifloat-xgot
#as: -32 -mips1 -mtune=r3000 -KPIC -xgot -EB --defsym XGOT=1
#source: lifloat.s

# Test the li.d and li.s macros with -KPIC -xgot.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw	at,0\(gp\)
[ 	]*0: R_MIPS_GOT16	.rodata
0+0004 <[^>]*> nop
0+0008 <[^>]*> lw	a0,0\(at\)
[ 	]*8: R_MIPS_LO16	.rodata
0+000c <[^>]*> lw	a1,4\(at\)
[ 	]*c: R_MIPS_LO16	.rodata
0+0010 <[^>]*> lw	at,0\(gp\)
[ 	]*10: R_MIPS_GOT16	.rodata
0+0014 <[^>]*> nop
0+0018 <[^>]*> lwc1	\$f5,8\(at\)
[ 	]*18: R_MIPS_LO16	.rodata
0+001c <[^>]*> lwc1	\$f4,12\(at\)
[ 	]*1c: R_MIPS_LO16	.rodata
0+0020 <[^>]*> lui	a0,0x3f8f
0+0024 <[^>]*> ori	a0,a0,0xcd36
0+0028 <[^>]*> lui	at,0x3f8f
0+002c <[^>]*> ori	at,at,0xcd36
0+0030 <[^>]*> mtc1	at,\$f4
	...
