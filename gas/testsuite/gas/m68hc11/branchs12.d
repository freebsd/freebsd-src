#objdump: -d --prefix-addresses --reloc
#as: -m68hc12
#name: 68HC12 branchs

.*: +file format elf32\-m68hc12

Disassembly of section .text:
0+00 <start> bgt	0+48 <L1>
[	]+0: R_M68HC12_RL_JUMP	\*ABS\*
0+02 <start\+0x2> bge	0+48 <L1>
[	]+2: R_M68HC12_RL_JUMP	\*ABS\*
0+04 <start\+0x4> ble	0+48 <L1>
[	]+4: R_M68HC12_RL_JUMP	\*ABS\*
0+06 <start\+0x6> blt	0+48 <L1>
[	]+6: R_M68HC12_RL_JUMP	\*ABS\*
0+08 <start\+0x8> bhi	0+48 <L1>
[	]+8: R_M68HC12_RL_JUMP	\*ABS\*
0+0a <start\+0xa> bcc	0+48 <L1>
[	]+a: R_M68HC12_RL_JUMP	\*ABS\*
0+0c <start\+0xc> bcc	0+48 <L1>
[	]+c: R_M68HC12_RL_JUMP	\*ABS\*
0+0e <start\+0xe> beq	0+48 <L1>
[	]+e: R_M68HC12_RL_JUMP	\*ABS\*
0+10 <start\+0x10> bls	0+48 <L1>
[	]+10: R_M68HC12_RL_JUMP	\*ABS\*
0+12 <start\+0x12> bcs	0+48 <L1>
[	]+12: R_M68HC12_RL_JUMP	\*ABS\*
0+14 <start\+0x14> bcs	0+48 <L1>
[	]+14: R_M68HC12_RL_JUMP	\*ABS\*
0+16 <start\+0x16> bmi	0+48 <L1>
[	]+16: R_M68HC12_RL_JUMP	\*ABS\*
0+18 <start\+0x18> bvs	0+48 <L1>
[	]+18: R_M68HC12_RL_JUMP	\*ABS\*
0+1a <start\+0x1a> bra	0+48 <L1>
[	]+1a: R_M68HC12_RL_JUMP	\*ABS\*
0+1c <start\+0x1c> bvc	0+48 <L1>
[	]+1c: R_M68HC12_RL_JUMP	\*ABS\*
0+1e <start\+0x1e> bne	0+48 <L1>
[	]+1e: R_M68HC12_RL_JUMP	\*ABS\*
0+20 <start\+0x20> bpl	0+48 <L1>
[	]+20: R_M68HC12_RL_JUMP	\*ABS\*
0+22 <start\+0x22> brn	0+48 <L1>
[	]+22: R_M68HC12_RL_JUMP	\*ABS\*
0+24 <start\+0x24> bgt	0+00 <start>
[	]+24: R_M68HC12_RL_JUMP	\*ABS\*
0+26 <start\+0x26> bge	0+00 <start>
[	]+26: R_M68HC12_RL_JUMP	\*ABS\*
0+28 <start\+0x28> ble	0+00 <start>
[	]+28: R_M68HC12_RL_JUMP	\*ABS\*
0+2a <start\+0x2a> blt	0+00 <start>
[	]+2a: R_M68HC12_RL_JUMP	\*ABS\*
0+2c <start\+0x2c> bhi	0+00 <start>
[	]+2c: R_M68HC12_RL_JUMP	\*ABS\*
0+2e <start\+0x2e> bcc	0+00 <start>
[	]+2e: R_M68HC12_RL_JUMP	\*ABS\*
0+30 <start\+0x30> bcc	0+00 <start>
[	]+30: R_M68HC12_RL_JUMP	\*ABS\*
0+32 <start\+0x32> beq	0+00 <start>
[	]+32: R_M68HC12_RL_JUMP	\*ABS\*
0+34 <start\+0x34> bls	0+00 <start>
[	]+34: R_M68HC12_RL_JUMP	\*ABS\*
0+36 <start\+0x36> bcs	0+00 <start>
[	]+36: R_M68HC12_RL_JUMP	\*ABS\*
0+38 <start\+0x38> bcs	0+00 <start>
[	]+38: R_M68HC12_RL_JUMP	\*ABS\*
0+3a <start\+0x3a> bmi	0+00 <start>
[	]+3a: R_M68HC12_RL_JUMP	\*ABS\*
0+3c <start\+0x3c> bvs	0+00 <start>
[	]+3c: R_M68HC12_RL_JUMP	\*ABS\*
0+3e <start\+0x3e> bra	0+00 <start>
[	]+3e: R_M68HC12_RL_JUMP	\*ABS\*
0+40 <start\+0x40> bvc	0+00 <start>
[	]+40: R_M68HC12_RL_JUMP	\*ABS\*
0+42 <start\+0x42> bne	0+00 <start>
[	]+42: R_M68HC12_RL_JUMP	\*ABS\*
0+44 <start\+0x44> bpl	0+00 <start>
[	]+44: R_M68HC12_RL_JUMP	\*ABS\*
0+46 <start\+0x46> brn	0+00 <start>
[	]+46: R_M68HC12_RL_JUMP	\*ABS\*
0+48 <L1> lbgt	0+1e7 <L2>
[	]+48: R_M68HC12_RL_JUMP	\*ABS\*
0+4c <L1\+0x4> lbge	0+1e7 <L2>
[	]+4c: R_M68HC12_RL_JUMP	\*ABS\*
0+50 <L1\+0x8> lble	0+1e7 <L2>
[	]+50: R_M68HC12_RL_JUMP	\*ABS\*
0+54 <L1\+0xc> lblt	0+1e7 <L2>
[	]+54: R_M68HC12_RL_JUMP	\*ABS\*
0+58 <L1\+0x10> lbhi	0+1e7 <L2>
[	]+58: R_M68HC12_RL_JUMP	\*ABS\*
0+5c <L1\+0x14> lbcc	0+1e7 <L2>
[	]+5c: R_M68HC12_RL_JUMP	\*ABS\*
0+60 <L1\+0x18> lbcc	0+1e7 <L2>
[	]+60: R_M68HC12_RL_JUMP	\*ABS\*
0+64 <L1\+0x1c> lbeq	0+1e7 <L2>
[	]+64: R_M68HC12_RL_JUMP	\*ABS\*
0+68 <L1\+0x20> lbls	0+1e7 <L2>
[	]+68: R_M68HC12_RL_JUMP	\*ABS\*
0+6c <L1\+0x24> lbcs	0+1e7 <L2>
[	]+6c: R_M68HC12_RL_JUMP	\*ABS\*
0+70 <L1\+0x28> lbcs	0+1e7 <L2>
[	]+70: R_M68HC12_RL_JUMP	\*ABS\*
0+74 <L1\+0x2c> lbmi	0+1e7 <L2>
[	]+74: R_M68HC12_RL_JUMP	\*ABS\*
0+78 <L1\+0x30> lbvs	0+1e7 <L2>
[	]+78: R_M68HC12_RL_JUMP	\*ABS\*
0+7c <L1\+0x34> lbra	0+1e7 <L2>
[	]+7c: R_M68HC12_RL_JUMP	\*ABS\*
0+80 <L1\+0x38> lbvc	0+1e7 <L2>
[	]+80: R_M68HC12_RL_JUMP	\*ABS\*
0+84 <L1\+0x3c> lbne	0+1e7 <L2>
[	]+84: R_M68HC12_RL_JUMP	\*ABS\*
0+88 <L1\+0x40> lbpl	0+1e7 <L2>
[	]+88: R_M68HC12_RL_JUMP	\*ABS\*
0+8c <L1\+0x44> lbrn	0+1e7 <L2>
[	]+8c: R_M68HC12_RL_JUMP	\*ABS\*
0+90 <L1\+0x48> lbgt	0+00 <start>
[	]+90: R_M68HC12_RL_JUMP	\*ABS\*
[	]+92: R_M68HC12_PCREL_16	undefined
0+94 <L1\+0x4c> lbge	0+00 <start>
[	]+94: R_M68HC12_RL_JUMP	\*ABS\*
[	]+96: R_M68HC12_PCREL_16	undefined
0+98 <L1\+0x50> lble	0+00 <start>
[	]+98: R_M68HC12_RL_JUMP	\*ABS\*
[	]+9a: R_M68HC12_PCREL_16	undefined
0+9c <L1\+0x54> lblt	0+00 <start>
[	]+9c: R_M68HC12_RL_JUMP	\*ABS\*
[	]+9e: R_M68HC12_PCREL_16	undefined
0+a0 <L1\+0x58> lbhi	0+00 <start>
[	]+a0: R_M68HC12_RL_JUMP	\*ABS\*
[	]+a2: R_M68HC12_PCREL_16	undefined
0+a4 <L1\+0x5c> lbcc	0+00 <start>
[	]+a4: R_M68HC12_RL_JUMP	\*ABS\*
[	]+a6: R_M68HC12_PCREL_16	undefined
0+a8 <L1\+0x60> lbcc	0+00 <start>
[	]+a8: R_M68HC12_RL_JUMP	\*ABS\*
[	]+aa: R_M68HC12_PCREL_16	undefined
0+ac <L1\+0x64> lbeq	0+00 <start>
[	]+ac: R_M68HC12_RL_JUMP	\*ABS\*
[	]+ae: R_M68HC12_PCREL_16	undefined
0+b0 <L1\+0x68> lbls	0+00 <start>
[	]+b0: R_M68HC12_RL_JUMP	\*ABS\*
[	]+b2: R_M68HC12_PCREL_16	undefined
0+b4 <L1\+0x6c> lbcs	0+00 <start>
[	]+b4: R_M68HC12_RL_JUMP	\*ABS\*
[	]+b6: R_M68HC12_PCREL_16	undefined
0+b8 <L1\+0x70> lbcs	0+00 <start>
[	]+b8: R_M68HC12_RL_JUMP	\*ABS\*
[	]+ba: R_M68HC12_PCREL_16	undefined
0+bc <L1\+0x74> lbmi	0+00 <start>
[	]+bc: R_M68HC12_RL_JUMP	\*ABS\*
[	]+be: R_M68HC12_PCREL_16	undefined
0+c0 <L1\+0x78> lbvs	0+00 <start>
[	]+c0: R_M68HC12_RL_JUMP	\*ABS\*
[	]+c2: R_M68HC12_PCREL_16	undefined
0+c4 <L1\+0x7c> jmp	0+00 <start>
[	]+c4: R_M68HC12_RL_JUMP	\*ABS\*
[	]+c5: R_M68HC12_16	undefined
0+c7 <L1\+0x7f> lbvc	0+00 <start>
[	]+c7: R_M68HC12_RL_JUMP	\*ABS\*
[	]+c9: R_M68HC12_PCREL_16	undefined
0+cb <L1\+0x83> lbne	0+00 <start>
[	]+cb: R_M68HC12_RL_JUMP	\*ABS\*
[	]+cd: R_M68HC12_PCREL_16	undefined
0+cf <L1\+0x87> lbpl	0+00 <start>
[	]+cf: R_M68HC12_RL_JUMP	\*ABS\*
[	]+d1: R_M68HC12_PCREL_16	undefined
0+d3 <L1\+0x8b> lbrn	0+00 <start>
[	]+d3: R_M68HC12_RL_JUMP	\*ABS\*
[	]+d5: R_M68HC12_PCREL_16	undefined
0+d7 <L1\+0x8f> lbgt	0+10 <start\+0x10>
[	]+d7: R_M68HC12_RL_JUMP	\*ABS\*
[	]+d9: R_M68HC12_PCREL_16	undefined
0+db <L1\+0x93> lbge	0+10 <start\+0x10>
[	]+db: R_M68HC12_RL_JUMP	\*ABS\*
[	]+dd: R_M68HC12_PCREL_16	undefined
0+df <L1\+0x97> lble	0+10 <start\+0x10>
[	]+df: R_M68HC12_RL_JUMP	\*ABS\*
[	]+e1: R_M68HC12_PCREL_16	undefined
0+e3 <L1\+0x9b> lblt	0+10 <start\+0x10>
[	]+e3: R_M68HC12_RL_JUMP	\*ABS\*
[	]+e5: R_M68HC12_PCREL_16	undefined
0+e7 <L1\+0x9f> lbhi	0+10 <start\+0x10>
[	]+e7: R_M68HC12_RL_JUMP	\*ABS\*
[	]+e9: R_M68HC12_PCREL_16	undefined
0+eb <L1\+0xa3> lbcc	0+10 <start\+0x10>
[	]+eb: R_M68HC12_RL_JUMP	\*ABS\*
[	]+ed: R_M68HC12_PCREL_16	undefined
0+ef <L1\+0xa7> lbcc	0+10 <start\+0x10>
[	]+ef: R_M68HC12_RL_JUMP	\*ABS\*
[	]+f1: R_M68HC12_PCREL_16	undefined
0+f3 <L1\+0xab> lbeq	0+10 <start\+0x10>
[	]+f3: R_M68HC12_RL_JUMP	\*ABS\*
[	]+f5: R_M68HC12_PCREL_16	undefined
0+f7 <L1\+0xaf> lbls	0+10 <start\+0x10>
[	]+f7: R_M68HC12_RL_JUMP	\*ABS\*
[	]+f9: R_M68HC12_PCREL_16	undefined
0+fb <L1\+0xb3> lbcs	0+10 <start\+0x10>
[	]+fb: R_M68HC12_RL_JUMP	\*ABS\*
[	]+fd: R_M68HC12_PCREL_16	undefined
0+ff <L1\+0xb7> lbcs	0+10 <start\+0x10>
[	]+ff: R_M68HC12_RL_JUMP	\*ABS\*
[	]+101: R_M68HC12_PCREL_16	undefined
0+103 <L1\+0xbb> lbmi	0+10 <start\+0x10>
[	]+103: R_M68HC12_RL_JUMP	\*ABS\*
[	]+105: R_M68HC12_PCREL_16	undefined
0+107 <L1\+0xbf> lbvs	0+10 <start\+0x10>
[	]+107: R_M68HC12_RL_JUMP	\*ABS\*
[	]+109: R_M68HC12_PCREL_16	undefined
0+10b <L1\+0xc3> lbra	0+10 <start\+0x10>
[	]+10b: R_M68HC12_RL_JUMP	\*ABS\*
[	]+10d: R_M68HC12_PCREL_16	undefined
0+10f <L1\+0xc7> lbvc	0+10 <start\+0x10>
[	]+10f: R_M68HC12_RL_JUMP	\*ABS\*
[	]+111: R_M68HC12_PCREL_16	undefined
0+113 <L1\+0xcb> lbne	0+10 <start\+0x10>
[	]+113: R_M68HC12_RL_JUMP	\*ABS\*
[	]+115: R_M68HC12_PCREL_16	undefined
0+117 <L1\+0xcf> lbpl	0+10 <start\+0x10>
[	]+117: R_M68HC12_RL_JUMP	\*ABS\*
[	]+119: R_M68HC12_PCREL_16	undefined
0+11b <L1\+0xd3> lbrn	0+10 <start\+0x10>
[	]+11b: R_M68HC12_RL_JUMP	\*ABS\*
[	]+11d: R_M68HC12_PCREL_16	undefined
	...
0+1e7 <L2> rts
