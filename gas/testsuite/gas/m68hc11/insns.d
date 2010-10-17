#objdump: -d --prefix-addresses --reloc
#as: -m68hc11
#name: insns

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+0+ <_start> lds	#0+0400 <stack_end>
[	]+1: R_M68HC11_16	stack
0+0003 <_start\+0x3> ldx	#0+0001 <_start\+0x1>
0+0006 <Loop> jsr	0+0+ <_start>
[	]+6: R_M68HC11_RL_JUMP	\*ABS\*
[	]+7: R_M68HC11_16	test
0+0009 <Loop\+0x3> dex
0+000a <Loop\+0x4> bne	0+0006 <Loop>
[	]+a: R_M68HC11_RL_JUMP	\*ABS\*
0+000c <Stop> .byte	0xcd, 0x03
0+000e <Stop\+0x2> bra	0+0+ <_start>
[	]+e: R_M68HC11_RL_JUMP	\*ABS\*
0+0010 <test> ldd	#0+0002 <_start\+0x2>
0+0013 <test\+0x3> jsr	0+0+ <_start>
[	]+13: R_M68HC11_RL_JUMP	\*ABS\*
[	]+14: R_M68HC11_16	test2
0+0016 <test\+0x6> rts
0+0017 <test2> ldx	23,y
0+001a <test2\+0x3> std	23,x
0+001c <test2\+0x5> ldd	0,x
0+001e <test2\+0x7> sty	0,y
0+0021 <test2\+0xa> stx	0,y
0+0024 <test2\+0xd> brclr	6,x #\$04 0+0017 <test2>
[	]+24: R_M68HC11_RL_JUMP	\*ABS\*
0+0028 <test2\+0x11> brclr	12,x #\$08 0+0017 <test2>
[	]+28: R_M68HC11_RL_JUMP	\*ABS\*
0+002c <test2\+0x15> ldd	\*0+0+ <_start>
[	]+2d: R_M68HC11_8	ZD1
0+002e <test2\+0x17> ldx	\*0+0002 <_start\+0x2>
[	]+2f: R_M68HC11_8	ZD1
0+0030 <test2\+0x19> clr	0+0+ <_start>
[	]+31: R_M68HC11_16	ZD2
0+0033 <test2\+0x1c> clr	0+0001 <_start\+0x1>
[	]+34: R_M68HC11_16	ZD2
0+0036 <test2\+0x1f> bne	0+0034 <test2\+0x1d>
0+0038 <test2\+0x21> beq	0+003c <test2\+0x25>
0+003a <test2\+0x23> bclr	\*0+0001 <_start\+0x1> #\$20
[	]+3b: R_M68HC11_8	ZD1
0+003d <test2\+0x26> brclr	\*0+0002 <_start\+0x2> #\$28 0+0017 <test2>
[	]+3d: R_M68HC11_RL_JUMP	\*ABS\*
[	]+3e: R_M68HC11_8	ZD2
0+0041 <test2\+0x2a> ldy	#0+ffec <stack_end\+0xfbec>
[	]+43: R_M68HC11_16	_start
0+0045 <test2\+0x2e> ldd	12,y
0+0048 <test2\+0x31> addd	44,y
0+004b <test2\+0x34> addd	50,y
0+004e <test2\+0x37> subd	0+002c <test2\+0x15>
0+0051 <test2\+0x3a> subd	#0+002c <test2\+0x15>
0+0054 <test2\+0x3d> jmp	0+0+ <_start>
[	]+54: R_M68HC11_RL_JUMP	\*ABS\*
[	]+55: R_M68HC11_16	Stop
0+0057 <L1> anda	#23
[	]+58: R_M68HC11_LO8	\.text
0+0059 <L1\+0x2> andb	#0
[	]+5a: R_M68HC11_HI8	\.text
0+5b <L1\+0x4> ldab	#0
[	]+5c: R_M68HC11_PAGE	test2
0+5d <L1\+0x6> ldy	#0+ <_start>
[	]+5f: R_M68HC11_LO16	test2
0+61 <L1\+0xa> rts
