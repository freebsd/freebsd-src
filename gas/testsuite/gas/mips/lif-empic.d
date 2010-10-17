#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS lifloat-empic
#as: -32 -mips1 -membedded-pic --defsym EMPIC=1
#source: lifloat.s

# Test the li.d and li.s macros with -membedded-pic.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> addiu	at,gp,-16384
[ 	]*0: [A-Z0-9_]*GPREL[A-Z0-9_]*	.rdata.*
0+0004 <[^>]*> lw	a0,0\(at\)
0+0008 <[^>]*> lw	a1,4\(at\)
0+000c <[^>]*> lwc1	\$f[45],-16368\(gp\)
[ 	]*c: [A-Z0-9_]*LITERAL[A-Z0-9_]*	.lit8.*
0+0010 <[^>]*> lwc1	\$f[45],-16364\(gp\)
[ 	]*10: [A-Z0-9_]*LITERAL[A-Z0-9_]*	.lit8.*
0+0014 <[^>]*> lui	a0,0x3f8f
0+0018 <[^>]*> ori	a0,a0,0xcd36
0+001c <[^>]*> lui	at,0x3f8f
0+0020 <[^>]*> ori	at,at,0xcd36
0+0024 <[^>]*> mtc1	at,\$f4
	...
