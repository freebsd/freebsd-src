#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS ulh-xgot
#as: -32 -mips1 -mtune=r3000 -KPIC -xgot -EB --defsym XGOT=1
#source: ulh-pic.s

# Test the unaligned load and store macros with -KPIC -xgot.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> lw	at,0\(gp\)
[ 	]*0: R_MIPS_GOT16	.data
0+0004 <[^>]*> nop
0+0008 <[^>]*> addiu	at,at,0
[ 	]*8: R_MIPS_LO16	.data
0+000c <[^>]*> nop
0+0010 <[^>]*> lb	a0,0\(at\)
0+0014 <[^>]*> lbu	at,1\(at\)
0+0018 <[^>]*> sll	a0,a0,0x8
0+001c <[^>]*> or	a0,a0,at
0+0020 <[^>]*> lui	at,0x0
[ 	]*20: R_MIPS_GOT_HI16	big_external_data_label
0+0024 <[^>]*> addu	at,at,gp
0+0028 <[^>]*> lw	at,0\(at\)
[ 	]*28: R_MIPS_GOT_LO16	big_external_data_label
0+002c <[^>]*> nop
0+0030 <[^>]*> lbu	a0,0\(at\)
0+0034 <[^>]*> lbu	at,1\(at\)
0+0038 <[^>]*> sll	a0,a0,0x8
0+003c <[^>]*> or	a0,a0,at
0+0040 <[^>]*> lui	at,0x0
[ 	]*40: R_MIPS_GOT_HI16	small_external_data_label
0+0044 <[^>]*> addu	at,at,gp
0+0048 <[^>]*> lw	at,0\(at\)
[ 	]*48: R_MIPS_GOT_LO16	small_external_data_label
0+004c <[^>]*> nop
0+0050 <[^>]*> lwl	a0,0\(at\)
0+0054 <[^>]*> lwr	a0,3\(at\)
0+0058 <[^>]*> lui	at,0x0
[ 	]*58: R_MIPS_GOT_HI16	big_external_common
0+005c <[^>]*> addu	at,at,gp
0+0060 <[^>]*> lw	at,0\(at\)
[ 	]*60: R_MIPS_GOT_LO16	big_external_common
0+0064 <[^>]*> nop
0+0068 <[^>]*> sb	a0,1\(at\)
0+006c <[^>]*> srl	a0,a0,0x8
0+0070 <[^>]*> sb	a0,0\(at\)
0+0074 <[^>]*> lbu	at,1\(at\)
0+0078 <[^>]*> sll	a0,a0,0x8
0+007c <[^>]*> or	a0,a0,at
0+0080 <[^>]*> lui	at,0x0
[ 	]*80: R_MIPS_GOT_HI16	small_external_common
0+0084 <[^>]*> addu	at,at,gp
0+0088 <[^>]*> lw	at,0\(at\)
[ 	]*88: R_MIPS_GOT_LO16	small_external_common
0+008c <[^>]*> nop
0+0090 <[^>]*> swl	a0,0\(at\)
0+0094 <[^>]*> swr	a0,3\(at\)
0+0098 <[^>]*> lw	at,0\(gp\)
[ 	]*98: R_MIPS_GOT16	.bss
0+009c <[^>]*> nop
0+00a0 <[^>]*> addiu	at,at,0
[ 	]*a0: R_MIPS_LO16	.bss
0+00a4 <[^>]*> nop
0+00a8 <[^>]*> lb	a0,0\(at\)
0+00ac <[^>]*> lbu	at,1\(at\)
0+00b0 <[^>]*> sll	a0,a0,0x8
0+00b4 <[^>]*> or	a0,a0,at
0+00b8 <[^>]*> lw	at,0\(gp\)
[ 	]*b8: R_MIPS_GOT16	.bss
0+00bc <[^>]*> nop
0+00c0 <[^>]*> addiu	at,at,1000
[ 	]*c0: R_MIPS_LO16	.bss
0+00c4 <[^>]*> nop
0+00c8 <[^>]*> lbu	a0,0\(at\)
0+00cc <[^>]*> lbu	at,1\(at\)
0+00d0 <[^>]*> sll	a0,a0,0x8
0+00d4 <[^>]*> or	a0,a0,at
0+00d8 <[^>]*> lw	at,0\(gp\)
[ 	]*d8: R_MIPS_GOT16	.data
0+00dc <[^>]*> nop
0+00e0 <[^>]*> addiu	at,at,0
[ 	]*e0: R_MIPS_LO16	.data
0+00e4 <[^>]*> nop
0+00e8 <[^>]*> addiu	at,at,1
0+00ec <[^>]*> lwl	a0,0\(at\)
0+00f0 <[^>]*> lwr	a0,3\(at\)
0+00f4 <[^>]*> lui	at,0x0
[ 	]*f4: R_MIPS_GOT_HI16	big_external_data_label
0+00f8 <[^>]*> addu	at,at,gp
0+00fc <[^>]*> lw	at,0\(at\)
[ 	]*fc: R_MIPS_GOT_LO16	big_external_data_label
0+0100 <[^>]*> nop
0+0104 <[^>]*> addiu	at,at,1
0+0108 <[^>]*> sb	a0,1\(at\)
0+010c <[^>]*> srl	a0,a0,0x8
0+0110 <[^>]*> sb	a0,0\(at\)
0+0114 <[^>]*> lbu	at,1\(at\)
0+0118 <[^>]*> sll	a0,a0,0x8
0+011c <[^>]*> or	a0,a0,at
0+0120 <[^>]*> lui	at,0x0
[ 	]*120: R_MIPS_GOT_HI16	small_external_data_label
0+0124 <[^>]*> addu	at,at,gp
0+0128 <[^>]*> lw	at,0\(at\)
[ 	]*128: R_MIPS_GOT_LO16	small_external_data_label
0+012c <[^>]*> nop
0+0130 <[^>]*> addiu	at,at,1
0+0134 <[^>]*> swl	a0,0\(at\)
0+0138 <[^>]*> swr	a0,3\(at\)
0+013c <[^>]*> lui	at,0x0
[ 	]*13c: R_MIPS_GOT_HI16	big_external_common
0+0140 <[^>]*> addu	at,at,gp
0+0144 <[^>]*> lw	at,0\(at\)
[ 	]*144: R_MIPS_GOT_LO16	big_external_common
0+0148 <[^>]*> nop
0+014c <[^>]*> addiu	at,at,1
0+0150 <[^>]*> lb	a0,0\(at\)
0+0154 <[^>]*> lbu	at,1\(at\)
0+0158 <[^>]*> sll	a0,a0,0x8
0+015c <[^>]*> or	a0,a0,at
0+0160 <[^>]*> lui	at,0x0
[ 	]*160: R_MIPS_GOT_HI16	small_external_common
0+0164 <[^>]*> addu	at,at,gp
0+0168 <[^>]*> lw	at,0\(at\)
[ 	]*168: R_MIPS_GOT_LO16	small_external_common
0+016c <[^>]*> nop
0+0170 <[^>]*> addiu	at,at,1
0+0174 <[^>]*> lbu	a0,0\(at\)
0+0178 <[^>]*> lbu	at,1\(at\)
0+017c <[^>]*> sll	a0,a0,0x8
0+0180 <[^>]*> or	a0,a0,at
0+0184 <[^>]*> lw	at,0\(gp\)
[ 	]*184: R_MIPS_GOT16	.bss
0+0188 <[^>]*> nop
0+018c <[^>]*> addiu	at,at,0
[ 	]*18c: R_MIPS_LO16	.bss
0+0190 <[^>]*> nop
0+0194 <[^>]*> addiu	at,at,1
0+0198 <[^>]*> lwl	a0,0\(at\)
0+019c <[^>]*> lwr	a0,3\(at\)
0+01a0 <[^>]*> lw	at,0\(gp\)
[ 	]*1a0: R_MIPS_GOT16	.bss
0+01a4 <[^>]*> nop
0+01a8 <[^>]*> addiu	at,at,1000
[ 	]*1a8: R_MIPS_LO16	.bss
0+01ac <[^>]*> nop
0+01b0 <[^>]*> addiu	at,at,1
0+01b4 <[^>]*> sb	a0,1\(at\)
0+01b8 <[^>]*> srl	a0,a0,0x8
0+01bc <[^>]*> sb	a0,0\(at\)
0+01c0 <[^>]*> lbu	at,1\(at\)
0+01c4 <[^>]*> sll	a0,a0,0x8
0+01c8 <[^>]*> or	a0,a0,at
0+01cc <[^>]*> nop
