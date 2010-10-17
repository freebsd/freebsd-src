#objdump: -d --prefix-addresses --reloc
#as: -m68hc11
#name: lbranch

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+0+ <_rcall> ldaa	#16
0+0002 <_rcall\+0x2> jmp	0+0+ <_rcall>
[	]+2: R_M68HC11_RL_JUMP	\*ABS\*
[	]+3: R_M68HC11_16	Lend
0+0005 <_rcall\+0x5> jsr	0+0+ <_rcall>
[	]+5: R_M68HC11_RL_JUMP	\*ABS\*
[	]+6: R_M68HC11_16	toto
0+0008 <_rcall\+0x8> beq	0+000d <_rcall\+0xd>
[	]+8: R_M68HC11_RL_JUMP	\*ABS\*
0+000a <_rcall\+0xa> jmp	0+0+ <_rcall>
[	]+b: R_M68HC11_16	toto
0+000d <_rcall\+0xd> bne	0+0012 <_rcall\+0x12>
[	]+d: R_M68HC11_RL_JUMP	\*ABS\*
0+000f <_rcall\+0xf> jmp	0+0+ <_rcall>
[	]+10: R_M68HC11_16	toto
0+0012 <_rcall\+0x12> bcc	0+0017 <_rcall\+0x17>
[	]+12: R_M68HC11_RL_JUMP	\*ABS\*
0+0014 <_rcall\+0x14> jmp	0+0+ <_rcall>
[	]+15: R_M68HC11_16	toto
0+0017 <_rcall\+0x17> bcs	0+001c <_rcall\+0x1c>
[	]+17: R_M68HC11_RL_JUMP	\*ABS\*
0+0019 <_rcall\+0x19> jmp	0+0+ <_rcall>
[	]+1a: R_M68HC11_16	toto
0+001c <_rcall\+0x1c> xgdx
0+001d <_rcall\+0x1d> xgdx
0+001e <_rcall\+0x1e> bne	0+0023 <_rcall\+0x23>
[	]+1e: R_M68HC11_RL_JUMP	\*ABS\*
0+0020 <_rcall\+0x20> jmp	0+0+ <_rcall>
[	]+21: R_M68HC11_16	bidule
0+0023 <_rcall\+0x23> bcc	0+0028 <_rcall\+0x28>
[	]+23: R_M68HC11_RL_JUMP	\*ABS\*
0+0025 <_rcall\+0x25> jmp	0+0+ <_rcall>
[	]+26: R_M68HC11_16	bidule
0+0028 <_rcall\+0x28> bcs	0+002d <_rcall\+0x2d>
[	]+28: R_M68HC11_RL_JUMP	\*ABS\*
0+002a <_rcall\+0x2a> jmp	0+0+ <_rcall>
[	]+2b: R_M68HC11_16	bidule
0+002d <_rcall\+0x2d> xgdx
0+002e <_rcall\+0x2e> jmp	0+00c8 <_rcall\+0xc8>
[	]+2e: R_M68HC11_RL_JUMP	\*ABS\*
0+0031 <_rcall\+0x31> jsr	0+0783 <bidule\+0x603>
[	]+31: R_M68HC11_RL_JUMP	\*ABS\*
0+0034 <_rcall\+0x34> beq	0+0039 <_rcall\+0x39>
[	]+34: R_M68HC11_RL_JUMP	\*ABS\*
0+0036 <_rcall\+0x36> jmp	0+0+ <_rcall>
[	]+37: R_M68HC11_16	Lend
0+0039 <_rcall\+0x39> jsr	0+0+ <_rcall>
[	]+39: R_M68HC11_RL_JUMP	\*ABS\*
[	]+3a: R_M68HC11_16	toto
0+003c <_rcall\+0x3c> bne	0+0041 <_rcall\+0x41>
[	]+3c: R_M68HC11_RL_JUMP	\*ABS\*
0+003e <_rcall\+0x3e> jmp	0+0+ <_rcall>
[	]+3f: R_M68HC11_16	toto
	...
0+0109 <_rcall\+0x109> ldaa	\*0+0+ <_rcall>
[	]+10a: R_M68HC11_8	.page0
0+010b <Lend> bls	0+0110 <Lend\+0x5>
[	]+10b: R_M68HC11_RL_JUMP	\*ABS\*
0+010d <Lend\+0x2> jmp	0+0+ <_rcall>
[	]+10e: R_M68HC11_16	external_op
0+0110 <Lend\+0x5> bhi	0+0115 <Lend\+0xa>
[	]+110: R_M68HC11_RL_JUMP	\*ABS\*
0+0112 <Lend\+0x7> jmp	0+0+ <_rcall>
[	]+113: R_M68HC11_16	external_op
0+0115 <Lend\+0xa> jsr	0+0+ <_rcall>
[	]+115: R_M68HC11_RL_JUMP	\*ABS\*
[	]+116: R_M68HC11_16	out
0+0118 <Lend\+0xd> ldx	#0+000c <_rcall\+0xc>
0+011b <toto> rts
	...
0+0180 <bidule> rts
