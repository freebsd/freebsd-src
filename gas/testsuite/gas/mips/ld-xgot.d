#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS ld-xgot
#as: -32 -mips1 -mtune=r3000 -KPIC -xgot
#source: ld-pic.s

# Test the ld macro with -KPIC -xgot.

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
0+00a0 <[^>]*> lui	at,0x0
[ 	]*a0: R_MIPS_GOT_HI16	big_external_data_label
0+00a4 <[^>]*> addu	at,at,gp
0+00a8 <[^>]*> lw	at,0\(at\)
[ 	]*a8: R_MIPS_GOT_LO16	big_external_data_label
0+00ac <[^>]*> nop
0+00b0 <[^>]*> lw	a0,0\(at\)
0+00b4 <[^>]*> lw	a1,4\(at\)
0+00b8 <[^>]*> lui	at,0x0
[ 	]*b8: R_MIPS_GOT_HI16	small_external_data_label
0+00bc <[^>]*> addu	at,at,gp
0+00c0 <[^>]*> lw	at,0\(at\)
[ 	]*c0: R_MIPS_GOT_LO16	small_external_data_label
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> lw	a0,0\(at\)
0+00cc <[^>]*> lw	a1,4\(at\)
0+00d0 <[^>]*> lui	at,0x0
[ 	]*d0: R_MIPS_GOT_HI16	big_external_common
0+00d4 <[^>]*> addu	at,at,gp
0+00d8 <[^>]*> lw	at,0\(at\)
[ 	]*d8: R_MIPS_GOT_LO16	big_external_common
0+00dc <[^>]*> nop
0+00e0 <[^>]*> lw	a0,0\(at\)
0+00e4 <[^>]*> lw	a1,4\(at\)
0+00e8 <[^>]*> lui	at,0x0
[ 	]*e8: R_MIPS_GOT_HI16	small_external_common
0+00ec <[^>]*> addu	at,at,gp
0+00f0 <[^>]*> lw	at,0\(at\)
[ 	]*f0: R_MIPS_GOT_LO16	small_external_common
0+00f4 <[^>]*> nop
0+00f8 <[^>]*> lw	a0,0\(at\)
0+00fc <[^>]*> lw	a1,4\(at\)
0+0100 <[^>]*> lw	at,0\(gp\)
[ 	]*100: R_MIPS_GOT16	.bss
0+0104 <[^>]*> nop
0+0108 <[^>]*> lw	a0,0\(at\)
[ 	]*108: R_MIPS_LO16	.bss
0+010c <[^>]*> lw	a1,4\(at\)
[ 	]*10c: R_MIPS_LO16	.bss
0+0110 <[^>]*> lw	at,0\(gp\)
[ 	]*110: R_MIPS_GOT16	.bss
0+0114 <[^>]*> nop
0+0118 <[^>]*> lw	a0,1000\(at\)
[ 	]*118: R_MIPS_LO16	.bss
0+011c <[^>]*> lw	a1,1004\(at\)
[ 	]*11c: R_MIPS_LO16	.bss
0+0120 <[^>]*> lw	at,0\(gp\)
[ 	]*120: R_MIPS_GOT16	.data
0+0124 <[^>]*> nop
0+0128 <[^>]*> lw	a0,1\(at\)
[ 	]*128: R_MIPS_LO16	.data
0+012c <[^>]*> lw	a1,5\(at\)
[ 	]*12c: R_MIPS_LO16	.data
0+0130 <[^>]*> lui	at,0x0
[ 	]*130: R_MIPS_GOT_HI16	big_external_data_label
0+0134 <[^>]*> addu	at,at,gp
0+0138 <[^>]*> lw	at,0\(at\)
[ 	]*138: R_MIPS_GOT_LO16	big_external_data_label
0+013c <[^>]*> nop
0+0140 <[^>]*> lw	a0,1\(at\)
0+0144 <[^>]*> lw	a1,5\(at\)
0+0148 <[^>]*> lui	at,0x0
[ 	]*148: R_MIPS_GOT_HI16	small_external_data_label
0+014c <[^>]*> addu	at,at,gp
0+0150 <[^>]*> lw	at,0\(at\)
[ 	]*150: R_MIPS_GOT_LO16	small_external_data_label
0+0154 <[^>]*> nop
0+0158 <[^>]*> lw	a0,1\(at\)
0+015c <[^>]*> lw	a1,5\(at\)
0+0160 <[^>]*> lui	at,0x0
[ 	]*160: R_MIPS_GOT_HI16	big_external_common
0+0164 <[^>]*> addu	at,at,gp
0+0168 <[^>]*> lw	at,0\(at\)
[ 	]*168: R_MIPS_GOT_LO16	big_external_common
0+016c <[^>]*> nop
0+0170 <[^>]*> lw	a0,1\(at\)
0+0174 <[^>]*> lw	a1,5\(at\)
0+0178 <[^>]*> lui	at,0x0
[ 	]*178: R_MIPS_GOT_HI16	small_external_common
0+017c <[^>]*> addu	at,at,gp
0+0180 <[^>]*> lw	at,0\(at\)
[ 	]*180: R_MIPS_GOT_LO16	small_external_common
0+0184 <[^>]*> nop
0+0188 <[^>]*> lw	a0,1\(at\)
0+018c <[^>]*> lw	a1,5\(at\)
0+0190 <[^>]*> lw	at,0\(gp\)
[ 	]*190: R_MIPS_GOT16	.bss
0+0194 <[^>]*> nop
0+0198 <[^>]*> lw	a0,1\(at\)
[ 	]*198: R_MIPS_LO16	.bss
0+019c <[^>]*> lw	a1,5\(at\)
[ 	]*19c: R_MIPS_LO16	.bss
0+01a0 <[^>]*> lw	at,0\(gp\)
[ 	]*1a0: R_MIPS_GOT16	.bss
0+01a4 <[^>]*> nop
0+01a8 <[^>]*> lw	a0,1001\(at\)
[ 	]*1a8: R_MIPS_LO16	.bss
0+01ac <[^>]*> lw	a1,1005\(at\)
[ 	]*1ac: R_MIPS_LO16	.bss
0+01b0 <[^>]*> lw	at,0\(gp\)
[ 	]*1b0: R_MIPS_GOT16	.data
0+01b4 <[^>]*> nop
0+01b8 <[^>]*> addu	at,a1,at
0+01bc <[^>]*> lw	a0,0\(at\)
[ 	]*1bc: R_MIPS_LO16	.data
0+01c0 <[^>]*> lw	a1,4\(at\)
[ 	]*1c0: R_MIPS_LO16	.data
0+01c4 <[^>]*> lui	at,0x0
[ 	]*1c4: R_MIPS_GOT_HI16	big_external_data_label
0+01c8 <[^>]*> addu	at,at,gp
0+01cc <[^>]*> lw	at,0\(at\)
[ 	]*1cc: R_MIPS_GOT_LO16	big_external_data_label
0+01d0 <[^>]*> nop
0+01d4 <[^>]*> addu	at,a1,at
0+01d8 <[^>]*> lw	a0,0\(at\)
0+01dc <[^>]*> lw	a1,4\(at\)
0+01e0 <[^>]*> lui	at,0x0
[ 	]*1e0: R_MIPS_GOT_HI16	small_external_data_label
0+01e4 <[^>]*> addu	at,at,gp
0+01e8 <[^>]*> lw	at,0\(at\)
[ 	]*1e8: R_MIPS_GOT_LO16	small_external_data_label
0+01ec <[^>]*> nop
0+01f0 <[^>]*> addu	at,a1,at
0+01f4 <[^>]*> lw	a0,0\(at\)
0+01f8 <[^>]*> lw	a1,4\(at\)
0+01fc <[^>]*> lui	at,0x0
[ 	]*1fc: R_MIPS_GOT_HI16	big_external_common
0+0200 <[^>]*> addu	at,at,gp
0+0204 <[^>]*> lw	at,0\(at\)
[ 	]*204: R_MIPS_GOT_LO16	big_external_common
0+0208 <[^>]*> nop
0+020c <[^>]*> addu	at,a1,at
0+0210 <[^>]*> lw	a0,0\(at\)
0+0214 <[^>]*> lw	a1,4\(at\)
0+0218 <[^>]*> lui	at,0x0
[ 	]*218: R_MIPS_GOT_HI16	small_external_common
0+021c <[^>]*> addu	at,at,gp
0+0220 <[^>]*> lw	at,0\(at\)
[ 	]*220: R_MIPS_GOT_LO16	small_external_common
0+0224 <[^>]*> nop
0+0228 <[^>]*> addu	at,a1,at
0+022c <[^>]*> lw	a0,0\(at\)
0+0230 <[^>]*> lw	a1,4\(at\)
0+0234 <[^>]*> lw	at,0\(gp\)
[ 	]*234: R_MIPS_GOT16	.bss
0+0238 <[^>]*> nop
0+023c <[^>]*> addu	at,a1,at
0+0240 <[^>]*> lw	a0,0\(at\)
[ 	]*240: R_MIPS_LO16	.bss
0+0244 <[^>]*> lw	a1,4\(at\)
[ 	]*244: R_MIPS_LO16	.bss
0+0248 <[^>]*> lw	at,0\(gp\)
[ 	]*248: R_MIPS_GOT16	.bss
0+024c <[^>]*> nop
0+0250 <[^>]*> addu	at,a1,at
0+0254 <[^>]*> lw	a0,1000\(at\)
[ 	]*254: R_MIPS_LO16	.bss
0+0258 <[^>]*> lw	a1,1004\(at\)
[ 	]*258: R_MIPS_LO16	.bss
0+025c <[^>]*> lw	at,0\(gp\)
[ 	]*25c: R_MIPS_GOT16	.data
0+0260 <[^>]*> nop
0+0264 <[^>]*> addu	at,a1,at
0+0268 <[^>]*> lw	a0,1\(at\)
[ 	]*268: R_MIPS_LO16	.data
0+026c <[^>]*> lw	a1,5\(at\)
[ 	]*26c: R_MIPS_LO16	.data
0+0270 <[^>]*> lui	at,0x0
[ 	]*270: R_MIPS_GOT_HI16	big_external_data_label
0+0274 <[^>]*> addu	at,at,gp
0+0278 <[^>]*> lw	at,0\(at\)
[ 	]*278: R_MIPS_GOT_LO16	big_external_data_label
0+027c <[^>]*> nop
0+0280 <[^>]*> addu	at,a1,at
0+0284 <[^>]*> lw	a0,1\(at\)
0+0288 <[^>]*> lw	a1,5\(at\)
0+028c <[^>]*> lui	at,0x0
[ 	]*28c: R_MIPS_GOT_HI16	small_external_data_label
0+0290 <[^>]*> addu	at,at,gp
0+0294 <[^>]*> lw	at,0\(at\)
[ 	]*294: R_MIPS_GOT_LO16	small_external_data_label
0+0298 <[^>]*> nop
0+029c <[^>]*> addu	at,a1,at
0+02a0 <[^>]*> lw	a0,1\(at\)
0+02a4 <[^>]*> lw	a1,5\(at\)
0+02a8 <[^>]*> lui	at,0x0
[ 	]*2a8: R_MIPS_GOT_HI16	big_external_common
0+02ac <[^>]*> addu	at,at,gp
0+02b0 <[^>]*> lw	at,0\(at\)
[ 	]*2b0: R_MIPS_GOT_LO16	big_external_common
0+02b4 <[^>]*> nop
0+02b8 <[^>]*> addu	at,a1,at
0+02bc <[^>]*> lw	a0,1\(at\)
0+02c0 <[^>]*> lw	a1,5\(at\)
0+02c4 <[^>]*> lui	at,0x0
[ 	]*2c4: R_MIPS_GOT_HI16	small_external_common
0+02c8 <[^>]*> addu	at,at,gp
0+02cc <[^>]*> lw	at,0\(at\)
[ 	]*2cc: R_MIPS_GOT_LO16	small_external_common
0+02d0 <[^>]*> nop
0+02d4 <[^>]*> addu	at,a1,at
0+02d8 <[^>]*> lw	a0,1\(at\)
0+02dc <[^>]*> lw	a1,5\(at\)
0+02e0 <[^>]*> lw	at,0\(gp\)
[ 	]*2e0: R_MIPS_GOT16	.bss
0+02e4 <[^>]*> nop
0+02e8 <[^>]*> addu	at,a1,at
0+02ec <[^>]*> lw	a0,1\(at\)
[ 	]*2ec: R_MIPS_LO16	.bss
0+02f0 <[^>]*> lw	a1,5\(at\)
[ 	]*2f0: R_MIPS_LO16	.bss
0+02f4 <[^>]*> lw	at,0\(gp\)
[ 	]*2f4: R_MIPS_GOT16	.bss
0+02f8 <[^>]*> nop
0+02fc <[^>]*> addu	at,a1,at
0+0300 <[^>]*> lw	a0,1001\(at\)
[ 	]*300: R_MIPS_LO16	.bss
0+0304 <[^>]*> lw	a1,1005\(at\)
[ 	]*304: R_MIPS_LO16	.bss
	...
