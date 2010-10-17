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
0+0058 <.*> nop
0+005c <.*> addiu	a0,a0,0
			5c: R_MIPS_LO16	\.data
0+0060 <.*> lb	a0,0\(a0\)
0+0064 <.*> lui	a0,0x0
			64: R_MIPS_GOT_HI16	big_external_data_label
0+0068 <.*> addu	a0,a0,gp
0+006c <.*> lw	a0,0\(a0\)
			6c: R_MIPS_GOT_LO16	big_external_data_label
0+0070 <.*> lb	a0,0\(a0\)
0+0074 <.*> lui	a0,0x0
			74: R_MIPS_GOT_HI16	small_external_data_label
0+0078 <.*> addu	a0,a0,gp
0+007c <.*> lw	a0,0\(a0\)
			7c: R_MIPS_GOT_LO16	small_external_data_label
0+0080 <.*> lb	a0,0\(a0\)
0+0084 <.*> lui	a0,0x0
			84: R_MIPS_GOT_HI16	big_external_common
0+0088 <.*> addu	a0,a0,gp
0+008c <.*> lw	a0,0\(a0\)
			8c: R_MIPS_GOT_LO16	big_external_common
0+0090 <.*> lb	a0,0\(a0\)
0+0094 <.*> lui	a0,0x0
			94: R_MIPS_GOT_HI16	small_external_common
0+0098 <.*> addu	a0,a0,gp
0+009c <.*> lw	a0,0\(a0\)
			9c: R_MIPS_GOT_LO16	small_external_common
0+00a0 <.*> lb	a0,0\(a0\)
0+00a4 <.*> lw	a0,0\(gp\)
			a4: R_MIPS_GOT16	\.bss
0+00a8 <.*> nop
0+00ac <.*> addiu	a0,a0,0
			ac: R_MIPS_LO16	\.bss
0+00b0 <.*> lb	a0,0\(a0\)
0+00b4 <.*> lw	a0,0\(gp\)
			b4: R_MIPS_GOT16	\.bss
0+00b8 <.*> nop
0+00bc <.*> addiu	a0,a0,1000
			bc: R_MIPS_LO16	\.bss
0+00c0 <.*> lb	a0,0\(a0\)
0+00c4 <.*> lw	a0,0\(gp\)
			c4: R_MIPS_GOT16	\.data
0+00c8 <.*> nop
0+00cc <.*> addiu	a0,a0,0
			cc: R_MIPS_LO16	\.data
0+00d0 <.*> lb	a0,1\(a0\)
0+00d4 <.*> lui	a0,0x0
			d4: R_MIPS_GOT_HI16	big_external_data_label
0+00d8 <.*> addu	a0,a0,gp
0+00dc <.*> lw	a0,0\(a0\)
			dc: R_MIPS_GOT_LO16	big_external_data_label
0+00e0 <.*> lb	a0,1\(a0\)
0+00e4 <.*> lui	a0,0x0
			e4: R_MIPS_GOT_HI16	small_external_data_label
0+00e8 <.*> addu	a0,a0,gp
0+00ec <.*> lw	a0,0\(a0\)
			ec: R_MIPS_GOT_LO16	small_external_data_label
0+00f0 <.*> lb	a0,1\(a0\)
0+00f4 <.*> lui	a0,0x0
			f4: R_MIPS_GOT_HI16	big_external_common
0+00f8 <.*> addu	a0,a0,gp
0+00fc <.*> lw	a0,0\(a0\)
			fc: R_MIPS_GOT_LO16	big_external_common
0+0100 <.*> lb	a0,1\(a0\)
0+0104 <.*> lui	a0,0x0
			104: R_MIPS_GOT_HI16	small_external_common
0+0108 <.*> addu	a0,a0,gp
0+010c <.*> lw	a0,0\(a0\)
			10c: R_MIPS_GOT_LO16	small_external_common
0+0110 <.*> lb	a0,1\(a0\)
0+0114 <.*> lw	a0,0\(gp\)
			114: R_MIPS_GOT16	\.bss
0+0118 <.*> nop
0+011c <.*> addiu	a0,a0,0
			11c: R_MIPS_LO16	\.bss
0+0120 <.*> lb	a0,1\(a0\)
0+0124 <.*> lw	a0,0\(gp\)
			124: R_MIPS_GOT16	\.bss
0+0128 <.*> nop
0+012c <.*> addiu	a0,a0,1000
			12c: R_MIPS_LO16	\.bss
0+0130 <.*> lb	a0,1\(a0\)
0+0134 <.*> lw	a0,0\(gp\)
			134: R_MIPS_GOT16	\.data
0+0138 <.*> nop
0+013c <.*> addiu	a0,a0,0
			13c: R_MIPS_LO16	\.data
0+0140 <.*> addu	a0,a0,a1
0+0144 <.*> lb	a0,0\(a0\)
0+0148 <.*> lui	a0,0x0
			148: R_MIPS_GOT_HI16	big_external_data_label
