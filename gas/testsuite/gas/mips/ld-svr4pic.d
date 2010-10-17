#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS ld-svr4pic
#as: -32 -mips1 -mtune=r3000 -KPIC
#source: ld-pic.s

# Test the ld macro with -KPIC.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw	a0,0\(zero\)
0+0004 <[^>]*> lw	a1,4\(zero\)
0+0008 <[^>]*> lw	a0,1\(zero\)
0+000c <[^>]*> lw	a1,5\(zero\)
0+0010 <[^>]*> lui	at,0x1
0+0014 <[^>]*> lw	a0,-32768\(at\)
0+0018 <[^>]*> lw	a1,-32764\(at\)
0+001c <[^>]*> lw	a0,-32768\(zero\)
0+0020 <[^>]*> lw	a1,-32764\(zero\)
0+0024 <[^>]*> lui	at,0x1
0+0028 <[^>]*> lw	a0,0\(at\)
0+002c <[^>]*> lw	a1,4\(at\)
0+0030 <[^>]*> lui	at,0x2
0+0034 <[^>]*> lw	a0,-23131\(at\)
0+0038 <[^>]*> lw	a1,-23127\(at\)
0+003c <[^>]*> nop
0+0040 <[^>]*> lw	a0,0\(a1\)
0+0044 <[^>]*> lw	a1,4\(a1\)
0+0048 <[^>]*> nop
0+004c <[^>]*> lw	a0,1\(a1\)
0+0050 <[^>]*> lw	a1,5\(a1\)
0+0054 <[^>]*> lui	at,0x1
0+0058 <[^>]*> addu	at,a1,at
0+005c <[^>]*> lw	a0,-32768\(at\)
0+0060 <[^>]*> lw	a1,-32764\(at\)
0+0064 <[^>]*> nop
0+0068 <[^>]*> lw	a0,-32768\(a1\)
0+006c <[^>]*> lw	a1,-32764\(a1\)
0+0070 <[^>]*> lui	at,0x1
0+0074 <[^>]*> addu	at,a1,at
0+0078 <[^>]*> lw	a0,0\(at\)
0+007c <[^>]*> lw	a1,4\(at\)
0+0080 <[^>]*> lui	at,0x2
0+0084 <[^>]*> addu	at,a1,at
0+0088 <[^>]*> lw	a0,-23131\(at\)
0+008c <[^>]*> lw	a1,-23127\(at\)
0+0090 <[^>]*> lw	at,0\(gp\)
[ 	]*90: R_MIPS_GOT16	.data
0+0094 <[^>]*> nop
0+0098 <[^>]*> lw	a0,0\(at\)
[ 	]*98: R_MIPS_LO16	.data
0+009c <[^>]*> lw	a1,4\(at\)
[ 	]*9c: R_MIPS_LO16	.data
0+00a0 <[^>]*> lw	at,0\(gp\)
[ 	]*a0: R_MIPS_GOT16	big_external_data_label
0+00a4 <[^>]*> nop
0+00a8 <[^>]*> lw	a0,0\(at\)
0+00ac <[^>]*> lw	a1,4\(at\)
0+00b0 <[^>]*> lw	at,0\(gp\)
[ 	]*b0: R_MIPS_GOT16	small_external_data_label
0+00b4 <[^>]*> nop
0+00b8 <[^>]*> lw	a0,0\(at\)
0+00bc <[^>]*> lw	a1,4\(at\)
0+00c0 <[^>]*> lw	at,0\(gp\)
[ 	]*c0: R_MIPS_GOT16	big_external_common
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> lw	a0,0\(at\)
0+00cc <[^>]*> lw	a1,4\(at\)
0+00d0 <[^>]*> lw	at,0\(gp\)
[ 	]*d0: R_MIPS_GOT16	small_external_common
0+00d4 <[^>]*> nop
0+00d8 <[^>]*> lw	a0,0\(at\)
0+00dc <[^>]*> lw	a1,4\(at\)
0+00e0 <[^>]*> lw	at,0\(gp\)
[ 	]*e0: R_MIPS_GOT16	.bss
0+00e4 <[^>]*> nop
0+00e8 <[^>]*> lw	a0,0\(at\)
[ 	]*e8: R_MIPS_LO16	.bss
0+00ec <[^>]*> lw	a1,4\(at\)
[ 	]*ec: R_MIPS_LO16	.bss
0+00f0 <[^>]*> lw	at,0\(gp\)
[ 	]*f0: R_MIPS_GOT16	.bss
0+00f4 <[^>]*> nop
0+00f8 <[^>]*> lw	a0,1000\(at\)
[ 	]*f8: R_MIPS_LO16	.bss
0+00fc <[^>]*> lw	a1,1004\(at\)
[ 	]*fc: R_MIPS_LO16	.bss
0+0100 <[^>]*> lw	at,0\(gp\)
[ 	]*100: R_MIPS_GOT16	.data
0+0104 <[^>]*> nop
0+0108 <[^>]*> lw	a0,1\(at\)
[ 	]*108: R_MIPS_LO16	.data
0+010c <[^>]*> lw	a1,5\(at\)
[ 	]*10c: R_MIPS_LO16	.data
0+0110 <[^>]*> lw	at,0\(gp\)
[ 	]*110: R_MIPS_GOT16	big_external_data_label
0+0114 <[^>]*> nop
0+0118 <[^>]*> lw	a0,1\(at\)
0+011c <[^>]*> lw	a1,5\(at\)
0+0120 <[^>]*> lw	at,0\(gp\)
[ 	]*120: R_MIPS_GOT16	small_external_data_label
0+0124 <[^>]*> nop
0+0128 <[^>]*> lw	a0,1\(at\)
0+012c <[^>]*> lw	a1,5\(at\)
0+0130 <[^>]*> lw	at,0\(gp\)
[ 	]*130: R_MIPS_GOT16	big_external_common
0+0134 <[^>]*> nop
0+0138 <[^>]*> lw	a0,1\(at\)
0+013c <[^>]*> lw	a1,5\(at\)
0+0140 <[^>]*> lw	at,0\(gp\)
[ 	]*140: R_MIPS_GOT16	small_external_common
0+0144 <[^>]*> nop
0+0148 <[^>]*> lw	a0,1\(at\)
0+014c <[^>]*> lw	a1,5\(at\)
0+0150 <[^>]*> lw	at,0\(gp\)
[ 	]*150: R_MIPS_GOT16	.bss
0+0154 <[^>]*> nop
0+0158 <[^>]*> lw	a0,1\(at\)
[ 	]*158: R_MIPS_LO16	.bss
0+015c <[^>]*> lw	a1,5\(at\)
[ 	]*15c: R_MIPS_LO16	.bss
0+0160 <[^>]*> lw	at,0\(gp\)
[ 	]*160: R_MIPS_GOT16	.bss
0+0164 <[^>]*> nop
0+0168 <[^>]*> lw	a0,1001\(at\)
[ 	]*168: R_MIPS_LO16	.bss
0+016c <[^>]*> lw	a1,1005\(at\)
[ 	]*16c: R_MIPS_LO16	.bss
0+0170 <[^>]*> lw	at,0\(gp\)
[ 	]*170: R_MIPS_GOT16	.data
0+0174 <[^>]*> nop
0+0178 <[^>]*> addu	at,a1,at
0+017c <[^>]*> lw	a0,0\(at\)
[ 	]*17c: R_MIPS_LO16	.data
0+0180 <[^>]*> lw	a1,4\(at\)
[ 	]*180: R_MIPS_LO16	.data
0+0184 <[^>]*> lw	at,0\(gp\)
[ 	]*184: R_MIPS_GOT16	big_external_data_label
0+0188 <[^>]*> nop
0+018c <[^>]*> addu	at,a1,at
0+0190 <[^>]*> lw	a0,0\(at\)
0+0194 <[^>]*> lw	a1,4\(at\)
0+0198 <[^>]*> lw	at,0\(gp\)
[ 	]*198: R_MIPS_GOT16	small_external_data_label
0+019c <[^>]*> nop
0+01a0 <[^>]*> addu	at,a1,at
0+01a4 <[^>]*> lw	a0,0\(at\)
0+01a8 <[^>]*> lw	a1,4\(at\)
0+01ac <[^>]*> lw	at,0\(gp\)
[ 	]*1ac: R_MIPS_GOT16	big_external_common
0+01b0 <[^>]*> nop
0+01b4 <[^>]*> addu	at,a1,at
0+01b8 <[^>]*> lw	a0,0\(at\)
0+01bc <[^>]*> lw	a1,4\(at\)
0+01c0 <[^>]*> lw	at,0\(gp\)
[ 	]*1c0: R_MIPS_GOT16	small_external_common
0+01c4 <[^>]*> nop
0+01c8 <[^>]*> addu	at,a1,at
0+01cc <[^>]*> lw	a0,0\(at\)
0+01d0 <[^>]*> lw	a1,4\(at\)
0+01d4 <[^>]*> lw	at,0\(gp\)
[ 	]*1d4: R_MIPS_GOT16	.bss
0+01d8 <[^>]*> nop
0+01dc <[^>]*> addu	at,a1,at
0+01e0 <[^>]*> lw	a0,0\(at\)
[ 	]*1e0: R_MIPS_LO16	.bss
0+01e4 <[^>]*> lw	a1,4\(at\)
[ 	]*1e4: R_MIPS_LO16	.bss
0+01e8 <[^>]*> lw	at,0\(gp\)
[ 	]*1e8: R_MIPS_GOT16	.bss
0+01ec <[^>]*> nop
0+01f0 <[^>]*> addu	at,a1,at
0+01f4 <[^>]*> lw	a0,1000\(at\)
[ 	]*1f4: R_MIPS_LO16	.bss
0+01f8 <[^>]*> lw	a1,1004\(at\)
[ 	]*1f8: R_MIPS_LO16	.bss
0+01fc <[^>]*> lw	at,0\(gp\)
[ 	]*1fc: R_MIPS_GOT16	.data
0+0200 <[^>]*> nop
0+0204 <[^>]*> addu	at,a1,at
0+0208 <[^>]*> lw	a0,1\(at\)
[ 	]*208: R_MIPS_LO16	.data
0+020c <[^>]*> lw	a1,5\(at\)
[ 	]*20c: R_MIPS_LO16	.data
0+0210 <[^>]*> lw	at,0\(gp\)
[ 	]*210: R_MIPS_GOT16	big_external_data_label
0+0214 <[^>]*> nop
0+0218 <[^>]*> addu	at,a1,at
0+021c <[^>]*> lw	a0,1\(at\)
0+0220 <[^>]*> lw	a1,5\(at\)
0+0224 <[^>]*> lw	at,0\(gp\)
[ 	]*224: R_MIPS_GOT16	small_external_data_label
0+0228 <[^>]*> nop
0+022c <[^>]*> addu	at,a1,at
0+0230 <[^>]*> lw	a0,1\(at\)
0+0234 <[^>]*> lw	a1,5\(at\)
0+0238 <[^>]*> lw	at,0\(gp\)
[ 	]*238: R_MIPS_GOT16	big_external_common
0+023c <[^>]*> nop
0+0240 <[^>]*> addu	at,a1,at
0+0244 <[^>]*> lw	a0,1\(at\)
0+0248 <[^>]*> lw	a1,5\(at\)
0+024c <[^>]*> lw	at,0\(gp\)
[ 	]*24c: R_MIPS_GOT16	small_external_common
0+0250 <[^>]*> nop
0+0254 <[^>]*> addu	at,a1,at
0+0258 <[^>]*> lw	a0,1\(at\)
0+025c <[^>]*> lw	a1,5\(at\)
0+0260 <[^>]*> lw	at,0\(gp\)
[ 	]*260: R_MIPS_GOT16	.bss
0+0264 <[^>]*> nop
0+0268 <[^>]*> addu	at,a1,at
0+026c <[^>]*> lw	a0,1\(at\)
[ 	]*26c: R_MIPS_LO16	.bss
0+0270 <[^>]*> lw	a1,5\(at\)
[ 	]*270: R_MIPS_LO16	.bss
0+0274 <[^>]*> lw	at,0\(gp\)
[ 	]*274: R_MIPS_GOT16	.bss
0+0278 <[^>]*> nop
0+027c <[^>]*> addu	at,a1,at
0+0280 <[^>]*> lw	a0,1001\(at\)
[ 	]*280: R_MIPS_LO16	.bss
0+0284 <[^>]*> lw	a1,1005\(at\)
[ 	]*284: R_MIPS_LO16	.bss
	...
