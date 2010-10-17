#objdump: -d --prefix-addresses --reloc
#as: -m68hc12
#name: 68HC12 specific instructions (insns12)

.*:  +file format elf32-m68hc12

Disassembly of section .text:
0+ <call_test> call	0+ <call_test> \{0+ <call_test>, 0\}
			0: R_M68HC12_RL_JUMP	\*ABS\*
			1: R_M68HC12_24	_foo
0+4 <call_test\+0x4> call	0+ <call_test> \{0+ <call_test>, 1\}
			4: R_M68HC12_RL_JUMP	\*ABS\*
			5: R_M68HC12_LO16	_foo
0+8 <call_test\+0x8> call	0+ <call_test> \{0+ <call_test>, 0\}
			8: R_M68HC12_RL_JUMP	\*ABS\*
			9: R_M68HC12_LO16	_foo
			b: R_M68HC12_PAGE	foo_page
0+c <call_test\+0xc> call	0,X, 3
			c: R_M68HC12_RL_JUMP	\*ABS\*
0+f <call_test\+0xf> call	4,Y, 12
			f: R_M68HC12_RL_JUMP	\*ABS\*
0+12 <call_test\+0x12> call	7,SP, 13
			12: R_M68HC12_RL_JUMP	\*ABS\*
0+15 <call_test\+0x15> call	12,X, 0
			15: R_M68HC12_RL_JUMP	\*ABS\*
			17: R_M68HC12_PAGE	foo_page
0+18 <call_test\+0x18> call	4,Y, 0
			18: R_M68HC12_RL_JUMP	\*ABS\*
			1a: R_M68HC12_PAGE	foo_page
0+1b <call_test\+0x1b> call	7,SP, 0
			1b: R_M68HC12_RL_JUMP	\*ABS\*
			1d: R_M68HC12_PAGE	foo_page
0+1e <call_test\+0x1e> call	\[D,X\]
			1e: R_M68HC12_RL_JUMP	\*ABS\*
0+20 <call_test\+0x20> ldab	\[32767,SP\]
0+24 <call_test\+0x24> call	\[2048,SP\]
			24: R_M68HC12_RL_JUMP	\*ABS\*
0+28 <call_test\+0x28> call	\[0,X\]
			28: R_M68HC12_RL_JUMP	\*ABS\*
			2a: R_M68HC12_16	_foo
0+2c <call_test\+0x2c> rtc
0+2d <special_test> emacs	0+ <call_test>
			2f: R_M68HC12_16	_foo
0+31 <special_test\+0x4> maxa	0,X
0+34 <special_test\+0x7> maxa	819,Y
0+39 <special_test\+0xc> maxa	\[D,X\]
0+3c <special_test\+0xf> maxa	\[0,X\]
			3f: R_M68HC12_16	_foo
0+41 <special_test\+0x14> maxm	0,X
0+44 <special_test\+0x17> maxm	819,Y
0+49 <special_test\+0x1c> maxm	\[D,X\]
0+4c <special_test\+0x1f> maxm	\[0,X\]
			4f: R_M68HC12_16	_foo
0+51 <special_test\+0x24> emaxd	0,X
0+54 <special_test\+0x27> emaxd	819,Y
0+59 <special_test\+0x2c> emaxd	\[D,X\]
0+5c <special_test\+0x2f> emaxd	\[0,X\]
			5f: R_M68HC12_16	_foo
0+61 <special_test\+0x34> emaxm	0,X
0+64 <special_test\+0x37> emaxm	819,Y
0+69 <special_test\+0x3c> emaxm	\[D,X\]
0+6c <special_test\+0x3f> emaxm	\[0,X\]
			6f: R_M68HC12_16	_foo
0+71 <special_test\+0x44> mina	0,X
0+74 <special_test\+0x47> mina	819,Y
0+79 <special_test\+0x4c> mina	\[D,X\]
0+7c <special_test\+0x4f> mina	\[0,X\]
			7f: R_M68HC12_16	_foo
0+81 <special_test\+0x54> minm	0,X
0+84 <special_test\+0x57> minm	819,Y
0+89 <special_test\+0x5c> minm	\[D,X\]
0+8c <special_test\+0x5f> minm	\[0,X\]
			8f: R_M68HC12_16	_foo
0+91 <special_test\+0x64> emind	0,X
0+94 <special_test\+0x67> emind	819,Y
0+99 <special_test\+0x6c> emind	\[D,X\]
0+9c <special_test\+0x6f> emind	\[0,X\]
			9f: R_M68HC12_16	_foo
0+a1 <special_test\+0x74> emul
0+a2 <special_test\+0x75> emuls
0+a4 <special_test\+0x77> etbl	3,X
0+a7 <special_test\+0x7a> etbl	4,PC \{0+ae <special_test\+0x81>\}
0+aa <special_test\+0x7d> rev
0+ac <special_test\+0x7f> revw
0+ae <special_test\+0x81> wav
