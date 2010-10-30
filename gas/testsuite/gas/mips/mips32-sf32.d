#objdump: -dr --prefix-addresses --show-raw-insn -M reg-names=numeric
#name: MIPS32 odd single-precision float registers
#as: -32 

# Check MIPS32 instruction assembly

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 3c013f80 	lui	\$1,0x3f80
0+0004 <[^>]*> 44810800 	mtc1	\$1,\$f1
0+0008 <[^>]*> c783c000 	lwc1	\$f3,-16384\(\$28\)
			8: R_MIPS_LITERAL	\.lit4\+0x4000
0+000c <[^>]*> 46030940 	add.s	\$f5,\$f1,\$f3
0+0010 <[^>]*> 46003a21 	cvt.d.s	\$f8,\$f7
0+0014 <[^>]*> 46803a21 	cvt.d.w	\$f8,\$f7
0+0018 <[^>]*> 462041e0 	cvt.s.d	\$f7,\$f8
0+001c <[^>]*> 462041cd 	trunc.w.d	\$f7,\$f8
	\.\.\.
