# name: Thumb-1 unified
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> 200c      	movs	r0, #12
0[0-9a-f]+ <[^>]+> 1cd1      	adds	r1, r2, #3
0[0-9a-f]+ <[^>]+> 1ed1      	subs	r1, r2, #3
0[0-9a-f]+ <[^>]+> 3364      	adds	r3, #100
0[0-9a-f]+ <[^>]+> 3c83      	subs	r4, #131
0[0-9a-f]+ <[^>]+> 2d27      	cmp	r5, #39
0[0-9a-f]+ <[^>]+> a103      	add	r1, pc, #12	\(adr [^)]*\)
0[0-9a-f]+ <[^>]+> 4a03      	ldr	r2, \[pc, #12\]	\([^)]*\)
0[0-9a-f]+ <[^>]+> 6863      	ldr	r3, \[r4, #4\]
0[0-9a-f]+ <[^>]+> 9d01      	ldr	r5, \[sp, #4\]
0[0-9a-f]+ <[^>]+> b001      	add	sp, #4
0[0-9a-f]+ <[^>]+> b081      	sub	sp, #4
0[0-9a-f]+ <[^>]+> af01      	add	r7, sp, #4
0[0-9a-f]+ <[^>]+> 4251      	negs	r1, r2
