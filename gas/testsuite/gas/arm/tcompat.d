#name: ARM Thumb-compat pseudos
#objdump: -dr --prefix-addresses --show-raw-insn
#as:

# Test the ARM pseudo instructions that exist for Thumb source compatibility

.*: +file format .*arm.*

Disassembly of section .text:

0+00 <[^>]*> 91a00000 ?	movls	r0, r0
0+04 <[^>]*> e1a09000 ?	mov	r9, r0
0+08 <[^>]*> e1a00009 ?	mov	r0, r9
0+0c <[^>]*> e1a0c00e ?	mov	ip, lr
0+10 <[^>]*> 91b09019 ?	movlss	r9, r9, lsl r0
0+14 <[^>]*> 91a00910 ?	movls	r0, r0, lsl r9
0+18 <[^>]*> e1b00880 ?	movs	r0, r0, lsl #17
0+1c <[^>]*> e1a00889 ?	mov	r0, r9, lsl #17
0+20 <[^>]*> 91b09039 ?	movlss	r9, r9, lsr r0
0+24 <[^>]*> 91a00930 ?	movls	r0, r0, lsr r9
0+28 <[^>]*> e1b008a0 ?	movs	r0, r0, lsr #17
0+2c <[^>]*> e1a008a9 ?	mov	r0, r9, lsr #17
0+30 <[^>]*> 91b09059 ?	movlss	r9, r9, asr r0
0+34 <[^>]*> 91a00950 ?	movls	r0, r0, asr r9
0+38 <[^>]*> e1b008c0 ?	movs	r0, r0, asr #17
0+3c <[^>]*> e1a008c9 ?	mov	r0, r9, asr #17
0+40 <[^>]*> 91b09079 ?	movlss	r9, r9, ror r0
0+44 <[^>]*> 91a00970 ?	movls	r0, r0, ror r9
0+48 <[^>]*> e1b008e0 ?	movs	r0, r0, ror #17
0+4c <[^>]*> e1a008e9 ?	mov	r0, r9, ror #17
0+50 <[^>]*> e2690000 ?	rsb	r0, r9, #0	; 0x0
0+54 <[^>]*> e2709000 ?	rsbs	r9, r0, #0	; 0x0
0+58 <[^>]*> 92600000 ?	rsbls	r0, r0, #0	; 0x0
0+5c <[^>]*> 92799000 ?	rsblss	r9, r9, #0	; 0x0
0+60 <[^>]*> e92d000e ?	stmdb	sp!, {r1, r2, r3}
0+64 <[^>]*> 992d8154 ?	stmlsdb	sp!, {r2, r4, r6, r8, pc}
0+68 <[^>]*> e8bd000e ?	ldmia	sp!, {r1, r2, r3}
0+6c <[^>]*> 98bd8154 ?	ldmlsia	sp!, {r2, r4, r6, r8, pc}
0+70 <[^>]*> e0000001 ?	and	r0, r0, r1
0+74 <[^>]*> e0200001 ?	eor	r0, r0, r1
0+78 <[^>]*> e0400001 ?	sub	r0, r0, r1
0+7c <[^>]*> e0600001 ?	rsb	r0, r0, r1
0+80 <[^>]*> e0800001 ?	add	r0, r0, r1
0+84 <[^>]*> e0a00001 ?	adc	r0, r0, r1
0+88 <[^>]*> e0c00001 ?	sbc	r0, r0, r1
0+8c <[^>]*> e0e00001 ?	rsc	r0, r0, r1
0+90 <[^>]*> e1800001 ?	orr	r0, r0, r1
0+94 <[^>]*> e1c00001 ?	bic	r0, r0, r1
0+98 <[^>]*> e0000091 ?	mul	r0, r1, r0
0+9c <[^>]*> e1a00000 ?	nop			\(mov r0,r0\)
