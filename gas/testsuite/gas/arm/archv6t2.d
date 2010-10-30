#name: ARM V6T2 instructions
#as: -march=armv6t2
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]+> e7c00010 	bfi	r0, r0, #0, #1
0+04 <[^>]+> 17c00010 	bfine	r0, r0, #0, #1
0+08 <[^>]+> e7c09010 	bfi	r9, r0, #0, #1
0+0c <[^>]+> e7c00019 	bfi	r0, r9, #0, #1
0+10 <[^>]+> e7d10010 	bfi	r0, r0, #0, #18
0+14 <[^>]+> e7d10890 	bfi	r0, r0, #17, #1
0+18 <[^>]+> e7c0001f 	bfc	r0, #0, #1
0+1c <[^>]+> e7c0001f 	bfc	r0, #0, #1
0+20 <[^>]+> 17c0001f 	bfcne	r0, #0, #1
0+24 <[^>]+> e7c0901f 	bfc	r9, #0, #1
0+28 <[^>]+> e7d1001f 	bfc	r0, #0, #18
0+2c <[^>]+> e7d1089f 	bfc	r0, #17, #1
0+30 <[^>]+> e7a00050 	sbfx	r0, r0, #0, #1
0+34 <[^>]+> 17a00050 	sbfxne	r0, r0, #0, #1
0+38 <[^>]+> e7e00050 	ubfx	r0, r0, #0, #1
0+3c <[^>]+> e7a09050 	sbfx	r9, r0, #0, #1
0+40 <[^>]+> e7a00059 	sbfx	r0, r9, #0, #1
0+44 <[^>]+> e7a008d0 	sbfx	r0, r0, #17, #1
0+48 <[^>]+> e7b10050 	sbfx	r0, r0, #0, #18
0+4c <[^>]+> e6ff0f30 	rbit	r0, r0
0+50 <[^>]+> 16ff0f30 	rbitne	r0, r0
0+54 <[^>]+> e6ff9f30 	rbit	r9, r0
0+58 <[^>]+> e6ff0f39 	rbit	r0, r9
0+5c <[^>]+> e0600090 	mls	r0, r0, r0, r0
0+60 <[^>]+> 10600090 	mlsne	r0, r0, r0, r0
0+64 <[^>]+> e0690090 	mls	r9, r0, r0, r0
0+68 <[^>]+> e0600099 	mls	r0, r9, r0, r0
0+6c <[^>]+> e0600990 	mls	r0, r0, r9, r0
0+70 <[^>]+> e0609090 	mls	r0, r0, r0, r9
0+74 <[^>]+> e3000000 	movw	r0, #0	; 0x0
0+78 <[^>]+> e3400000 	movt	r0, #0	; 0x0
0+7c <[^>]+> 13000000 	movwne	r0, #0	; 0x0
0+80 <[^>]+> e3009000 	movw	r9, #0	; 0x0
0+84 <[^>]+> e3000999 	movw	r0, #2457	; 0x999
0+88 <[^>]+> e3090000 	movw	r0, #36864	; 0x9000
0+8c <[^>]+> e0f900b0 	ldrht	r0, \[r9\]
0+90 <[^>]+> e0f900f0 	ldrsht	r0, \[r9\]
0+94 <[^>]+> e0f900d0 	ldrsbt	r0, \[r9\]
0+98 <[^>]+> e0e900b0 	strht	r0, \[r9\]
0+9c <[^>]+> 10f900b0 	ldrhtne	r0, \[r9\]
0+a0 <[^>]+> e0b090b9 	ldrht	r9, \[r0\], r9
0+a4 <[^>]+> e03090b9 	ldrht	r9, \[r0\], -r9
0+a8 <[^>]+> e0f099b9 	ldrht	r9, \[r0\], #153
0+ac <[^>]+> e07099b9 	ldrht	r9, \[r0\], #-153
