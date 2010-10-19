#objdump: -dr --prefix-addresses -mmips:3000
#name: MIPS lb-xgot-ilocks
#as: -32 -mips1 -KPIC -xgot -mtune=r3900 -march=r3900
#source: lb-pic.s

# Test the lb macro with -KPIC -xgot.

.*: +file format .*

Disassembly of section \.text:
0+0000 <.*> lb	a0,0\(zero\)
0+0004 <.*> lb	a0,1\(zero\)
0+0008 <.*> lui	a0,0x1
0+000c <.*> lb	a0,-32768\(a0\)
0+0010 <.*> lb	a0,-32768\(zero\)
0+0014 <.*> lui	a0,0x1
0+0018 <.*> lb	a0,0\(a0\)
0+001c <.*> lui	a0,0x2
0+0020 <.*> lb	a0,-23131\(a0\)
0+0024 <.*> lb	a0,0\(a1\)
0+0028 <.*> lb	a0,1\(a1\)
0+002c <.*> lui	a0,0x1
0+0030 <.*> addu	a0,a0,a1
0+0034 <.*> lb	a0,-32768\(a0\)
0+0038 <.*> lb	a0,-32768\(a1\)
0+003c <.*> lui	a0,0x1
0+0040 <.*> addu	a0,a0,a1
0+0044 <.*> lb	a0,0\(a0\)
0+0048 <.*> lui	a0,0x2
0+004c <.*> addu	a0,a0,a1
0+0050 <.*> lb	a0,-23131\(a0\)
0+0054 <.*> lw	a0,0\(gp\)
			54: R_MIPS_GOT16	\.data
0+0058 <.*> addiu	a0,a0,0
			58: R_MIPS_LO16	\.data
0+005c <.*> lb	a0,0\(a0\)
0+0060 <.*> lui	a0,0x0
			60: R_MIPS_GOT_HI16	big_external_data_label
0+0064 <.*> addu	a0,a0,gp
0+0068 <.*> lw	a0,0\(a0\)
			68: R_MIPS_GOT_LO16	big_external_data_label
0+006c <.*> lb	a0,0\(a0\)
0+0070 <.*> lui	a0,0x0
			70: R_MIPS_GOT_HI16	small_external_data_label
0+0074 <.*> addu	a0,a0,gp
0+0078 <.*> lw	a0,0\(a0\)
			78: R_MIPS_GOT_LO16	small_external_data_label
0+007c <.*> lb	a0,0\(a0\)
0+0080 <.*> lui	a0,0x0
			80: R_MIPS_GOT_HI16	big_external_common
0+0084 <.*> addu	a0,a0,gp
0+0088 <.*> lw	a0,0\(a0\)
			88: R_MIPS_GOT_LO16	big_external_common
0+008c <.*> lb	a0,0\(a0\)
0+0090 <.*> lui	a0,0x0
			90: R_MIPS_GOT_HI16	small_external_common
0+0094 <.*> addu	a0,a0,gp
0+0098 <.*> lw	a0,0\(a0\)
			98: R_MIPS_GOT_LO16	small_external_common
0+009c <.*> lb	a0,0\(a0\)
0+00a0 <.*> lw	a0,0\(gp\)
			a0: R_MIPS_GOT16	\.bss
0+00a4 <.*> addiu	a0,a0,0
			a4: R_MIPS_LO16	\.bss
0+00a8 <.*> lb	a0,0\(a0\)
0+00ac <.*> lw	a0,0\(gp\)
			ac: R_MIPS_GOT16	\.bss
0+00b0 <.*> addiu	a0,a0,1000
			b0: R_MIPS_LO16	\.bss
0+00b4 <.*> lb	a0,0\(a0\)
0+00b8 <.*> lw	a0,0\(gp\)
			b8: R_MIPS_GOT16	\.data
0+00bc <.*> addiu	a0,a0,0
			bc: R_MIPS_LO16	\.data
0+00c0 <.*> lb	a0,1\(a0\)
0+00c4 <.*> lui	a0,0x0
			c4: R_MIPS_GOT_HI16	big_external_data_label
0+00c8 <.*> addu	a0,a0,gp
0+00cc <.*> lw	a0,0\(a0\)
			cc: R_MIPS_GOT_LO16	big_external_data_label
0+00d0 <.*> lb	a0,1\(a0\)
0+00d4 <.*> lui	a0,0x0
			d4: R_MIPS_GOT_HI16	small_external_data_label
0+00d8 <.*> addu	a0,a0,gp
0+00dc <.*> lw	a0,0\(a0\)
			dc: R_MIPS_GOT_LO16	small_external_data_label
0+00e0 <.*> lb	a0,1\(a0\)
0+00e4 <.*> lui	a0,0x0
			e4: R_MIPS_GOT_HI16	big_external_common
0+00e8 <.*> addu	a0,a0,gp
0+00ec <.*> lw	a0,0\(a0\)
			ec: R_MIPS_GOT_LO16	big_external_common
0+00f0 <.*> lb	a0,1\(a0\)
0+00f4 <.*> lui	a0,0x0
			f4: R_MIPS_GOT_HI16	small_external_common
0+00f8 <.*> addu	a0,a0,gp
0+00fc <.*> lw	a0,0\(a0\)
			fc: R_MIPS_GOT_LO16	small_external_common
0+0100 <.*> lb	a0,1\(a0\)
0+0104 <.*> lw	a0,0\(gp\)
			104: R_MIPS_GOT16	\.bss
0+0108 <.*> addiu	a0,a0,0
			108: R_MIPS_LO16	\.bss
