#objdump: -dr --prefix-addresses -mmips:6000
#name: MIPS2 branch likely relaxation with swapping
#as: -32 -mips2 -KPIC -relax-branch
#source: relax-swap2.s
#stderr: relax-swap2.l

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> move	v0,a0
0+0004 <[^>]*> beql	v0,v1,00000000 <foo>
0+0008 <[^>]*> nop
0+000c <[^>]*> move	v0,a0
0+0010 <[^>]*> beql	v0,v1,00000020 <foo\+0x20>
0+0014 <[^>]*> nop
0+0018 <[^>]*> beqzl	zero,00000030 <foo\+0x30>
0+001c <[^>]*> nop
0+0020 <[^>]*> lw	at,2\(gp\)
[ 	]*20: R_MIPS_GOT16	\.text
0+0024 <[^>]*> addiu	at,at,424
[ 	]*24: R_MIPS_LO16	\.text
0+0028 <[^>]*> jr	at
0+002c <[^>]*> nop
0+0030 <[^>]*> move	v0,a0
0+0034 <[^>]*> beql	a0,a1,00000000 <foo>
0+0038 <[^>]*> nop
0+003c <[^>]*> move	v0,a0
0+0040 <[^>]*> beql	a0,a1,00000050 <foo\+0x50>
0+0044 <[^>]*> nop
0+0048 <[^>]*> beqzl	zero,00000060 <foo\+0x60>
0+004c <[^>]*> nop
0+0050 <[^>]*> lw	at,2\(gp\)
[ 	]*50: R_MIPS_GOT16	\.text
0+0054 <[^>]*> addiu	at,at,424
[ 	]*54: R_MIPS_LO16	\.text
0+0058 <[^>]*> jr	at
0+005c <[^>]*> nop
0+0060 <[^>]*> addiu	v0,a0,1
0+0064 <[^>]*> beql	v0,v1,00000000 <foo>
0+0068 <[^>]*> nop
0+006c <[^>]*> addiu	v0,a0,1
0+0070 <[^>]*> beql	v0,v1,00000080 <foo\+0x80>
0+0074 <[^>]*> nop
0+0078 <[^>]*> beqzl	zero,00000090 <foo\+0x90>
0+007c <[^>]*> nop
0+0080 <[^>]*> lw	at,2\(gp\)
[ 	]*80: R_MIPS_GOT16	\.text
0+0084 <[^>]*> addiu	at,at,424
[ 	]*84: R_MIPS_LO16	\.text
0+0088 <[^>]*> jr	at
0+008c <[^>]*> nop
0+0090 <[^>]*> addiu	v0,a0,1
0+0094 <[^>]*> beql	a0,a1,00000000 <foo>
0+0098 <[^>]*> nop
0+009c <[^>]*> addiu	v0,a0,1
0+00a0 <[^>]*> beql	a0,a1,000000b0 <foo\+0xb0>
0+00a4 <[^>]*> nop
0+00a8 <[^>]*> beqzl	zero,000000c0 <foo\+0xc0>
0+00ac <[^>]*> nop
0+00b0 <[^>]*> lw	at,2\(gp\)
[ 	]*b0: R_MIPS_GOT16	\.text
0+00b4 <[^>]*> addiu	at,at,424
[ 	]*b4: R_MIPS_LO16	\.text
0+00b8 <[^>]*> jr	at
0+00bc <[^>]*> nop
0+00c0 <[^>]*> lw	v0,0\(a0\)
0+00c4 <[^>]*> beql	v0,v1,00000000 <foo>
0+00c8 <[^>]*> nop
0+00cc <[^>]*> lw	v0,0\(a0\)
0+00d0 <[^>]*> beql	v0,v1,000000e0 <foo\+0xe0>
0+00d4 <[^>]*> nop
0+00d8 <[^>]*> beqzl	zero,000000f0 <foo\+0xf0>
0+00dc <[^>]*> nop
0+00e0 <[^>]*> lw	at,2\(gp\)
[ 	]*e0: R_MIPS_GOT16	\.text
0+00e4 <[^>]*> addiu	at,at,424
[ 	]*e4: R_MIPS_LO16	\.text
0+00e8 <[^>]*> jr	at
0+00ec <[^>]*> nop
0+00f0 <[^>]*> lw	v0,0\(a0\)
0+00f4 <[^>]*> beql	a0,a1,00000000 <foo>
0+00f8 <[^>]*> nop
0+00fc <[^>]*> lw	v0,0\(a0\)
0+0100 <[^>]*> beql	a0,a1,00000110 <foo\+0x110>
0+0104 <[^>]*> nop
0+0108 <[^>]*> beqzl	zero,00000120 <foo\+0x120>
0+010c <[^>]*> nop
0+0110 <[^>]*> lw	at,2\(gp\)
[ 	]*110: R_MIPS_GOT16	\.text
0+0114 <[^>]*> addiu	at,at,424
[ 	]*114: R_MIPS_LO16	\.text
0+0118 <[^>]*> jr	at
0+011c <[^>]*> nop
0+0120 <[^>]*> sw	v0,0\(a0\)
0+0124 <[^>]*> beql	v0,v1,00000000 <foo>
0+0128 <[^>]*> nop
0+012c <[^>]*> sw	v0,0\(a0\)
0+0130 <[^>]*> beql	v0,v1,00000140 <foo\+0x140>
0+0134 <[^>]*> nop
0+0138 <[^>]*> beqzl	zero,00000150 <foo\+0x150>
0+013c <[^>]*> nop
0+0140 <[^>]*> lw	at,2\(gp\)
[ 	]*140: R_MIPS_GOT16	\.text
0+0144 <[^>]*> addiu	at,at,424
[ 	]*144: R_MIPS_LO16	\.text
0+0148 <[^>]*> jr	at
0+014c <[^>]*> nop
0+0150 <[^>]*> sw	v0,0\(a0\)
0+0154 <[^>]*> beql	a0,a1,00000000 <foo>
0+0158 <[^>]*> nop
0+015c <[^>]*> sw	v0,0\(a0\)
0+0160 <[^>]*> beql	a0,a1,00000170 <foo\+0x170>
0+0164 <[^>]*> nop
0+0168 <[^>]*> beqzl	zero,00000180 <foo\+0x180>
0+016c <[^>]*> nop
0+0170 <[^>]*> lw	at,2\(gp\)
[ 	]*170: R_MIPS_GOT16	\.text
0+0174 <[^>]*> addiu	at,at,424
[ 	]*174: R_MIPS_LO16	\.text
0+0178 <[^>]*> jr	at
0+017c <[^>]*> nop
0+0180 <[^>]*> teq	v0,a0
0+0184 <[^>]*> beq	a0,a1,00000000 <foo>
0+0188 <[^>]*> nop
0+018c <[^>]*> teq	v0,a0
0+0190 <[^>]*> bne	a0,a1,000001a4 <foo\+0x1a4>
0+0194 <[^>]*> nop
0+0198 <[^>]*> lw	at,2\(gp\)
[ 	]*198: R_MIPS_GOT16	\.text
0+019c <[^>]*> addiu	at,at,424
[ 	]*19c: R_MIPS_LO16	\.text
0+01a0 <[^>]*> jr	at
0+01a4 <[^>]*> nop
	\.\.\.
	\.\.\.
