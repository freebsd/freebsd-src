#objdump: -dr --prefix-addresses --show-raw-insn
#name: ARM ldr with immediate constant (WinCE version)
#as: -mcpu=arm7m -EL
#source: ldconst.s

# This file is the same as ldconst.d except that the PC-
# relative LDR instructions have not had a -8 bias inserted.

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]*> e3a00000 ?	mov	r0, #0	; 0x0
0+04 <[^>]*> e3a004ff ?	mov	r0, #-16777216	; 0xff000000
0+08 <[^>]*> e3e00000 ?	mvn	r0, #0	; 0x0
0+0c <[^>]*> e51f000c ?	ldr	r0, \[pc, #-12\]	; 0+08 <[^>]*>
0+10 <[^>]*> 0fff0000 ?	.*
0+14 <[^>]*> e3a0e000 ?	mov	lr, #0	; 0x0
0+18 <[^>]*> e3a0e8ff ?	mov	lr, #16711680	; 0xff0000
0+1c <[^>]*> e3e0e8ff ?	mvn	lr, #16711680	; 0xff0000
0+20 <[^>]*> e51fe00c ?	ldr	lr, \[pc, #-12\]	; 0+1c <[^>]*>
0+24 <[^>]*> 00fff000 ?	.*
0+28 <[^>]*> 03a00000 ?	moveq	r0, #0	; 0x0
0+2c <[^>]*> 03a00cff ?	moveq	r0, #65280	; 0xff00
0+30 <[^>]*> 03e00cff ?	mvneq	r0, #65280	; 0xff00
0+34 <[^>]*> 051f000c ?	ldreq	r0, \[pc, #-12\]	; 0+30 <[^>]*>
0+38 <[^>]*> 000fff00 ?	.*
0+3c <[^>]*> 43a0b000 ?	movmi	fp, #0	; 0x0
0+40 <[^>]*> 43a0b0ff ?	movmi	fp, #255	; 0xff
0+44 <[^>]*> 43e0b0ff ?	mvnmi	fp, #255	; 0xff
0+48 <[^>]*> 451fb00c ?	ldrmi	fp, \[pc, #-12\]	; 0+44 <[^>]*>
0+4c <[^>]*> 0000fff0 ?	.*
