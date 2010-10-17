#objdump: -dr --prefix-addresses --show-raw-insn --architecture=armv5te
#name: ARM CoProcessor Instructions (WinCE version)
#as: -march=armv5te -EL
#source: copro.s

# This file is the same as copro.d except that the PC-relative
# LDC and STFS instructions have not had a -8 bias inserted.

# Test the standard ARM co-processor instructions:

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> ee421103 	dvfs	f1, f2, f3
0+004 <[^>]*> 0e3414a5 	cfadddeq	mvd1, mvd4, mvd5
0+008 <[^>]*> ed939500 	cfldr32	mvfx9, \[r3\]
0+00c <[^>]*> edd1e108 	ldfp	f6, \[r1, #32\]
0+010 <[^>]*> 4db200ff 	ldcmi	0, cr0, \[r2, #1020\]!
0+014 <[^>]*> 5cf31710 	ldcpll	7, cr1, \[r3\], #64
0+018 <[^>]*> ed1f8003 	ldc	0, cr8, \[pc, #-12\]
0+01c <[^>]*> ed830500 	cfstr32	mvfx0, \[r3\]
0+020 <[^>]*> edc0f302 	stcl	3, cr15, \[r0, #8\]
0+024 <[^>]*> 0da2c419 	cfstrseq	mvf12, \[r2, #100\]!
0+028 <[^>]*> 3ca4860c 	stccc	6, cr8, \[r4\], #48
0+02c <[^>]*> ed0f7103 	stfs	f7, \[pc, #-12\]
0+030 <[^>]*> ee715212 	mrc	2, 3, r5, cr1, cr2, \{0\}
0+034 <[^>]*> aeb1f4f2 	mrcge	4, 5, pc, cr1, cr2, \{7\}
0+038 <[^>]*> ee21f711 	mcr	7, 1, pc, cr1, cr1, \{0\}
0+03c <[^>]*> be228519 	cfsh64lt	mvdx8, mvdx2, #9
0+040 <[^>]*> ec907300 	ldc	3, cr7, \[r0\], \{0\}
0+044 <[^>]*> ec816e01 	stc	14, cr6, \[r1\], \{1\}
0+048 <[^>]*> fc925502 	ldc2	5, cr5, \[r2\], \{2\}
0+04c <[^>]*> fc834603 	stc2	6, cr4, \[r3\], \{3\}
0+050 <[^>]*> ecd43704 	ldcl	7, cr3, \[r4\], \{4\}
0+054 <[^>]*> ecc52805 	stcl	8, cr2, \[r5\], \{5\}
0+058 <[^>]*> fcd61906 	ldc2l	9, cr1, \[r6\], \{6\}
0+05c <[^>]*> fcc70a07 	stc2l	10, cr0, \[r7\], \{7\}
0+060 <[^>]*> ecd88bff 	ldcl	11, cr8, \[r8\], \{255\}
0+064 <[^>]*> ecc99cfe 	stcl	12, cr9, \[r9\], \{254\}
0+068 <[^>]*> ec507d04 	mrrc	13, 0, r7, r0, cr4
0+06c <[^>]*> ec407e05 	mcrr	14, 0, r7, r0, cr5
0+070 <[^>]*> ec507fff 	mrrc	15, 15, r7, r0, cr15
0+074 <[^>]*> ec407efe 	mcrr	14, 15, r7, r0, cr14
0+078 <[^>]*> e1a00000 	nop			\(mov r0,r0\)
0+07c <[^>]*> e1a00000 	nop			\(mov r0,r0\)
