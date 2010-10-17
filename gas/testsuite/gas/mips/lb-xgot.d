#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS lb-xgot
#as: -32 -mips1 -KPIC -xgot -mtune=r3000
#source: lb-pic.s

# Test the lb macro with -KPIC -xgot.

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
0+0060 <[^>]*> nop
0+0064 <[^>]*> lb	a0,0\(a0\)
0+0068 <[^>]*> lui	a0,0x0
[ 	]*68: R_MIPS_GOT_HI16	big_external_data_label
0+006c <[^>]*> addu	a0,a0,gp
0+0070 <[^>]*> lw	a0,0\(a0\)
[ 	]*70: R_MIPS_GOT_LO16	big_external_data_label
0+0074 <[^>]*> nop
0+0078 <[^>]*> lb	a0,0\(a0\)
0+007c <[^>]*> lui	a0,0x0
[ 	]*7c: R_MIPS_GOT_HI16	small_external_data_label
0+0080 <[^>]*> addu	a0,a0,gp
0+0084 <[^>]*> lw	a0,0\(a0\)
[ 	]*84: R_MIPS_GOT_LO16	small_external_data_label
0+0088 <[^>]*> nop
0+008c <[^>]*> lb	a0,0\(a0\)
0+0090 <[^>]*> lui	a0,0x0
[ 	]*90: R_MIPS_GOT_HI16	big_external_common
0+0094 <[^>]*> addu	a0,a0,gp
0+0098 <[^>]*> lw	a0,0\(a0\)
[ 	]*98: R_MIPS_GOT_LO16	big_external_common
0+009c <[^>]*> nop
0+00a0 <[^>]*> lb	a0,0\(a0\)
0+00a4 <[^>]*> lui	a0,0x0
[ 	]*a4: R_MIPS_GOT_HI16	small_external_common
0+00a8 <[^>]*> addu	a0,a0,gp
0+00ac <[^>]*> lw	a0,0\(a0\)
[ 	]*ac: R_MIPS_GOT_LO16	small_external_common
0+00b0 <[^>]*> nop
0+00b4 <[^>]*> lb	a0,0\(a0\)
0+00b8 <[^>]*> lw	a0,0\(gp\)
[ 	]*b8: R_MIPS_GOT16	.bss
0+00bc <[^>]*> nop
0+00c0 <[^>]*> addiu	a0,a0,0
[ 	]*c0: R_MIPS_LO16	.bss
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> lb	a0,0\(a0\)
0+00cc <[^>]*> lw	a0,0\(gp\)
[ 	]*cc: R_MIPS_GOT16	.bss
0+00d0 <[^>]*> nop
0+00d4 <[^>]*> addiu	a0,a0,1000
[ 	]*d4: R_MIPS_LO16	.bss
0+00d8 <[^>]*> nop
0+00dc <[^>]*> lb	a0,0\(a0\)
0+00e0 <[^>]*> lw	a0,0\(gp\)
[ 	]*e0: R_MIPS_GOT16	.data
0+00e4 <[^>]*> nop
0+00e8 <[^>]*> addiu	a0,a0,0
[ 	]*e8: R_MIPS_LO16	.data
0+00ec <[^>]*> nop
0+00f0 <[^>]*> lb	a0,1\(a0\)
0+00f4 <[^>]*> lui	a0,0x0
[ 	]*f4: R_MIPS_GOT_HI16	big_external_data_label
0+00f8 <[^>]*> addu	a0,a0,gp
0+00fc <[^>]*> lw	a0,0\(a0\)
[ 	]*fc: R_MIPS_GOT_LO16	big_external_data_label
0+0100 <[^>]*> nop
0+0104 <[^>]*> lb	a0,1\(a0\)
0+0108 <[^>]*> lui	a0,0x0
[ 	]*108: R_MIPS_GOT_HI16	small_external_data_label
0+010c <[^>]*> addu	a0,a0,gp
0+0110 <[^>]*> lw	a0,0\(a0\)
[ 	]*110: R_MIPS_GOT_LO16	small_external_data_label
0+0114 <[^>]*> nop
0+0118 <[^>]*> lb	a0,1\(a0\)
0+011c <[^>]*> lui	a0,0x0
[ 	]*11c: R_MIPS_GOT_HI16	big_external_common
0+0120 <[^>]*> addu	a0,a0,gp
0+0124 <[^>]*> lw	a0,0\(a0\)
[ 	]*124: R_MIPS_GOT_LO16	big_external_common
0+0128 <[^>]*> nop
0+012c <[^>]*> lb	a0,1\(a0\)
0+0130 <[^>]*> lui	a0,0x0
[ 	]*130: R_MIPS_GOT_HI16	small_external_common
0+0134 <[^>]*> addu	a0,a0,gp
0+0138 <[^>]*> lw	a0,0\(a0\)
[ 	]*138: R_MIPS_GOT_LO16	small_external_common
0+013c <[^>]*> nop
0+0140 <[^>]*> lb	a0,1\(a0\)
0+0144 <[^>]*> lw	a0,0\(gp\)
[ 	]*144: R_MIPS_GOT16	.bss
0+0148 <[^>]*> nop
0+014c <[^>]*> addiu	a0,a0,0
[ 	]*14c: R_MIPS_LO16	.bss
0+0150 <[^>]*> nop
0+0154 <[^>]*> lb	a0,1\(a0\)
0+0158 <[^>]*> lw	a0,0\(gp\)
[ 	]*158: R_MIPS_GOT16	.bss
0+015c <[^>]*> nop
0+0160 <[^>]*> addiu	a0,a0,1000
[ 	]*160: R_MIPS_LO16	.bss
0+0164 <[^>]*> nop
0+0168 <[^>]*> lb	a0,1\(a0\)
0+016c <[^>]*> lw	a0,0\(gp\)
[ 	]*16c: R_MIPS_GOT16	.data
0+0170 <[^>]*> nop
0+0174 <[^>]*> addiu	a0,a0,0
[ 	]*174: R_MIPS_LO16	.data
0+0178 <[^>]*> nop
0+017c <[^>]*> addu	a0,a0,a1
0+0180 <[^>]*> lb	a0,0\(a0\)
0+0184 <[^>]*> lui	a0,0x0
[ 	]*184: R_MIPS_GOT_HI16	big_external_data_label
0+0188 <[^>]*> addu	a0,a0,gp
0+018c <[^>]*> lw	a0,0\(a0\)
[ 	]*18c: R_MIPS_GOT_LO16	big_external_data_label
0+0190 <[^>]*> nop
0+0194 <[^>]*> addu	a0,a0,a1
0+0198 <[^>]*> lb	a0,0\(a0\)
0+019c <[^>]*> lui	a0,0x0
[ 	]*19c: R_MIPS_GOT_HI16	small_external_data_label
0+01a0 <[^>]*> addu	a0,a0,gp
0+01a4 <[^>]*> lw	a0,0\(a0\)
[ 	]*1a4: R_MIPS_GOT_LO16	small_external_data_label
0+01a8 <[^>]*> nop
0+01ac <[^>]*> addu	a0,a0,a1
0+01b0 <[^>]*> lb	a0,0\(a0\)
0+01b4 <[^>]*> lui	a0,0x0
[ 	]*1b4: R_MIPS_GOT_HI16	big_external_common
0+01b8 <[^>]*> addu	a0,a0,gp
0+01bc <[^>]*> lw	a0,0\(a0\)
[ 	]*1bc: R_MIPS_GOT_LO16	big_external_common
0+01c0 <[^>]*> nop
0+01c4 <[^>]*> addu	a0,a0,a1
0+01c8 <[^>]*> lb	a0,0\(a0\)
0+01cc <[^>]*> lui	a0,0x0
[ 	]*1cc: R_MIPS_GOT_HI16	small_external_common
0+01d0 <[^>]*> addu	a0,a0,gp
0+01d4 <[^>]*> lw	a0,0\(a0\)
[ 	]*1d4: R_MIPS_GOT_LO16	small_external_common
0+01d8 <[^>]*> nop
0+01dc <[^>]*> addu	a0,a0,a1
0+01e0 <[^>]*> lb	a0,0\(a0\)
0+01e4 <[^>]*> lw	a0,0\(gp\)
[ 	]*1e4: R_MIPS_GOT16	.bss
0+01e8 <[^>]*> nop
0+01ec <[^>]*> addiu	a0,a0,0
[ 	]*1ec: R_MIPS_LO16	.bss
0+01f0 <[^>]*> nop
0+01f4 <[^>]*> addu	a0,a0,a1
0+01f8 <[^>]*> lb	a0,0\(a0\)
0+01fc <[^>]*> lw	a0,0\(gp\)
[ 	]*1fc: R_MIPS_GOT16	.bss
0+0200 <[^>]*> nop
0+0204 <[^>]*> addiu	a0,a0,1000
[ 	]*204: R_MIPS_LO16	.bss
0+0208 <[^>]*> nop
0+020c <[^>]*> addu	a0,a0,a1
0+0210 <[^>]*> lb	a0,0\(a0\)
0+0214 <[^>]*> lw	a0,0\(gp\)
[ 	]*214: R_MIPS_GOT16	.data
0+0218 <[^>]*> nop
0+021c <[^>]*> addiu	a0,a0,0
[ 	]*21c: R_MIPS_LO16	.data
0+0220 <[^>]*> nop
0+0224 <[^>]*> addu	a0,a0,a1
0+0228 <[^>]*> lb	a0,1\(a0\)
0+022c <[^>]*> lui	a0,0x0
[ 	]*22c: R_MIPS_GOT_HI16	big_external_data_label
0+0230 <[^>]*> addu	a0,a0,gp
0+0234 <[^>]*> lw	a0,0\(a0\)
[ 	]*234: R_MIPS_GOT_LO16	big_external_data_label
0+0238 <[^>]*> nop
0+023c <[^>]*> addu	a0,a0,a1
0+0240 <[^>]*> lb	a0,1\(a0\)
0+0244 <[^>]*> lui	a0,0x0
[ 	]*244: R_MIPS_GOT_HI16	small_external_data_label
0+0248 <[^>]*> addu	a0,a0,gp
0+024c <[^>]*> lw	a0,0\(a0\)
[ 	]*24c: R_MIPS_GOT_LO16	small_external_data_label
0+0250 <[^>]*> nop
0+0254 <[^>]*> addu	a0,a0,a1
0+0258 <[^>]*> lb	a0,1\(a0\)
0+025c <[^>]*> lui	a0,0x0
[ 	]*25c: R_MIPS_GOT_HI16	big_external_common
0+0260 <[^>]*> addu	a0,a0,gp
0+0264 <[^>]*> lw	a0,0\(a0\)
[ 	]*264: R_MIPS_GOT_LO16	big_external_common
0+0268 <[^>]*> nop
0+026c <[^>]*> addu	a0,a0,a1
0+0270 <[^>]*> lb	a0,1\(a0\)
0+0274 <[^>]*> lui	a0,0x0
[ 	]*274: R_MIPS_GOT_HI16	small_external_common
0+0278 <[^>]*> addu	a0,a0,gp
0+027c <[^>]*> lw	a0,0\(a0\)
[ 	]*27c: R_MIPS_GOT_LO16	small_external_common
0+0280 <[^>]*> nop
0+0284 <[^>]*> addu	a0,a0,a1
0+0288 <[^>]*> lb	a0,1\(a0\)
0+028c <[^>]*> lw	a0,0\(gp\)
[ 	]*28c: R_MIPS_GOT16	.bss
0+0290 <[^>]*> nop
0+0294 <[^>]*> addiu	a0,a0,0
[ 	]*294: R_MIPS_LO16	.bss
0+0298 <[^>]*> nop
0+029c <[^>]*> addu	a0,a0,a1
0+02a0 <[^>]*> lb	a0,1\(a0\)
0+02a4 <[^>]*> lw	a0,0\(gp\)
[ 	]*2a4: R_MIPS_GOT16	.bss
0+02a8 <[^>]*> nop
0+02ac <[^>]*> addiu	a0,a0,1000
[ 	]*2ac: R_MIPS_LO16	.bss
0+02b0 <[^>]*> nop
0+02b4 <[^>]*> addu	a0,a0,a1
0+02b8 <[^>]*> lb	a0,1\(a0\)
0+02bc <[^>]*> nop
