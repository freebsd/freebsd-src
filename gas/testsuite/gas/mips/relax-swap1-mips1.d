#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS1 branch relaxation with swapping
#as: -32 -mips1 -KPIC -relax-branch
#source: relax-swap1.s
#stderr: relax-swap1.l

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> b	00000000 <foo>
0+0004 <[^>]*> move	v0,a0
0+0008 <[^>]*> lw	at,2\(gp\)
[ 	]*8: R_MIPS_GOT16	\.text
0+000c <[^>]*> nop
0+0010 <[^>]*> addiu	at,at,1000
[ 	]*10: R_MIPS_LO16	\.text
0+0014 <[^>]*> jr	at
0+0018 <[^>]*> move	v0,a0
0+001c <[^>]*> lw	v0,0\(a0\)
0+0020 <[^>]*> b	00000000 <foo>
0+0024 <[^>]*> nop
0+0028 <[^>]*> lw	v0,0\(a0\)
0+002c <[^>]*> lw	at,2\(gp\)
[ 	]*2c: R_MIPS_GOT16	\.text
0+0030 <[^>]*> nop
0+0034 <[^>]*> addiu	at,at,1000
[ 	]*34: R_MIPS_LO16	\.text
0+0038 <[^>]*> jr	at
0+003c <[^>]*> nop
0+0040 <[^>]*> b	00000000 <foo>
0+0044 <[^>]*> sw	v0,0\(a0\)
0+0048 <[^>]*> lw	at,2\(gp\)
[ 	]*48: R_MIPS_GOT16	\.text
0+004c <[^>]*> nop
0+0050 <[^>]*> addiu	at,at,1000
[ 	]*50: R_MIPS_LO16	\.text
0+0054 <[^>]*> jr	at
0+0058 <[^>]*> sw	v0,0\(a0\)
0+005c <[^>]*> move	v0,a0
0+0060 <[^>]*> beq	v0,v1,00000000 <foo>
0+0064 <[^>]*> nop
0+0068 <[^>]*> move	v0,a0
0+006c <[^>]*> bne	v0,v1,00000084 <foo\+0x84>
0+0070 <[^>]*> nop
0+0074 <[^>]*> lw	at,2\(gp\)
[ 	]*74: R_MIPS_GOT16	\.text
0+0078 <[^>]*> nop
0+007c <[^>]*> addiu	at,at,1000
[ 	]*7c: R_MIPS_LO16	\.text
0+0080 <[^>]*> jr	at
0+0084 <[^>]*> nop
0+0088 <[^>]*> beq	a0,a1,00000000 <foo>
0+008c <[^>]*> move	v0,a0
0+0090 <[^>]*> bne	a0,a1,000000a8 <foo\+0xa8>
0+0094 <[^>]*> nop
0+0098 <[^>]*> lw	at,2\(gp\)
[ 	]*98: R_MIPS_GOT16	\.text
0+009c <[^>]*> nop
0+00a0 <[^>]*> addiu	at,at,1000
[ 	]*a0: R_MIPS_LO16	\.text
0+00a4 <[^>]*> jr	at
0+00a8 <[^>]*> move	v0,a0
0+00ac <[^>]*> addiu	v0,a0,1
0+00b0 <[^>]*> beq	v0,v1,00000000 <foo>
0+00b4 <[^>]*> nop
0+00b8 <[^>]*> addiu	v0,a0,1
0+00bc <[^>]*> bne	v0,v1,000000d4 <foo\+0xd4>
0+00c0 <[^>]*> nop
0+00c4 <[^>]*> lw	at,2\(gp\)
[ 	]*c4: R_MIPS_GOT16	\.text
0+00c8 <[^>]*> nop
0+00cc <[^>]*> addiu	at,at,1000
[ 	]*cc: R_MIPS_LO16	\.text
0+00d0 <[^>]*> jr	at
0+00d4 <[^>]*> nop
0+00d8 <[^>]*> beq	a0,a1,00000000 <foo>
0+00dc <[^>]*> addiu	v0,a0,1
0+00e0 <[^>]*> bne	a0,a1,000000f8 <foo\+0xf8>
0+00e4 <[^>]*> nop
0+00e8 <[^>]*> lw	at,2\(gp\)
[ 	]*e8: R_MIPS_GOT16	\.text
0+00ec <[^>]*> nop
0+00f0 <[^>]*> addiu	at,at,1000
[ 	]*f0: R_MIPS_LO16	\.text
0+00f4 <[^>]*> jr	at
0+00f8 <[^>]*> addiu	v0,a0,1
0+00fc <[^>]*> lw	v0,0\(a0\)
0+0100 <[^>]*> nop
0+0104 <[^>]*> beq	v0,v1,00000000 <foo>
0+0108 <[^>]*> nop
0+010c <[^>]*> lw	v0,0\(a0\)
0+0110 <[^>]*> nop
0+0114 <[^>]*> bne	v0,v1,0000012c <foo\+0x12c>
0+0118 <[^>]*> nop
0+011c <[^>]*> lw	at,2\(gp\)
[ 	]*11c: R_MIPS_GOT16	\.text
0+0120 <[^>]*> nop
0+0124 <[^>]*> addiu	at,at,1000
[ 	]*124: R_MIPS_LO16	\.text
0+0128 <[^>]*> jr	at
0+012c <[^>]*> nop
0+0130 <[^>]*> lw	v0,0\(a0\)
0+0134 <[^>]*> beq	a0,a1,00000000 <foo>
0+0138 <[^>]*> nop
0+013c <[^>]*> lw	v0,0\(a0\)
0+0140 <[^>]*> bne	a0,a1,00000158 <foo\+0x158>
0+0144 <[^>]*> nop
0+0148 <[^>]*> lw	at,2\(gp\)
[ 	]*148: R_MIPS_GOT16	\.text
0+014c <[^>]*> nop
0+0150 <[^>]*> addiu	at,at,1000
[ 	]*150: R_MIPS_LO16	\.text
0+0154 <[^>]*> jr	at
0+0158 <[^>]*> nop
0+015c <[^>]*> beq	v0,v1,00000000 <foo>
0+0160 <[^>]*> sw	v0,0\(a0\)
0+0164 <[^>]*> bne	v0,v1,0000017c <foo\+0x17c>
0+0168 <[^>]*> nop
0+016c <[^>]*> lw	at,2\(gp\)
[ 	]*16c: R_MIPS_GOT16	\.text
0+0170 <[^>]*> nop
0+0174 <[^>]*> addiu	at,at,1000
[ 	]*174: R_MIPS_LO16	\.text
0+0178 <[^>]*> jr	at
0+017c <[^>]*> sw	v0,0\(a0\)
0+0180 <[^>]*> beq	a0,a1,00000000 <foo>
0+0184 <[^>]*> sw	v0,0\(a0\)
0+0188 <[^>]*> bne	a0,a1,000001a0 <foo\+0x1a0>
0+018c <[^>]*> nop
0+0190 <[^>]*> lw	at,2\(gp\)
[ 	]*190: R_MIPS_GOT16	\.text
0+0194 <[^>]*> nop
0+0198 <[^>]*> addiu	at,at,1000
[ 	]*198: R_MIPS_LO16	\.text
0+019c <[^>]*> jr	at
0+01a0 <[^>]*> sw	v0,0\(a0\)
0+01a4 <[^>]*> mfc1	v0,\$f0
0+01a8 <[^>]*> move	a2,a3
0+01ac <[^>]*> beq	v0,v1,00000000 <foo>
0+01b0 <[^>]*> nop
0+01b4 <[^>]*> mfc1	v0,\$f0
0+01b8 <[^>]*> move	a2,a3
0+01bc <[^>]*> bne	v0,v1,000001d4 <foo\+0x1d4>
0+01c0 <[^>]*> nop
0+01c4 <[^>]*> lw	at,2\(gp\)
[ 	]*1c4: R_MIPS_GOT16	\.text
0+01c8 <[^>]*> nop
0+01cc <[^>]*> addiu	at,at,1000
[ 	]*1cc: R_MIPS_LO16	\.text
0+01d0 <[^>]*> jr	at
0+01d4 <[^>]*> nop
0+01d8 <[^>]*> mfc1	v0,\$f0
0+01dc <[^>]*> beq	a0,a1,00000000 <foo>
0+01e0 <[^>]*> move	a2,a3
0+01e4 <[^>]*> mfc1	v0,\$f0
0+01e8 <[^>]*> bne	a0,a1,00000200 <foo\+0x200>
0+01ec <[^>]*> nop
0+01f0 <[^>]*> lw	at,2\(gp\)
[ 	]*1f0: R_MIPS_GOT16	\.text
0+01f4 <[^>]*> nop
0+01f8 <[^>]*> addiu	at,at,1000
[ 	]*1f8: R_MIPS_LO16	\.text
0+01fc <[^>]*> jr	at
0+0200 <[^>]*> move	a2,a3
0+0204 <[^>]*> move	v0,a0
0+0208 <[^>]*> bc1t	00000000 <foo>
0+020c <[^>]*> nop
0+0210 <[^>]*> move	v0,a0
0+0214 <[^>]*> bc1f	0000022c <foo\+0x22c>
0+0218 <[^>]*> nop
0+021c <[^>]*> lw	at,2\(gp\)
[ 	]*21c: R_MIPS_GOT16	\.text
0+0220 <[^>]*> nop
0+0224 <[^>]*> addiu	at,at,1000
[ 	]*224: R_MIPS_LO16	\.text
0+0228 <[^>]*> jr	at
0+022c <[^>]*> nop
0+0230 <[^>]*> move	v0,a0
0+0234 <[^>]*> b	00000000 <foo>
0+0238 <[^>]*> nop
0+023c <[^>]*> move	v0,a0
0+0240 <[^>]*> lw	at,2\(gp\)
[ 	]*240: R_MIPS_GOT16	\.text
0+0244 <[^>]*> nop
0+0248 <[^>]*> addiu	at,at,1000
[ 	]*248: R_MIPS_LO16	\.text
0+024c <[^>]*> jr	at
0+0250 <[^>]*> nop
0+0254 <[^>]*> move	v0,a0
0+0258 <[^>]*> b	00000000 <foo>
0+025c <[^>]*> nop
0+0260 <[^>]*> move	v0,a0
0+0264 <[^>]*> lw	at,2\(gp\)
[ 	]*264: R_MIPS_GOT16	\.text
0+0268 <[^>]*> nop
0+026c <[^>]*> addiu	at,at,1000
[ 	]*26c: R_MIPS_LO16	\.text
0+0270 <[^>]*> jr	at
0+0274 <[^>]*> nop
0+0278 <[^>]*> move	a2,a3
0+027c <[^>]*> move	v0,a0
0+0280 <[^>]*> b	00000000 <foo>
0+0284 <[^>]*> nop
0+0288 <[^>]*> move	a2,a3
0+028c <[^>]*> move	v0,a0
0+0290 <[^>]*> lw	at,2\(gp\)
[ 	]*290: R_MIPS_GOT16	\.text
0+0294 <[^>]*> nop
0+0298 <[^>]*> addiu	at,at,1000
[ 	]*298: R_MIPS_LO16	\.text
0+029c <[^>]*> jr	at
0+02a0 <[^>]*> nop
0+02a4 <[^>]*> lw	at,0\(gp\)
[ 	]*2a4: R_MIPS_GOT16	\.text
0+02a8 <[^>]*> nop
0+02ac <[^>]*> addiu	at,at,692
[ 	]*2ac: R_MIPS_LO16	\.text
0+02b0 <[^>]*> sw	v0,0\(at\)
0+02b4 <[^>]*> b	00000000 <foo>
0+02b8 <[^>]*> nop
0+02bc <[^>]*> lw	at,0\(gp\)
[ 	]*2bc: R_MIPS_GOT16	\.text
0+02c0 <[^>]*> nop
0+02c4 <[^>]*> addiu	at,at,716
[ 	]*2c4: R_MIPS_LO16	\.text
0+02c8 <[^>]*> sw	v0,0\(at\)
0+02cc <[^>]*> lw	at,2\(gp\)
[ 	]*2cc: R_MIPS_GOT16	\.text
0+02d0 <[^>]*> nop
0+02d4 <[^>]*> addiu	at,at,1000
[ 	]*2d4: R_MIPS_LO16	\.text
0+02d8 <[^>]*> jr	at
0+02dc <[^>]*> nop
0+02e0 <[^>]*> lwc1	\$f0,0\(a0\)
0+02e4 <[^>]*> b	00000000 <foo>
0+02e8 <[^>]*> nop
0+02ec <[^>]*> lwc1	\$f0,0\(a0\)
0+02f0 <[^>]*> lw	at,2\(gp\)
[ 	]*2f0: R_MIPS_GOT16	\.text
0+02f4 <[^>]*> nop
0+02f8 <[^>]*> addiu	at,at,1000
[ 	]*2f8: R_MIPS_LO16	\.text
0+02fc <[^>]*> jr	at
0+0300 <[^>]*> nop
0+0304 <[^>]*> cfc1	v0,\$31
0+0308 <[^>]*> b	00000000 <foo>
0+030c <[^>]*> nop
0+0310 <[^>]*> cfc1	v0,\$31
0+0314 <[^>]*> lw	at,2\(gp\)
[ 	]*314: R_MIPS_GOT16	\.text
0+0318 <[^>]*> nop
0+031c <[^>]*> addiu	at,at,1000
[ 	]*31c: R_MIPS_LO16	\.text
0+0320 <[^>]*> jr	at
0+0324 <[^>]*> nop
0+0328 <[^>]*> ctc1	v0,\$31
0+032c <[^>]*> b	00000000 <foo>
0+0330 <[^>]*> nop
0+0334 <[^>]*> ctc1	v0,\$31
0+0338 <[^>]*> lw	at,2\(gp\)
[ 	]*338: R_MIPS_GOT16	\.text
0+033c <[^>]*> nop
0+0340 <[^>]*> addiu	at,at,1000
[ 	]*340: R_MIPS_LO16	\.text
0+0344 <[^>]*> jr	at
0+0348 <[^>]*> nop
0+034c <[^>]*> mtc1	v0,\$f31
0+0350 <[^>]*> b	00000000 <foo>
0+0354 <[^>]*> nop
0+0358 <[^>]*> mtc1	v0,\$f31
0+035c <[^>]*> lw	at,2\(gp\)
[ 	]*35c: R_MIPS_GOT16	\.text
0+0360 <[^>]*> nop
0+0364 <[^>]*> addiu	at,at,1000
[ 	]*364: R_MIPS_LO16	\.text
0+0368 <[^>]*> jr	at
0+036c <[^>]*> nop
0+0370 <[^>]*> mfhi	v0
0+0374 <[^>]*> b	00000000 <foo>
0+0378 <[^>]*> nop
0+037c <[^>]*> mfhi	v0
0+0380 <[^>]*> lw	at,2\(gp\)
[ 	]*380: R_MIPS_GOT16	\.text
0+0384 <[^>]*> nop
0+0388 <[^>]*> addiu	at,at,1000
[ 	]*388: R_MIPS_LO16	\.text
0+038c <[^>]*> jr	at
0+0390 <[^>]*> nop
0+0394 <[^>]*> move	v0,a0
0+0398 <[^>]*> jr	v0
0+039c <[^>]*> nop
0+03a0 <[^>]*> jr	a0
0+03a4 <[^>]*> move	v0,a0
0+03a8 <[^>]*> move	v0,a0
0+03ac <[^>]*> jalr	v0
0+03b0 <[^>]*> nop
0+03b4 <[^>]*> jalr	a0
0+03b8 <[^>]*> move	v0,a0
0+03bc <[^>]*> move	v0,ra
0+03c0 <[^>]*> jalr	v1
0+03c4 <[^>]*> nop
0+03c8 <[^>]*> move	ra,a0
0+03cc <[^>]*> jalr	a1
0+03d0 <[^>]*> nop
0+03d4 <[^>]*> jalr	v0,v1
0+03d8 <[^>]*> move	ra,a0
0+03dc <[^>]*> move	v0,ra
0+03e0 <[^>]*> jalr	v0,v1
0+03e4 <[^>]*> nop
	\.\.\.
	\.\.\.
