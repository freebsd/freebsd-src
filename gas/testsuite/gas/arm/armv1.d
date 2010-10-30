#objdump: -dr --prefix-addresses --show-raw-insn
#name: ARM v1 instructions
#as: -mcpu=arm7t
#error-output: armv1.l

# Test the ARM v1 instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]*> e0000000 ?	and	r0, r0, r0
0+04 <[^>]*> e0100000 ?	ands	r0, r0, r0
0+08 <[^>]*> e0200000 ?	eor	r0, r0, r0
0+0c <[^>]*> e0300000 ?	eors	r0, r0, r0
0+10 <[^>]*> e0400000 ?	sub	r0, r0, r0
0+14 <[^>]*> e0500000 ?	subs	r0, r0, r0
0+18 <[^>]*> e0600000 ?	rsb	r0, r0, r0
0+1c <[^>]*> e0700000 ?	rsbs	r0, r0, r0
0+20 <[^>]*> e0800000 ?	add	r0, r0, r0
0+24 <[^>]*> e0900000 ?	adds	r0, r0, r0
0+28 <[^>]*> e0a00000 ?	adc	r0, r0, r0
0+2c <[^>]*> e0b00000 ?	adcs	r0, r0, r0
0+30 <[^>]*> e0c00000 ?	sbc	r0, r0, r0
0+34 <[^>]*> e0d00000 ?	sbcs	r0, r0, r0
0+38 <[^>]*> e0e00000 ?	rsc	r0, r0, r0
0+3c <[^>]*> e0f00000 ?	rscs	r0, r0, r0
0+40 <[^>]*> e1800000 ?	orr	r0, r0, r0
0+44 <[^>]*> e1900000 ?	orrs	r0, r0, r0
0+48 <[^>]*> e1c00000 ?	bic	r0, r0, r0
0+4c <[^>]*> e1d00000 ?	bics	r0, r0, r0
0+50 <[^>]*> e1100000 ?	tst	r0, r0
0+54 <[^>]*> e1100000 ?	tst	r0, r0
0+58 <[^>]*> e110f000 ?	tstp	r0, r0
0+5c <[^>]*> e1300000 ?	teq	r0, r0
0+60 <[^>]*> e1300000 ?	teq	r0, r0
0+64 <[^>]*> e130f000 ?	teqp	r0, r0
0+68 <[^>]*> e1500000 ?	cmp	r0, r0
0+6c <[^>]*> e1500000 ?	cmp	r0, r0
0+70 <[^>]*> e150f000 ?	cmpp	r0, r0
0+74 <[^>]*> e1700000 ?	cmn	r0, r0
0+78 <[^>]*> e1700000 ?	cmn	r0, r0
0+7c <[^>]*> e170f000 ?	cmnp	r0, r0
0+80 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+84 <[^>]*> e1b00000 ?	movs	r0, r0
0+88 <[^>]*> e1e00000 ?	mvn	r0, r0
0+8c <[^>]*> e1f00000 ?	mvns	r0, r0
0+90 <[^>]*> ef000000 ?	(swi|svc)	0x00000000
0+94 <[^>]*> e5900000 ?	ldr	r0, \[r0\]
0+98 <[^>]*> e5d00000 ?	ldrb	r0, \[r0\]
0+9c <[^>]*> e4b10000 ?	ldrt	r0, \[r1\]
0+a0 <[^>]*> e4f10000 ?	ldrbt	r0, \[r1\]
0+a4 <[^>]*> e5800000 ?	str	r0, \[r0\]
0+a8 <[^>]*> e5c00000 ?	strb	r0, \[r0\]
0+ac <[^>]*> e4a10000 ?	strt	r0, \[r1\]
0+b0 <[^>]*> e4e10000 ?	strbt	r0, \[r1\]
0+b4 <[^>]*> e8800001 ?	stm	r0, {r0}
0+b8 <[^>]*> e9800001 ?	stmib	r0, {r0}
0+bc <[^>]*> e8000001 ?	stmda	r0, {r0}
0+c0 <[^>]*> e9000001 ?	stmdb	r0, {r0}
0+c4 <[^>]*> e9000001 ?	stmdb	r0, {r0}
0+c8 <[^>]*> e9800001 ?	stmib	r0, {r0}
0+cc <[^>]*> e8800001 ?	stm	r0, {r0}
0+d0 <[^>]*> e8000001 ?	stmda	r0, {r0}
0+d4 <[^>]*> e8900001 ?	ldm	r0, {r0}
0+d8 <[^>]*> e9900001 ?	ldmib	r0, {r0}
0+dc <[^>]*> e8100001 ?	ldmda	r0, {r0}
0+e0 <[^>]*> e9100001 ?	ldmdb	r0, {r0}
0+e4 <[^>]*> e8900001 ?	ldm	r0, {r0}
0+e8 <[^>]*> e8100001 ?	ldmda	r0, {r0}
0+ec <[^>]*> e9100001 ?	ldmdb	r0, {r0}
0+f0 <[^>]*> e9900001 ?	ldmib	r0, {r0}
0+f4 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+f8 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+fc <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