0+010c <.*> lb	a0,1\(a0\)
0+0110 <.*> lw	a0,0\(gp\)
			110: R_MIPS_GOT16	\.bss
0+0114 <.*> addiu	a0,a0,1000
			114: R_MIPS_LO16	\.bss
0+0118 <.*> lb	a0,1\(a0\)
0+011c <.*> lw	a0,0\(gp\)
			11c: R_MIPS_GOT16	\.data
0+0120 <.*> addiu	a0,a0,0
			120: R_MIPS_LO16	\.data
0+0124 <.*> addu	a0,a0,a1
0+0128 <.*> lb	a0,0\(a0\)
0+012c <.*> lui	a0,0x0
			12c: R_MIPS_GOT_HI16	big_external_data_label
0+0130 <.*> addu	a0,a0,gp
0+0134 <.*> lw	a0,0\(a0\)
			134: R_MIPS_GOT_LO16	big_external_data_label
0+0138 <.*> addu	a0,a0,a1
0+013c <.*> lb	a0,0\(a0\)
0+0140 <.*> lui	a0,0x0
			140: R_MIPS_GOT_HI16	small_external_data_label
0+0144 <.*> addu	a0,a0,gp
0+0148 <.*> lw	a0,0\(a0\)
			148: R_MIPS_GOT_LO16	small_external_data_label
0+014c <.*> addu	a0,a0,a1
0+0150 <.*> lb	a0,0\(a0\)
0+0154 <.*> lui	a0,0x0
			154: R_MIPS_GOT_HI16	big_external_common
0+0158 <.*> addu	a0,a0,gp
0+015c <.*> lw	a0,0\(a0\)
			15c: R_MIPS_GOT_LO16	big_external_common
0+0160 <.*> addu	a0,a0,a1
0+0164 <.*> lb	a0,0\(a0\)
0+0168 <.*> lui	a0,0x0
			168: R_MIPS_GOT_HI16	small_external_common
0+016c <.*> addu	a0,a0,gp
0+0170 <.*> lw	a0,0\(a0\)
			170: R_MIPS_GOT_LO16	small_external_common
0+0174 <.*> addu	a0,a0,a1
0+0178 <.*> lb	a0,0\(a0\)
0+017c <.*> lw	a0,0\(gp\)
			17c: R_MIPS_GOT16	\.bss
0+0180 <.*> addiu	a0,a0,0
			180: R_MIPS_LO16	\.bss
0+0184 <.*> addu	a0,a0,a1
0+0188 <.*> lb	a0,0\(a0\)
0+018c <.*> lw	a0,0\(gp\)
			18c: R_MIPS_GOT16	\.bss
0+0190 <.*> addiu	a0,a0,1000
			190: R_MIPS_LO16	\.bss
0+0194 <.*> addu	a0,a0,a1
0+0198 <.*> lb	a0,0\(a0\)
0+019c <.*> lw	a0,0\(gp\)
			19c: R_MIPS_GOT16	\.data
0+01a0 <.*> addiu	a0,a0,0
			1a0: R_MIPS_LO16	\.data
0+01a4 <.*> addu	a0,a0,a1
0+01a8 <.*> lb	a0,1\(a0\)
0+01ac <.*> lui	a0,0x0
			1ac: R_MIPS_GOT_HI16	big_external_data_label
0+01b0 <.*> addu	a0,a0,gp
0+01b4 <.*> lw	a0,0\(a0\)
			1b4: R_MIPS_GOT_LO16	big_external_data_label
0+01b8 <.*> addu	a0,a0,a1
0+01bc <.*> lb	a0,1\(a0\)
0+01c0 <.*> lui	a0,0x0
			1c0: R_MIPS_GOT_HI16	small_external_data_label
0+01c4 <.*> addu	a0,a0,gp
0+01c8 <.*> lw	a0,0\(a0\)
			1c8: R_MIPS_GOT_LO16	small_external_data_label
0+01cc <.*> addu	a0,a0,a1
0+01d0 <.*> lb	a0,1\(a0\)
0+01d4 <.*> lui	a0,0x0
			1d4: R_MIPS_GOT_HI16	big_external_common
0+01d8 <.*> addu	a0,a0,gp
0+01dc <.*> lw	a0,0\(a0\)
			1dc: R_MIPS_GOT_LO16	big_external_common
0+01e0 <.*> addu	a0,a0,a1
0+01e4 <.*> lb	a0,1\(a0\)
0+01e8 <.*> lui	a0,0x0
			1e8: R_MIPS_GOT_HI16	small_external_common
0+01ec <.*> addu	a0,a0,gp
0+01f0 <.*> lw	a0,0\(a0\)
			1f0: R_MIPS_GOT_LO16	small_external_common
0+01f4 <.*> addu	a0,a0,a1
0+01f8 <.*> lb	a0,1\(a0\)
0+01fc <.*> lw	a0,0\(gp\)
			1fc: R_MIPS_GOT16	\.bss
0+0200 <.*> addiu	a0,a0,0
			200: R_MIPS_LO16	\.bss
0+0204 <.*> addu	a0,a0,a1
0+0208 <.*> lb	a0,1\(a0\)
0+020c <.*> lw	a0,0\(gp\)
			20c: R_MIPS_GOT16	\.bss
0+0210 <.*> addiu	a0,a0,1000
			210: R_MIPS_LO16	\.bss
0+0214 <.*> addu	a0,a0,a1
0+0218 <.*> lb	a0,1\(a0\)
0+021c <.*> nop
