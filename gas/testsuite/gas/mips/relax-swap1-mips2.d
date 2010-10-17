#objdump: -dr --prefix-addresses -mmips:6000
#name: MIPS2 branch relaxation with swapping
#as: -32 -mips2 -KPIC -relax-branch
#source: relax-swap1.s
#stderr: relax-swap1.l

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> b	00000000 <foo>
0+0004 <[^>]*> move	v0,a0
0+0008 <[^>]*> lw	at,2\(gp\)
[ 	]*8: R_MIPS_GOT16	\.text
0+000c <[^>]*> addiu	at,at,876
[ 	]*c: R_MIPS_LO16	\.text
0+0010 <[^>]*> jr	at
0+0014 <[^>]*> move	v0,a0
0+0018 <[^>]*> b	00000000 <foo>
0+001c <[^>]*> lw	v0,0\(a0\)
0+0020 <[^>]*> lw	at,2\(gp\)
[ 	]*20: R_MIPS_GOT16	\.text
0+0024 <[^>]*> addiu	at,at,876
[ 	]*24: R_MIPS_LO16	\.text
0+0028 <[^>]*> jr	at
0+002c <[^>]*> lw	v0,0\(a0\)
0+0030 <[^>]*> b	00000000 <foo>
0+0034 <[^>]*> sw	v0,0\(a0\)
0+0038 <[^>]*> lw	at,2\(gp\)
[ 	]*38: R_MIPS_GOT16	\.text
0+003c <[^>]*> addiu	at,at,876
[ 	]*3c: R_MIPS_LO16	\.text
0+0040 <[^>]*> jr	at
0+0044 <[^>]*> sw	v0,0\(a0\)
0+0048 <[^>]*> move	v0,a0
0+004c <[^>]*> beq	v0,v1,00000000 <foo>
0+0050 <[^>]*> nop
0+0054 <[^>]*> move	v0,a0
0+0058 <[^>]*> bne	v0,v1,0000006c <foo\+0x6c>
0+005c <[^>]*> nop
0+0060 <[^>]*> lw	at,2\(gp\)
[ 	]*60: R_MIPS_GOT16	\.text
0+0064 <[^>]*> addiu	at,at,876
[ 	]*64: R_MIPS_LO16	\.text
0+0068 <[^>]*> jr	at
0+006c <[^>]*> nop
0+0070 <[^>]*> beq	a0,a1,00000000 <foo>
0+0074 <[^>]*> move	v0,a0
0+0078 <[^>]*> bne	a0,a1,0000008c <foo\+0x8c>
0+007c <[^>]*> nop
0+0080 <[^>]*> lw	at,2\(gp\)
[ 	]*80: R_MIPS_GOT16	\.text
0+0084 <[^>]*> addiu	at,at,876
[ 	]*84: R_MIPS_LO16	\.text
0+0088 <[^>]*> jr	at
0+008c <[^>]*> move	v0,a0
0+0090 <[^>]*> addiu	v0,a0,1
0+0094 <[^>]*> beq	v0,v1,00000000 <foo>
0+0098 <[^>]*> nop
0+009c <[^>]*> addiu	v0,a0,1
0+00a0 <[^>]*> bne	v0,v1,000000b4 <foo\+0xb4>
0+00a4 <[^>]*> nop
0+00a8 <[^>]*> lw	at,2\(gp\)
[ 	]*a8: R_MIPS_GOT16	\.text
0+00ac <[^>]*> addiu	at,at,876
[ 	]*ac: R_MIPS_LO16	\.text
0+00b0 <[^>]*> jr	at
0+00b4 <[^>]*> nop
0+00b8 <[^>]*> beq	a0,a1,00000000 <foo>
0+00bc <[^>]*> addiu	v0,a0,1
0+00c0 <[^>]*> bne	a0,a1,000000d4 <foo\+0xd4>
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> lw	at,2\(gp\)
[ 	]*c8: R_MIPS_GOT16	\.text
0+00cc <[^>]*> addiu	at,at,876
[ 	]*cc: R_MIPS_LO16	\.text
0+00d0 <[^>]*> jr	at
0+00d4 <[^>]*> addiu	v0,a0,1
0+00d8 <[^>]*> lw	v0,0\(a0\)
0+00dc <[^>]*> beq	v0,v1,00000000 <foo>
0+00e0 <[^>]*> nop
0+00e4 <[^>]*> lw	v0,0\(a0\)
0+00e8 <[^>]*> bne	v0,v1,000000fc <foo\+0xfc>
0+00ec <[^>]*> nop
0+00f0 <[^>]*> lw	at,2\(gp\)
[ 	]*f0: R_MIPS_GOT16	\.text
0+00f4 <[^>]*> addiu	at,at,876
[ 	]*f4: R_MIPS_LO16	\.text
0+00f8 <[^>]*> jr	at
0+00fc <[^>]*> nop
0+0100 <[^>]*> beq	a0,a1,00000000 <foo>
0+0104 <[^>]*> lw	v0,0\(a0\)
0+0108 <[^>]*> bne	a0,a1,0000011c <foo\+0x11c>
0+010c <[^>]*> nop
0+0110 <[^>]*> lw	at,2\(gp\)
[ 	]*110: R_MIPS_GOT16	\.text
0+0114 <[^>]*> addiu	at,at,876
[ 	]*114: R_MIPS_LO16	\.text
0+0118 <[^>]*> jr	at
0+011c <[^>]*> lw	v0,0\(a0\)
0+0120 <[^>]*> beq	v0,v1,00000000 <foo>
0+0124 <[^>]*> sw	v0,0\(a0\)
0+0128 <[^>]*> bne	v0,v1,0000013c <foo\+0x13c>
0+012c <[^>]*> nop
0+0130 <[^>]*> lw	at,2\(gp\)
[ 	]*130: R_MIPS_GOT16	\.text
0+0134 <[^>]*> addiu	at,at,876
[ 	]*134: R_MIPS_LO16	\.text
0+0138 <[^>]*> jr	at
0+013c <[^>]*> sw	v0,0\(a0\)
0+0140 <[^>]*> beq	a0,a1,00000000 <foo>
0+0144 <[^>]*> sw	v0,0\(a0\)
0+0148 <[^>]*> bne	a0,a1,0000015c <foo\+0x15c>
0+014c <[^>]*> nop
0+0150 <[^>]*> lw	at,2\(gp\)
[ 	]*150: R_MIPS_GOT16	\.text
0+0154 <[^>]*> addiu	at,at,876
[ 	]*154: R_MIPS_LO16	\.text
0+0158 <[^>]*> jr	at
0+015c <[^>]*> sw	v0,0\(a0\)
0+0160 <[^>]*> mfc1	v0,\$f0
0+0164 <[^>]*> move	a2,a3
0+0168 <[^>]*> beq	v0,v1,00000000 <foo>
0+016c <[^>]*> nop
0+0170 <[^>]*> mfc1	v0,\$f0
0+0174 <[^>]*> move	a2,a3
0+0178 <[^>]*> bne	v0,v1,0000018c <foo\+0x18c>
0+017c <[^>]*> nop
0+0180 <[^>]*> lw	at,2\(gp\)
[ 	]*180: R_MIPS_GOT16	\.text
0+0184 <[^>]*> addiu	at,at,876
[ 	]*184: R_MIPS_LO16	\.text
0+0188 <[^>]*> jr	at
0+018c <[^>]*> nop
0+0190 <[^>]*> mfc1	v0,\$f0
0+0194 <[^>]*> beq	a0,a1,00000000 <foo>
0+0198 <[^>]*> move	a2,a3
0+019c <[^>]*> mfc1	v0,\$f0
0+01a0 <[^>]*> bne	a0,a1,000001b4 <foo\+0x1b4>
0+01a4 <[^>]*> nop
0+01a8 <[^>]*> lw	at,2\(gp\)
[ 	]*1a8: R_MIPS_GOT16	\.text
0+01ac <[^>]*> addiu	at,at,876
[ 	]*1ac: R_MIPS_LO16	\.text
0+01b0 <[^>]*> jr	at
0+01b4 <[^>]*> move	a2,a3
0+01b8 <[^>]*> move	v0,a0
0+01bc <[^>]*> bc1t	00000000 <foo>
0+01c0 <[^>]*> nop
0+01c4 <[^>]*> move	v0,a0
0+01c8 <[^>]*> bc1f	000001dc <foo\+0x1dc>
0+01cc <[^>]*> nop
0+01d0 <[^>]*> lw	at,2\(gp\)
[ 	]*1d0: R_MIPS_GOT16	\.text
0+01d4 <[^>]*> addiu	at,at,876
[ 	]*1d4: R_MIPS_LO16	\.text
0+01d8 <[^>]*> jr	at
0+01dc <[^>]*> nop
0+01e0 <[^>]*> move	v0,a0
0+01e4 <[^>]*> b	00000000 <foo>
0+01e8 <[^>]*> nop
0+01ec <[^>]*> move	v0,a0
0+01f0 <[^>]*> lw	at,2\(gp\)
[ 	]*1f0: R_MIPS_GOT16	\.text
0+01f4 <[^>]*> addiu	at,at,876
[ 	]*1f4: R_MIPS_LO16	\.text
0+01f8 <[^>]*> jr	at
0+01fc <[^>]*> nop
0+0200 <[^>]*> move	v0,a0
0+0204 <[^>]*> b	00000000 <foo>
0+0208 <[^>]*> nop
0+020c <[^>]*> move	v0,a0
0+0210 <[^>]*> lw	at,2\(gp\)
[ 	]*210: R_MIPS_GOT16	\.text
0+0214 <[^>]*> addiu	at,at,876
[ 	]*214: R_MIPS_LO16	\.text
0+0218 <[^>]*> jr	at
0+021c <[^>]*> nop
0+0220 <[^>]*> move	a2,a3
0+0224 <[^>]*> move	v0,a0
0+0228 <[^>]*> b	00000000 <foo>
0+022c <[^>]*> nop
0+0230 <[^>]*> move	a2,a3
0+0234 <[^>]*> move	v0,a0
0+0238 <[^>]*> lw	at,2\(gp\)
[ 	]*238: R_MIPS_GOT16	\.text
0+023c <[^>]*> addiu	at,at,876
[ 	]*23c: R_MIPS_LO16	\.text
0+0240 <[^>]*> jr	at
0+0244 <[^>]*> nop
0+0248 <[^>]*> lw	at,0\(gp\)
[ 	]*248: R_MIPS_GOT16	\.text
0+024c <[^>]*> nop
0+0250 <[^>]*> addiu	at,at,600
[ 	]*250: R_MIPS_LO16	\.text
0+0254 <[^>]*> sw	v0,0\(at\)
0+0258 <[^>]*> b	00000000 <foo>
0+025c <[^>]*> nop
0+0260 <[^>]*> lw	at,0\(gp\)
[ 	]*260: R_MIPS_GOT16	\.text
0+0264 <[^>]*> nop
0+0268 <[^>]*> addiu	at,at,624
[ 	]*268: R_MIPS_LO16	\.text
0+026c <[^>]*> sw	v0,0\(at\)
0+0270 <[^>]*> lw	at,2\(gp\)
[ 	]*270: R_MIPS_GOT16	\.text
0+0274 <[^>]*> addiu	at,at,876
[ 	]*274: R_MIPS_LO16	\.text
0+0278 <[^>]*> jr	at
0+027c <[^>]*> nop
0+0280 <[^>]*> b	00000000 <foo>
0+0284 <[^>]*> lwc1	\$f0,0\(a0\)
0+0288 <[^>]*> lw	at,2\(gp\)
[ 	]*288: R_MIPS_GOT16	\.text
0+028c <[^>]*> addiu	at,at,876
[ 	]*28c: R_MIPS_LO16	\.text
0+0290 <[^>]*> jr	at
0+0294 <[^>]*> lwc1	\$f0,0\(a0\)
0+0298 <[^>]*> cfc1	v0,\$31
0+029c <[^>]*> b	00000000 <foo>
0+02a0 <[^>]*> nop
0+02a4 <[^>]*> cfc1	v0,\$31
0+02a8 <[^>]*> lw	at,2\(gp\)
[ 	]*2a8: R_MIPS_GOT16	\.text
0+02ac <[^>]*> addiu	at,at,876
[ 	]*2ac: R_MIPS_LO16	\.text
0+02b0 <[^>]*> jr	at
0+02b4 <[^>]*> nop
0+02b8 <[^>]*> ctc1	v0,\$31
0+02bc <[^>]*> b	00000000 <foo>
0+02c0 <[^>]*> nop
0+02c4 <[^>]*> ctc1	v0,\$31
0+02c8 <[^>]*> lw	at,2\(gp\)
[ 	]*2c8: R_MIPS_GOT16	\.text
0+02cc <[^>]*> addiu	at,at,876
[ 	]*2cc: R_MIPS_LO16	\.text
0+02d0 <[^>]*> jr	at
0+02d4 <[^>]*> nop
0+02d8 <[^>]*> mtc1	v0,\$f31
0+02dc <[^>]*> b	00000000 <foo>
0+02e0 <[^>]*> nop
0+02e4 <[^>]*> mtc1	v0,\$f31
0+02e8 <[^>]*> lw	at,2\(gp\)
[ 	]*2e8: R_MIPS_GOT16	\.text
0+02ec <[^>]*> addiu	at,at,876
[ 	]*2ec: R_MIPS_LO16	\.text
0+02f0 <[^>]*> jr	at
0+02f4 <[^>]*> nop
0+02f8 <[^>]*> mfhi	v0
0+02fc <[^>]*> b	00000000 <foo>
0+0300 <[^>]*> nop
0+0304 <[^>]*> mfhi	v0
0+0308 <[^>]*> lw	at,2\(gp\)
[ 	]*308: R_MIPS_GOT16	\.text
0+030c <[^>]*> addiu	at,at,876
[ 	]*30c: R_MIPS_LO16	\.text
0+0310 <[^>]*> jr	at
0+0314 <[^>]*> nop
0+0318 <[^>]*> move	v0,a0
0+031c <[^>]*> jr	v0
0+0320 <[^>]*> nop
0+0324 <[^>]*> jr	a0
0+0328 <[^>]*> move	v0,a0
0+032c <[^>]*> move	v0,a0
0+0330 <[^>]*> jalr	v0
0+0334 <[^>]*> nop
0+0338 <[^>]*> jalr	a0
0+033c <[^>]*> move	v0,a0
0+0340 <[^>]*> move	v0,ra
0+0344 <[^>]*> jalr	v1
0+0348 <[^>]*> nop
0+034c <[^>]*> move	ra,a0
0+0350 <[^>]*> jalr	a1
0+0354 <[^>]*> nop
0+0358 <[^>]*> jalr	v0,v1
0+035c <[^>]*> move	ra,a0
0+0360 <[^>]*> move	v0,ra
0+0364 <[^>]*> jalr	v0,v1
0+0368 <[^>]*> nop
	\.\.\.
	\.\.\.
