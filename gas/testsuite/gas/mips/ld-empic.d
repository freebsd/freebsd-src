#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS ld-empic
#as: -32 -mips1 -membedded-pic --defsym EMPIC=1
#source: ld-pic.s

# Test the ld macro with -membedded-pic.

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
0+0090 <[^>]*> lw	a0,-16384\(gp\)
[ 	]*90: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+0094 <[^>]*> lw	a1,-16380\(gp\)
[ 	]*94: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+0098 <[^>]*> lw	a0,0\(gp\)
[ 	]*98: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+009c <[^>]*> lw	a1,4\(gp\)
[ 	]*9c: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+00a0 <[^>]*> lw	a0,0\(gp\)
[ 	]*a0: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+00a4 <[^>]*> lw	a1,4\(gp\)
[ 	]*a4: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+00a8 <[^>]*> lw	a0,0\(gp\)
[ 	]*a8: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+00ac <[^>]*> lw	a1,4\(gp\)
[ 	]*ac: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+00b0 <[^>]*> lw	a0,0\(gp\)
[ 	]*b0: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+00b4 <[^>]*> lw	a1,4\(gp\)
[ 	]*b4: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+00b8 <[^>]*> lw	a0,-16384\(gp\)
[ 	]*b8: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00bc <[^>]*> lw	a1,-16380\(gp\)
[ 	]*bc: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00c0 <[^>]*> lw	a0,-15384\(gp\)
[ 	]*c0: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00c4 <[^>]*> lw	a1,-15380\(gp\)
[ 	]*c4: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00c8 <[^>]*> lw	a0,-16383\(gp\)
[ 	]*c8: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+00cc <[^>]*> lw	a1,-16379\(gp\)
[ 	]*cc: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+00d0 <[^>]*> lw	a0,1\(gp\)
[ 	]*d0: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+00d4 <[^>]*> lw	a1,5\(gp\)
[ 	]*d4: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+00d8 <[^>]*> lw	a0,1\(gp\)
[ 	]*d8: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+00dc <[^>]*> lw	a1,5\(gp\)
[ 	]*dc: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+00e0 <[^>]*> lw	a0,1\(gp\)
[ 	]*e0: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+00e4 <[^>]*> lw	a1,5\(gp\)
[ 	]*e4: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+00e8 <[^>]*> lw	a0,1\(gp\)
[ 	]*e8: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+00ec <[^>]*> lw	a1,5\(gp\)
[ 	]*ec: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+00f0 <[^>]*> lw	a0,-16383\(gp\)
[ 	]*f0: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00f4 <[^>]*> lw	a1,-16379\(gp\)
[ 	]*f4: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00f8 <[^>]*> lw	a0,-15383\(gp\)
[ 	]*f8: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+00fc <[^>]*> lw	a1,-15379\(gp\)
[ 	]*fc: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+0100 <[^>]*> nop
0+0104 <[^>]*> addu	at,a1,gp
0+0108 <[^>]*> lw	a0,-16384\(at\)
[ 	]*108: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+010c <[^>]*> lw	a1,-16380\(at\)
[ 	]*10c: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+0110 <[^>]*> nop
0+0114 <[^>]*> addu	at,a1,gp
0+0118 <[^>]*> lw	a0,0\(at\)
[ 	]*118: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+011c <[^>]*> lw	a1,4\(at\)
[ 	]*11c: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+0120 <[^>]*> nop
0+0124 <[^>]*> addu	at,a1,gp
0+0128 <[^>]*> lw	a0,0\(at\)
[ 	]*128: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+012c <[^>]*> lw	a1,4\(at\)
[ 	]*12c: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+0130 <[^>]*> nop
0+0134 <[^>]*> addu	at,a1,gp
0+0138 <[^>]*> lw	a0,0\(at\)
[ 	]*138: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+013c <[^>]*> lw	a1,4\(at\)
[ 	]*13c: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+0140 <[^>]*> nop
0+0144 <[^>]*> addu	at,a1,gp
0+0148 <[^>]*> lw	a0,0\(at\)
[ 	]*148: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+014c <[^>]*> lw	a1,4\(at\)
[ 	]*14c: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+0150 <[^>]*> nop
0+0154 <[^>]*> addu	at,a1,gp
0+0158 <[^>]*> lw	a0,-16384\(at\)
[ 	]*158: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+015c <[^>]*> lw	a1,-16380\(at\)
[ 	]*15c: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+0160 <[^>]*> nop
0+0164 <[^>]*> addu	at,a1,gp
0+0168 <[^>]*> lw	a0,-15384\(at\)
[ 	]*168: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+016c <[^>]*> lw	a1,-15380\(at\)
[ 	]*16c: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+0170 <[^>]*> nop
0+0174 <[^>]*> addu	at,a1,gp
0+0178 <[^>]*> lw	a0,-16383\(at\)
[ 	]*178: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+017c <[^>]*> lw	a1,-16379\(at\)
[ 	]*17c: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sdata.*
0+0180 <[^>]*> nop
0+0184 <[^>]*> addu	at,a1,gp
0+0188 <[^>]*> lw	a0,1\(at\)
[ 	]*188: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+018c <[^>]*> lw	a1,5\(at\)
[ 	]*18c: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_data_label
0+0190 <[^>]*> nop
0+0194 <[^>]*> addu	at,a1,gp
0+0198 <[^>]*> lw	a0,1\(at\)
[ 	]*198: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+019c <[^>]*> lw	a1,5\(at\)
[ 	]*19c: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_data_label
0+01a0 <[^>]*> nop
0+01a4 <[^>]*> addu	at,a1,gp
0+01a8 <[^>]*> lw	a0,1\(at\)
[ 	]*1a8: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+01ac <[^>]*> lw	a1,5\(at\)
[ 	]*1ac: [A-Z0-9_]*GPREL[A-Z0-9_]*	big_external_common
0+01b0 <[^>]*> nop
0+01b4 <[^>]*> addu	at,a1,gp
0+01b8 <[^>]*> lw	a0,1\(at\)
[ 	]*1b8: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+01bc <[^>]*> lw	a1,5\(at\)
[ 	]*1bc: [A-Z0-9_]*GPREL[A-Z0-9_]*	small_external_common
0+01c0 <[^>]*> nop
0+01c4 <[^>]*> addu	at,a1,gp
0+01c8 <[^>]*> lw	a0,-16383\(at\)
[ 	]*1c8: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+01cc <[^>]*> lw	a1,-16379\(at\)
[ 	]*1cc: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+01d0 <[^>]*> nop
0+01d4 <[^>]*> addu	at,a1,gp
0+01d8 <[^>]*> lw	a0,-15383\(at\)
[ 	]*1d8: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
0+01dc <[^>]*> lw	a1,-15379\(at\)
[ 	]*1dc: [A-Z0-9_]*GPREL[A-Z0-9_]*	.sbss.*
