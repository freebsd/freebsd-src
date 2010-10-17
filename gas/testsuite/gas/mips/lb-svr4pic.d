#objdump: -dr --prefix-addresses
#name: MIPS lb-svr4pic
#as: -32 -KPIC
#source: lb-pic.s

# Test the lb macro with -KPIC.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lb	a0,0\(zero\)
0+0004 <[^>]*> lb	a0,1\(zero\)
0+0008 <[^>]*> lui	a0,0x1
0+000c <[^>]*> lb	a0,-32768\(a0\)
0+0010 <[^>]*> lb	a0,-32768\(zero\)
0+0014 <[^>]*> lui	a0,0x1
0+0018 <[^>]*> lb	a0,0\(a0\)
0+001c <[^>]*> lui	a0,0x2
0+0020 <[^>]*> lb	a0,-23131\(a0\)
0+0024 <[^>]*> lb	a0,0\(a1\)
0+0028 <[^>]*> lb	a0,1\(a1\)
0+002c <[^>]*> lui	a0,0x1
0+0030 <[^>]*> addu	a0,a0,a1
0+0034 <[^>]*> lb	a0,-32768\(a0\)
0+0038 <[^>]*> lb	a0,-32768\(a1\)
0+003c <[^>]*> lui	a0,0x1
0+0040 <[^>]*> addu	a0,a0,a1
0+0044 <[^>]*> lb	a0,0\(a0\)
0+0048 <[^>]*> lui	a0,0x2
0+004c <[^>]*> addu	a0,a0,a1
0+0050 <[^>]*> lb	a0,-23131\(a0\)
0+0054 <[^>]*> lw	a0,0\(gp\)
[ 	]*54: R_MIPS_GOT16	.data
0+0058 <[^>]*> nop
0+005c <[^>]*> addiu	a0,a0,0
[ 	]*5c: R_MIPS_LO16	.data
0+0060 <[^>]*> lb	a0,0\(a0\)
0+0064 <[^>]*> lw	a0,0\(gp\)
[ 	]*64: R_MIPS_GOT16	big_external_data_label
0+0068 <[^>]*> nop
0+006c <[^>]*> lb	a0,0\(a0\)
0+0070 <[^>]*> lw	a0,0\(gp\)
[ 	]*70: R_MIPS_GOT16	small_external_data_label
0+0074 <[^>]*> nop
0+0078 <[^>]*> lb	a0,0\(a0\)
0+007c <[^>]*> lw	a0,0\(gp\)
[ 	]*7c: R_MIPS_GOT16	big_external_common
0+0080 <[^>]*> nop
0+0084 <[^>]*> lb	a0,0\(a0\)
0+0088 <[^>]*> lw	a0,0\(gp\)
[ 	]*88: R_MIPS_GOT16	small_external_common
0+008c <[^>]*> nop
0+0090 <[^>]*> lb	a0,0\(a0\)
0+0094 <[^>]*> lw	a0,0\(gp\)
[ 	]*94: R_MIPS_GOT16	.bss
0+0098 <[^>]*> nop
0+009c <[^>]*> addiu	a0,a0,0
[ 	]*9c: R_MIPS_LO16	.bss
0+00a0 <[^>]*> lb	a0,0\(a0\)
0+00a4 <[^>]*> lw	a0,0\(gp\)
[ 	]*a4: R_MIPS_GOT16	.bss
0+00a8 <[^>]*> nop
0+00ac <[^>]*> addiu	a0,a0,1000
[ 	]*ac: R_MIPS_LO16	.bss
0+00b0 <[^>]*> lb	a0,0\(a0\)
0+00b4 <[^>]*> lw	a0,0\(gp\)
[ 	]*b4: R_MIPS_GOT16	.data
0+00b8 <[^>]*> nop
0+00bc <[^>]*> addiu	a0,a0,0
[ 	]*bc: R_MIPS_LO16	.data
0+00c0 <[^>]*> lb	a0,1\(a0\)
0+00c4 <[^>]*> lw	a0,0\(gp\)
[ 	]*c4: R_MIPS_GOT16	big_external_data_label
0+00c8 <[^>]*> nop
0+00cc <[^>]*> lb	a0,1\(a0\)
0+00d0 <[^>]*> lw	a0,0\(gp\)
[ 	]*d0: R_MIPS_GOT16	small_external_data_label
0+00d4 <[^>]*> nop
0+00d8 <[^>]*> lb	a0,1\(a0\)
0+00dc <[^>]*> lw	a0,0\(gp\)
[ 	]*dc: R_MIPS_GOT16	big_external_common
0+00e0 <[^>]*> nop
0+00e4 <[^>]*> lb	a0,1\(a0\)
0+00e8 <[^>]*> lw	a0,0\(gp\)
[ 	]*e8: R_MIPS_GOT16	small_external_common
0+00ec <[^>]*> nop
0+00f0 <[^>]*> lb	a0,1\(a0\)
0+00f4 <[^>]*> lw	a0,0\(gp\)
[ 	]*f4: R_MIPS_GOT16	.bss
0+00f8 <[^>]*> nop
0+00fc <[^>]*> addiu	a0,a0,0
[ 	]*fc: R_MIPS_LO16	.bss
0+0100 <[^>]*> lb	a0,1\(a0\)
0+0104 <[^>]*> lw	a0,0\(gp\)
[ 	]*104: R_MIPS_GOT16	.bss
0+0108 <[^>]*> nop
0+010c <[^>]*> addiu	a0,a0,1000
[ 	]*10c: R_MIPS_LO16	.bss
0+0110 <[^>]*> lb	a0,1\(a0\)
0+0114 <[^>]*> lw	a0,0\(gp\)
[ 	]*114: R_MIPS_GOT16	.data
0+0118 <[^>]*> nop
0+011c <[^>]*> addiu	a0,a0,0
[ 	]*11c: R_MIPS_LO16	.data
0+0120 <[^>]*> addu	a0,a0,a1
0+0124 <[^>]*> lb	a0,0\(a0\)
0+0128 <[^>]*> lw	a0,0\(gp\)
[ 	]*128: R_MIPS_GOT16	big_external_data_label
0+012c <[^>]*> nop
0+0130 <[^>]*> addu	a0,a0,a1
0+0134 <[^>]*> lb	a0,0\(a0\)
0+0138 <[^>]*> lw	a0,0\(gp\)
[ 	]*138: R_MIPS_GOT16	small_external_data_label
0+013c <[^>]*> nop
0+0140 <[^>]*> addu	a0,a0,a1
0+0144 <[^>]*> lb	a0,0\(a0\)
0+0148 <[^>]*> lw	a0,0\(gp\)
[ 	]*148: R_MIPS_GOT16	big_external_common
0+014c <[^>]*> nop
0+0150 <[^>]*> addu	a0,a0,a1
0+0154 <[^>]*> lb	a0,0\(a0\)
0+0158 <[^>]*> lw	a0,0\(gp\)
[ 	]*158: R_MIPS_GOT16	small_external_common
0+015c <[^>]*> nop
0+0160 <[^>]*> addu	a0,a0,a1
0+0164 <[^>]*> lb	a0,0\(a0\)
0+0168 <[^>]*> lw	a0,0\(gp\)
[ 	]*168: R_MIPS_GOT16	.bss
0+016c <[^>]*> nop
0+0170 <[^>]*> addiu	a0,a0,0
[ 	]*170: R_MIPS_LO16	.bss
0+0174 <[^>]*> addu	a0,a0,a1
0+0178 <[^>]*> lb	a0,0\(a0\)
0+017c <[^>]*> lw	a0,0\(gp\)
[ 	]*17c: R_MIPS_GOT16	.bss
0+0180 <[^>]*> nop
0+0184 <[^>]*> addiu	a0,a0,1000
[ 	]*184: R_MIPS_LO16	.bss
0+0188 <[^>]*> addu	a0,a0,a1
0+018c <[^>]*> lb	a0,0\(a0\)
0+0190 <[^>]*> lw	a0,0\(gp\)
[ 	]*190: R_MIPS_GOT16	.data
0+0194 <[^>]*> nop
0+0198 <[^>]*> addiu	a0,a0,0
[ 	]*198: R_MIPS_LO16	.data
0+019c <[^>]*> addu	a0,a0,a1
0+01a0 <[^>]*> lb	a0,1\(a0\)
0+01a4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1a4: R_MIPS_GOT16	big_external_data_label
0+01a8 <[^>]*> nop
0+01ac <[^>]*> addu	a0,a0,a1
0+01b0 <[^>]*> lb	a0,1\(a0\)
0+01b4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1b4: R_MIPS_GOT16	small_external_data_label
0+01b8 <[^>]*> nop
0+01bc <[^>]*> addu	a0,a0,a1
0+01c0 <[^>]*> lb	a0,1\(a0\)
0+01c4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1c4: R_MIPS_GOT16	big_external_common
0+01c8 <[^>]*> nop
0+01cc <[^>]*> addu	a0,a0,a1
0+01d0 <[^>]*> lb	a0,1\(a0\)
0+01d4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1d4: R_MIPS_GOT16	small_external_common
0+01d8 <[^>]*> nop
0+01dc <[^>]*> addu	a0,a0,a1
0+01e0 <[^>]*> lb	a0,1\(a0\)
0+01e4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1e4: R_MIPS_GOT16	.bss
0+01e8 <[^>]*> nop
0+01ec <[^>]*> addiu	a0,a0,0
[ 	]*1ec: R_MIPS_LO16	.bss
0+01f0 <[^>]*> addu	a0,a0,a1
0+01f4 <[^>]*> lb	a0,1\(a0\)
0+01f8 <[^>]*> lw	a0,0\(gp\)
[ 	]*1f8: R_MIPS_GOT16	.bss
0+01fc <[^>]*> nop
0+0200 <[^>]*> addiu	a0,a0,1000
[ 	]*200: R_MIPS_LO16	.bss
0+0204 <[^>]*> addu	a0,a0,a1
0+0208 <[^>]*> lb	a0,1\(a0\)
0+020c <[^>]*> nop
