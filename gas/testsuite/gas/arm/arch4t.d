# name: ARM architecture 4t instructions
# as: -march=armv4t
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]+> e12fff10 ?	bx	r0
0+04 <[^>]+> 012fff11 ?	bxeq	r1
0+08 <[^>]+> e15f30b8 ?	ldrh	r3, \[pc, #-8\]	; 0+08 <[^>]+>
0+0c <[^>]+> e1d540f0 ?	ldrsh	r4, \[r5\]
0+10 <[^>]+> e19140d3 ?	ldrsb	r4, \[r1, r3\]
0+14 <[^>]+> e1b410f4 ?	ldrsh	r1, \[r4, r4\]!
0+18 <[^>]+> 011510d3 ?	ldreqsb	r1, \[r5, -r3\]
0+1c <[^>]+> 109620b7 ?	ldrneh	r2, \[r6\], r7
0+20 <[^>]+> 309720f8 ?	ldrccsh	r2, \[r7\], r8
0+24 <[^>]+> e1d32fdf ?	ldrsb	r2, \[r3, #255\]
0+28 <[^>]+> e1541ffa ?	ldrsh	r1, \[r4, #-250\]
0+2c <[^>]+> e1d51fd0 ?	ldrsb	r1, \[r5, #240\]
0+30 <[^>]+> e1cf23b0 ?	strh	r2, \[pc, #48\]	; 0+68 <[^>]+>
0+34 <[^>]+> 11c330b0 ?	strneh	r3, \[r3\]
0+38 <[^>]+> e328f002 ?	msr	CPSR_f, #2	; 0x2
0+3c <[^>]+> e121f003 ?	msr	CPSR_c, r3
0+40 <[^>]+> e122f004 ?	msr	CPSR_x, r4
0+44 <[^>]+> e124f005 ?	msr	CPSR_s, r5
0+48 <[^>]+> e128f006 ?	msr	CPSR_f, r6
0+4c <[^>]+> e129f007 ?	msr	CPSR_fc, r7
0+50 <[^>]+> e368f004 ?	msr	SPSR_f, #4	; 0x4
0+54 <[^>]+> e161f008 ?	msr	SPSR_c, r8
0+58 <[^>]+> e162f009 ?	msr	SPSR_x, r9
0+5c <[^>]+> e164f00a ?	msr	SPSR_s, sl
0+60 <[^>]+> e168f00b ?	msr	SPSR_f, fp
0+64 <[^>]+> e169f00c ?	msr	SPSR_fc, ip
0+68 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+6c <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)

