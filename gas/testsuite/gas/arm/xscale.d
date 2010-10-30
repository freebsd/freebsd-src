#objdump: -dr --prefix-addresses --show-raw-insn
#name: XScale instructions
#as: -mcpu=xscale -EL

# Test the XScale instructions:

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <foo> ee201010 	mia	acc0, r0, r1
0+04 <[^>]*> be20d01e 	mialt	acc0, lr, sp
0+08 <[^>]*> ee284012 	miaph	acc0, r2, r4
0+0c <[^>]*> 1e286015 	miaphne	acc0, r5, r6
0+10 <[^>]*> ee2c8017 	miaBB	acc0, r7, r8
0+14 <[^>]*> ee2da019 	miaBT	acc0, r9, sl
0+18 <[^>]*> ee2eb01c 	miaTB	acc0, ip, fp
0+1c <[^>]*> ee2f0010 	miaTT	acc0, r0, r0
0+20 <[^>]*> ec411000 	mar	acc0, r1, r1
0+24 <[^>]*> cc4c2000 	margt	acc0, r2, ip
0+28 <[^>]*> ec543000 	mra	r3, r4, acc0
0+2c <[^>]*> ec585000 	mra	r5, r8, acc0
0+30 <[^>]*> f5d0f000 	pld	\[r0\]
0+34 <[^>]*> f5d1f789 	pld	\[r1, #1929\]
0+38 <[^>]*> f7d2f003 	pld	\[r2, r3\]
0+3c <[^>]*> f754f285 	pld	\[r4, -r5, lsl #5\]
0+40 <[^>]*> e1c100d0 	ldrd	r0, \[r1\]
0+44 <[^>]*> 01c327d8 	ldrdeq	r2, \[r3, #120\]
0+48 <[^>]*> b10540d6 	ldrdlt	r4, \[r5, -r6\]
0+4c <[^>]*> e16a88f9 	strd	r8, \[sl, #-137\]!
0+50 <[^>]*> e1ac00fd 	strd	r0, \[ip, sp\]!
0+54 <[^>]*> 30ce21f0 	strdcc	r2, \[lr\], #16
0+58 <[^>]*> 708640f8 	strdvc	r4, \[r6\], r8
0+5c <[^>]*> e5910000 	ldr	r0, \[r1\]
0+60 <[^>]*> e5832000 	str	r2, \[r3\]
0+64 <[^>]*> e321f011 	msr	CPSR_c, #17	; 0x11
0+68 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+6c <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
