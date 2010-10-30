# name: Conditional Neon instructions
# as: -mfpu=neon
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> 0d943b00 	vldreq	d3, \[r4\]
0[0-9a-f]+ <[^>]+> be035b70 	vmovlt\.16	d3\[1\], r5
0[0-9a-f]+ <[^>]+> ac474b13 	vmovge	d3, r4, r7
0[0-9a-f]+ <[^>]+> 3c543b3e 	vmovcc	r3, r4, d30
0[0-9a-f]+ <[^>]+> 1e223b10 	vmovne\.32	d2\[1\], r3
0[0-9a-f]+ <[^>]+> 2c521b13 	vmovcs	r1, r2, d3
0[0-9a-f]+ <[^>]+> 3c421b14 	vmovcc	d4, r1, r2
