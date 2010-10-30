# name: Overlapping multiplication operands for ARMv6
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> e0000090 	mul	r0, r0, r0
0[0-9a-f]+ <[^>]+> e0202190 	mla	r0, r0, r1, r2
0[0-9a-f]+ <[^>]+> e0602190 	mls	r0, r0, r1, r2
0[0-9a-f]+ <[^>]+> e12fff1e 	bx	lr
