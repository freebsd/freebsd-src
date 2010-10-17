#objdump: -Dr --prefix-addresses --show-raw-insn
#name: ARM arm7t (WinCE version)
#as: -mcpu=arm7t -EL
#source: arm7t.s

# This file is the same as arm7t.d except that the PC-relative
# LDR[S]H instructions have not had a -8 bias inserted.


# Test the halfword and signextend memory transfers:

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]*> e1d100b0 ?	ldrh	r0, \[r1\]
0+04 <[^>]*> e1f100b0 ?	ldrh	r0, \[r1\]!
0+08 <[^>]*> e19100b2 ?	ldrh	r0, \[r1, r2\]
0+0c <[^>]*> e1b100b2 ?	ldrh	r0, \[r1, r2\]!
0+10 <[^>]*> e1d100bc ?	ldrh	r0, \[r1, #12\]
0+14 <[^>]*> e1f100bc ?	ldrh	r0, \[r1, #12\]!
0+18 <[^>]*> e15100bc ?	ldrh	r0, \[r1, #-12\]
0+1c <[^>]*> e09100b2 ?	ldrh	r0, \[r1\], r2
0+20 <[^>]*> e3a00cff ?	mov	r0, #65280	; 0xff00
0+24 <[^>]*> e1df0abc ?	ldrh	r0, \[pc, #172\]	; 0+d8 <[^>]*>
0+28 <[^>]*> e1df0abc ?	ldrh	r0, \[pc, #172\]	; 0+dc <[^>]*>
0+2c <[^>]*> e1c100b0 ?	strh	r0, \[r1\]
0+30 <[^>]*> e1e100b0 ?	strh	r0, \[r1\]!
0+34 <[^>]*> e18100b2 ?	strh	r0, \[r1, r2\]
0+38 <[^>]*> e1a100b2 ?	strh	r0, \[r1, r2\]!
0+3c <[^>]*> e1c100bc ?	strh	r0, \[r1, #12\]
0+40 <[^>]*> e1e100bc ?	strh	r0, \[r1, #12\]!
0+44 <[^>]*> e14100bc ?	strh	r0, \[r1, #-12\]
0+48 <[^>]*> e08100b2 ?	strh	r0, \[r1\], r2
0+4c <[^>]*> e1cf08b8 ?	strh	r0, \[pc, #136\]	; 0+dc <[^>]*>
0+50 <[^>]*> e1d100d0 ?	ldrsb	r0, \[r1\]
0+54 <[^>]*> e1f100d0 ?	ldrsb	r0, \[r1\]!
0+58 <[^>]*> e19100d2 ?	ldrsb	r0, \[r1, r2\]
0+5c <[^>]*> e1b100d2 ?	ldrsb	r0, \[r1, r2\]!
0+60 <[^>]*> e1d100dc ?	ldrsb	r0, \[r1, #12\]
0+64 <[^>]*> e1f100dc ?	ldrsb	r0, \[r1, #12\]!
0+68 <[^>]*> e15100dc ?	ldrsb	r0, \[r1, #-12\]
0+6c <[^>]*> e09100d2 ?	ldrsb	r0, \[r1\], r2
0+70 <[^>]*> e3a000de ?	mov	r0, #222	; 0xde
0+74 <[^>]*> e1df06d0 ?	ldrsb	r0, \[pc, #96\]	; 0+dc <[^>]*>
0+78 <[^>]*> e1d100f0 ?	ldrsh	r0, \[r1\]
0+7c <[^>]*> e1f100f0 ?	ldrsh	r0, \[r1\]!
0+80 <[^>]*> e19100f2 ?	ldrsh	r0, \[r1, r2\]
0+84 <[^>]*> e1b100f2 ?	ldrsh	r0, \[r1, r2\]!
0+88 <[^>]*> e1d100fc ?	ldrsh	r0, \[r1, #12\]
0+8c <[^>]*> e1f100fc ?	ldrsh	r0, \[r1, #12\]!
0+90 <[^>]*> e15100fc ?	ldrsh	r0, \[r1, #-12\]
0+94 <[^>]*> e09100f2 ?	ldrsh	r0, \[r1\], r2
0+98 <[^>]*> e3a00cff ?	mov	r0, #65280	; 0xff00
0+9c <[^>]*> e1df03f4 ?	ldrsh	r0, \[pc, #52\]	; 0+d8 <[^>]*>
0+a0 <[^>]*> e1df03f4 ?	ldrsh	r0, \[pc, #52\]	; 0+dc <[^>]*>
0+a4 <[^>]*> e19100b2 ?	ldrh	r0, \[r1, r2\]
0+a8 <[^>]*> 119100b2 ?	ldrneh	r0, \[r1, r2\]
0+ac <[^>]*> 819100b2 ?	ldrhih	r0, \[r1, r2\]
0+b0 <[^>]*> b19100b2 ?	ldrlth	r0, \[r1, r2\]
0+b4 <[^>]*> e19100f2 ?	ldrsh	r0, \[r1, r2\]
0+b8 <[^>]*> 119100f2 ?	ldrnesh	r0, \[r1, r2\]
0+bc <[^>]*> 819100f2 ?	ldrhish	r0, \[r1, r2\]
0+c0 <[^>]*> b19100f2 ?	ldrltsh	r0, \[r1, r2\]
0+c4 <[^>]*> e19100d2 ?	ldrsb	r0, \[r1, r2\]
0+c8 <[^>]*> 119100d2 ?	ldrnesb	r0, \[r1, r2\]
0+cc <[^>]*> 819100d2 ?	ldrhisb	r0, \[r1, r2\]
0+d0 <[^>]*> b19100d2 ?	ldrltsb	r0, \[r1, r2\]
0+d4 <[^>]*> e15f00f4 ?	ldrsh	r0, \[pc, #-4\]	; 0+d8 <[^>]*>
0+d8 <[^>]*> e15f00f4 ?	ldrsh	r0, \[pc, #-4\]	; 0+dc <[^>]*>
0+dc <[^>]*> 00000000 ?	andeq	r0, r0, r0
[		]*dc:.*fred
0+e0 <[^>]*> 0000c0de ?	.*
0+e4 <[^>]*> 0000dead ?	.*
0+e8 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+ec <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
