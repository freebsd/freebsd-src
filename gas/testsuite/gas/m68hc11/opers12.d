#objdump: -d --prefix-addresses --reloc
#as: -m68hc12
#name: 68HC12 specific addressing modes (opers12)

.*: +file format elf32\-m68hc12

Disassembly of section .text:
0+0+ <start> anda	\[12,X\]
0+0004 <start\+0x4> ldaa	#10
0+0006 <start\+0x6> ldx	0+0+ <start>
[	]+7: R_M68HC12_16	L1
0+0009 <L1> ldy	0,X
0+000b <L1\+0x2> addd	1,Y
0+000d <L1\+0x4> subd	\-1,Y
0+000f <L1\+0x6> eora	15,Y
0+0011 <L1\+0x8> eora	\-16,Y
0+0013 <L1\+0xa> eorb	16,Y
0+0016 <L1\+0xd> eorb	\-17,Y
0+0019 <L1\+0x10> oraa	128,SP
0+001c <L1\+0x13> orab	\-128,SP
0+001f <L1\+0x16> orab	255,X
0+0022 <L1\+0x19> orab	\-256,X
0+0025 <L1\+0x1c> anda	256,X
0+0029 <L1\+0x20> andb	\-257,X
0+002d <L1\+0x24> anda	\[12,X\]
0+0031 <L1\+0x28> ldaa	\[257,Y\]
0+0035 <L1\+0x2c> ldab	\[32767,SP\]
0+0039 <L1\+0x30> ldd	\[32768,PC\]
0+003d <L1\+0x34> ldd	\-55,PC \{0+9 <L1>\}
0+0040 <L1\+0x37> std	A,X
0+0042 <L1\+0x39> ldx	B,X
0+0044 <L1\+0x3b> stx	D,Y
0+0046 <L1\+0x3d> addd	1,\+X
0+0048 <L1\+0x3f> addd	2,\+X
0+004a <L1\+0x41> addd	8,\+X
0+004c <L1\+0x43> addd	1,SP\+
0+004e <L1\+0x45> addd	2,SP\+
0+0050 <L1\+0x47> addd	8,SP\+
0+0052 <L1\+0x49> subd	1,\-Y
0+0054 <L1\+0x4b> subd	2,\-Y
0+0056 <L1\+0x4d> subd	8,\-Y
0+0058 <L1\+0x4f> addd	1,Y\-
0+005a <L1\+0x51> addd	2,Y\-
0+005c <L1\+0x53> addd	8,Y\-
0+005e <L1\+0x55> std	\[D,X\]
0+0060 <L1\+0x57> std	\[D,Y\]
0+0062 <L1\+0x59> std	\[D,SP\]
0+0064 <L1\+0x5b> std	\[D,PC\]
0+0066 <L1\+0x5d> beq	0+0009 <L1>
[	]+66: R_M68HC12_RL_JUMP	\*ABS\*
0+0068 <L1\+0x5f> lbeq	0+0+ <start>
[	]+68: R_M68HC12_RL_JUMP	\*ABS\*
0+006c <L1\+0x63> lbcc	0+00bc <L2>
[	]+6c: R_M68HC12_RL_JUMP	\*ABS\*
0+0070 <L1\+0x67> movb	0+0+ <start>, 1,X
[	]+73: R_M68HC12_16	start
0+0075 <L1\+0x6c> movw	1,X, 0+0+ <start>
[	]+78: R_M68HC12_16	start
0+007a <L1\+0x71> movb	0+0+ <start>, 1,\+X
[	]+7d: R_M68HC12_16	start
0+007f <L1\+0x76> movb	0+0+ <start>, 1,\-X
[	]+82: R_M68HC12_16	start
0+0084 <L1\+0x7b> movb	#23, 1,\-SP
0+0088 <L1\+0x7f> movb	0+0+ <start>, 0+0+ <start>
[	]+8a: R_M68HC12_16	L1
[	]+8c: R_M68HC12_16	L2
0+008e <L1\+0x85> movb	0+0+ <start>, A,X
[	]+91: R_M68HC12_16	L1
0+0093 <L1\+0x8a> movw	0+0+ <start>, B,X
[	]+96: R_M68HC12_16	L1
0+0098 <L1\+0x8f> movw	0+0+ <start>, D,X
[	]+9b: R_M68HC12_16	L1
0+009d <L1\+0x94> movw	D,X, A,X
0+00a1 <L1\+0x98> movw	B,SP, D,PC
0+00a5 <L1\+0x9c> movw	B,SP, 0+0+ <start>
[	]+a8: R_M68HC12_16	L1
0+00aa <L1\+0xa1> movw	B,SP, 1,X
0+00ae <L1\+0xa5> movw	D,X, A,Y
0+00b2 <L1\+0xa9> trap	#48
0+00b4 <L1\+0xab> trap	#57
0+00b6 <L1\+0xad> trap	#64
0+00b8 <L1\+0xaf> trap	#128
0+00ba <L1\+0xb1> trap	#255
0+00bc <L2> movw	1,X, 2,X
0+00c0 <L2\+0x4> movw	0+ffff <bb\+0xd7ff>, 0+ffff <bb\+0xd7ff>
0+00c6 <L2\+0xa> movw	0+ffff <bb\+0xd7ff>, 1,X
0+00cb <L2\+0xf> movw	#0+ffff <bb\+0xd7ff>, 1,X
0+00d0 <L2\+0x14> movw	0+0003 <start\+0x3>, 0+0008 <start\+0x8>
0+00d6 <L2\+0x1a> movw	#0+0003 <start\+0x3>, 0+0003 <start\+0x3>
0+00dc <L2\+0x20> movw	#0+0003 <start\+0x3>, 1,X
0+00e1 <L2\+0x25> movw	0+0003 <start\+0x3>, 1,X
0+00e6 <L2\+0x2a> movw	0+0003 <start\+0x3>, 2,X
0+00eb <L2\+0x2f> movw	0+0004 <start\+0x4>, \-2,X
0+00f0 <L2\+0x34> rts
0+00f1 <post_indexed_pb> leas	0,X
[	]+f3: R_M68HC12_16	abort
0+00f5 <t2> leax	4,Y
0+00f7 <t2\+0x2> leax	100,X
0+00fa <t2\+0x5> leas	110,SP
0+00fd <t2\+0x8> leay	10,X
0+00ff <t2\+0xa> leas	10240,Y
0+0103 <t2\+0xe> leas	-16,PC \{0+f5 <t2>\}
0+0105 <t2\+0x10> leas	15,PC \{0+116 <t2\+0x21>\}
0+0107 <t2\+0x12> leas	-256,PC \{0+b <L1\+0x2>\}
0+010b <t2\+0x16> leas	255,PC \{0+20d <max9b\+0x10e>\}
0+010e <t2\+0x19> movb	#23, 0+2345 <max9b\+0x2246>
0+0113 <t2\+0x1e> movb	#40, 12,SP
0+0117 <t2\+0x22> movb	#39, 3,\+SP
0+011b <t2\+0x26> movb	#20, 14,SP
0+011f <t2\+0x2a> movw	#0+3210 <bb\+0xa10>, 0+3456 <bb\+0xc56>
0+0125 <t2\+0x30> movw	#0+4040 <bb\+0x1840>, 12,SP
0+012a <t2\+0x35> movw	#0+3900 <bb\+0x1100>, 3,\+SP
0+012f <t2\+0x3a> movw	#0+2000 <max9b\+0x1f01>, 14,SP
