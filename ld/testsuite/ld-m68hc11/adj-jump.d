#source: adj-jump.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+8000 <_start> bra	0+8074 <L3>
	...
0+8016 <_start\+0x16> bra	0+8074 <L3>
0+8018 <L1> addd	0,x
0+801a <L1\+0x2> bne	0+8018 <L1>
0+801c <L1\+0x4> addd	\*0+4 <_toto>
0+801e <L1\+0x6> beq	0+8018 <L1>
0+8020 <L1\+0x8> addd	\*0+5 <_toto\+0x1>
0+8022 <L1\+0xa> bne	0+8018 <L1>
0+8024 <L1\+0xc> bgt	0+8018 <L1>
0+8026 <L1\+0xe> bge	0+8018 <L1>
0+8028 <L1\+0x10> beq	0+8018 <L1>
0+802a <L1\+0x12> ble	0+8018 <L1>
0+802c <L1\+0x14> blt	0+8018 <L1>
0+802e <L1\+0x16> bhi	0+8018 <L1>
0+8030 <L1\+0x18> bcc	0+8018 <L1>
0+8032 <L1\+0x1a> beq	0+8018 <L1>
0+8034 <L1\+0x1c> bls	0+8018 <L1>
0+8036 <L1\+0x1e> bcs	0+8018 <L1>
0+8038 <L1\+0x20> bcs	0+8018 <L1>
0+803a <L1\+0x22> bmi	0+8018 <L1>
0+803c <L1\+0x24> bvs	0+8018 <L1>
0+803e <L1\+0x26> bcc	0+8018 <L1>
0+8040 <L1\+0x28> bpl	0+8018 <L1>
0+8042 <L1\+0x2a> bvc	0+8018 <L1>
0+8044 <L1\+0x2c> bne	0+8018 <L1>
0+8046 <L1\+0x2e> brn	0+8018 <L1>
0+8048 <L1\+0x30> bra	0+8018 <L1>
0+804a <L1\+0x32> addd	\*0+4 <_toto>
0+804c <L1\+0x34> addd	\*0+4 <_toto>
0+804e <L1\+0x36> addd	\*0+4 <_toto>
0+8050 <L1\+0x38> addd	\*0+4 <_toto>
0+8052 <L1\+0x3a> addd	\*0+4 <_toto>
0+8054 <L1\+0x3c> addd	\*0+4 <_toto>
0+8056 <L1\+0x3e> addd	\*0+4 <_toto>
0+8058 <L1\+0x40> addd	\*0+4 <_toto>
0+805a <L1\+0x42> addd	\*0+4 <_toto>
0+805c <L1\+0x44> addd	\*0+4 <_toto>
0+805e <L1\+0x46> addd	\*0+4 <_toto>
0+8060 <L1\+0x48> addd	\*0+4 <_toto>
0+8062 <L1\+0x4a> addd	\*0+4 <_toto>
0+8064 <L1\+0x4c> addd	\*0+4 <_toto>
0+8066 <L1\+0x4e> addd	\*0+4 <_toto>
0+8068 <L2> bra	0+8000 <_start>
0+806a <L2\+0x2> bne	0+8068 <L2>
0+806c <L2\+0x4> beq	0+8074 <L3>
0+806e <L2\+0x6> addd	\*0+4 <_toto>
0+8070 <L2\+0x8> beq	0+8074 <L3>
0+8072 <L2\+0xa> addd	\*0+4 <_toto>
0+8074 <L3> addd	\*0+4 <_toto>
0+8076 <L3\+0x2> rts