0+014c <.*> addu	a0,a0,gp
0+0150 <.*> lw	a0,0\(a0\)
			150: R_MIPS_GOT_LO16	big_external_data_label
0+0154 <.*> addu	a0,a0,a1
0+0158 <.*> lb	a0,0\(a0\)
0+015c <.*> lui	a0,0x0
			15c: R_MIPS_GOT_HI16	small_external_data_label
0+0160 <.*> addu	a0,a0,gp
0+0164 <.*> lw	a0,0\(a0\)
			164: R_MIPS_GOT_LO16	small_external_data_label
0+0168 <.*> addu	a0,a0,a1
0+016c <.*> lb	a0,0\(a0\)
0+0170 <.*> lui	a0,0x0
			170: R_MIPS_GOT_HI16	big_external_common
0+0174 <.*> addu	a0,a0,gp
0+0178 <.*> lw	a0,0\(a0\)
			178: R_MIPS_GOT_LO16	big_external_common
0+017c <.*> addu	a0,a0,a1
0+0180 <.*> lb	a0,0\(a0\)
0+0184 <.*> lui	a0,0x0
			184: R_MIPS_GOT_HI16	small_external_common
0+0188 <.*> addu	a0,a0,gp
0+018c <.*> lw	a0,0\(a0\)
			18c: R_MIPS_GOT_LO16	small_external_common
0+0190 <.*> addu	a0,a0,a1
0+0194 <.*> lb	a0,0\(a0\)
0+0198 <.*> lw	a0,0\(gp\)
			198: R_MIPS_GOT16	\.bss
0+019c <.*> nop
0+01a0 <.*> addiu	a0,a0,0
			1a0: R_MIPS_LO16	\.bss
0+01a4 <.*> addu	a0,a0,a1
0+01a8 <.*> lb	a0,0\(a0\)
0+01ac <.*> lw	a0,0\(gp\)
			1ac: R_MIPS_GOT16	\.bss
0+01b0 <.*> nop
0+01b4 <.*> addiu	a0,a0,1000
			1b4: R_MIPS_LO16	\.bss
0+01b8 <.*> addu	a0,a0,a1
0+01bc <.*> lb	a0,0\(a0\)
0+01c0 <.*> lw	a0,0\(gp\)
			1c0: R_MIPS_GOT16	\.data
0+01c4 <.*> nop
0+01c8 <.*> addiu	a0,a0,0
			1c8: R_MIPS_LO16	\.data
0+01cc <.*> addu	a0,a0,a1
0+01d0 <.*> lb	a0,1\(a0\)
0+01d4 <.*> lui	a0,0x0
			1d4: R_MIPS_GOT_HI16	big_external_data_label
0+01d8 <.*> addu	a0,a0,gp
0+01dc <.*> lw	a0,0\(a0\)
			1dc: R_MIPS_GOT_LO16	big_external_data_label
0+01e0 <.*> addu	a0,a0,a1
0+01e4 <.*> lb	a0,1\(a0\)
0+01e8 <.*> lui	a0,0x0
			1e8: R_MIPS_GOT_HI16	small_external_data_label
0+01ec <.*> addu	a0,a0,gp
0+01f0 <.*> lw	a0,0\(a0\)
			1f0: R_MIPS_GOT_LO16	small_external_data_label
0+01f4 <.*> addu	a0,a0,a1
0+01f8 <.*> lb	a0,1\(a0\)
0+01fc <.*> lui	a0,0x0
			1fc: R_MIPS_GOT_HI16	big_external_common
0+0200 <.*> addu	a0,a0,gp
0+0204 <.*> lw	a0,0\(a0\)
			204: R_MIPS_GOT_LO16	big_external_common
0+0208 <.*> addu	a0,a0,a1
0+020c <.*> lb	a0,1\(a0\)
0+0210 <.*> lui	a0,0x0
			210: R_MIPS_GOT_HI16	small_external_common
0+0214 <.*> addu	a0,a0,gp
0+0218 <.*> lw	a0,0\(a0\)
			218: R_MIPS_GOT_LO16	small_external_common
0+021c <.*> addu	a0,a0,a1
0+0220 <.*> lb	a0,1\(a0\)
0+0224 <.*> lw	a0,0\(gp\)
			224: R_MIPS_GOT16	\.bss
0+0228 <.*> nop
0+022c <.*> addiu	a0,a0,0
			22c: R_MIPS_LO16	\.bss
0+0230 <.*> addu	a0,a0,a1
0+0234 <.*> lb	a0,1\(a0\)
0+0238 <.*> lw	a0,0\(gp\)
			238: R_MIPS_GOT16	\.bss
0+023c <.*> nop
0+0240 <.*> addiu	a0,a0,1000
			240: R_MIPS_LO16	\.bss
0+0244 <.*> addu	a0,a0,a1
0+0248 <.*> lb	a0,1\(a0\)
0+024c <.*> nop
