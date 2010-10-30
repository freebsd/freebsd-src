# name: Thumb-2 LDM/STM single reg
# as: -march=armv6t2
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> bc01      	pop	{r0}
0[0-9a-f]+ <[^>]+> f85d 8b04 	ldr.w	r8, \[sp\], #4
0[0-9a-f]+ <[^>]+> f8d1 9000 	ldr.w	r9, \[r1\]
0[0-9a-f]+ <[^>]+> f852 cb04 	ldr.w	ip, \[r2\], #4
0[0-9a-f]+ <[^>]+> f85d 2d04 	ldr.w	r2, \[sp, #-4\]!
0[0-9a-f]+ <[^>]+> f85d 8d04 	ldr.w	r8, \[sp, #-4\]!
0[0-9a-f]+ <[^>]+> f856 4c04 	ldr.w	r4, \[r6, #-4\]
0[0-9a-f]+ <[^>]+> f856 8c04 	ldr.w	r8, \[r6, #-4\]
0[0-9a-f]+ <[^>]+> f852 4d04 	ldr.w	r4, \[r2, #-4\]!
0[0-9a-f]+ <[^>]+> f852 cd04 	ldr.w	ip, \[r2, #-4\]!
0[0-9a-f]+ <[^>]+> b408      	push	{r3}
0[0-9a-f]+ <[^>]+> f84d 9b04 	str.w	r9, \[sp\], #4
0[0-9a-f]+ <[^>]+> f8c3 c000 	str.w	ip, \[r3\]
0[0-9a-f]+ <[^>]+> f844 cb04 	str.w	ip, \[r4\], #4
0[0-9a-f]+ <[^>]+> f84d 3d04 	str.w	r3, \[sp, #-4\]!
0[0-9a-f]+ <[^>]+> f84d 9d04 	str.w	r9, \[sp, #-4\]!
0[0-9a-f]+ <[^>]+> f847 5c04 	str.w	r5, \[r7, #-4\]
0[0-9a-f]+ <[^>]+> f846 cc04 	str.w	ip, \[r6, #-4\]
0[0-9a-f]+ <[^>]+> f846 bd04 	str.w	fp, \[r6, #-4\]!
0[0-9a-f]+ <[^>]+> f845 8d04 	str.w	r8, \[r5, #-4\]!
