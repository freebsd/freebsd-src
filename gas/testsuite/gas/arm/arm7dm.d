# name: ARM 7DM instructions
# as: -mcpu=arm7dm
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]+> e0c10392 ?	smull	r0, r1, r2, r3
0+04 <[^>]+> e0810392 ?	umull	r0, r1, r2, r3
0+08 <[^>]+> e0e10392 ?	smlal	r0, r1, r2, r3
0+0c <[^>]+> e0a10394 ?	umlal	r0, r1, r4, r3
0+10 <[^>]+> 10c10493 ?	smullne	r0, r1, r3, r4
0+14 <[^>]+> e0d01b99 ?	smulls	r1, r0, r9, fp
0+18 <[^>]+> 00b92994 ?	umlalseq	r2, r9, r4, r9
0+1c <[^>]+> a0eaee98 ?	smlalge	lr, sl, r8, lr
0+20 <[^>]+> e322f000 ?	msr	CPSR_x, #0	; 0x0
0+24 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+28 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+2c <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
