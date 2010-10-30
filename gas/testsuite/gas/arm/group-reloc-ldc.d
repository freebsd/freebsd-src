#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#name: Group relocation tests (ldc)

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			10: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\]
			14: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			18: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			1c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			20: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			24: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			28: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\]
			2c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			30: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			34: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			38: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			3c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			40: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\]
			44: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			48: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			4c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			50: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			54: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			58: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\]
			5c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			60: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			64: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			68: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			6c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			70: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\]
			74: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			78: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			7c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			80: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			84: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			88: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\]
			8c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			90: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			94: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			98: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			9c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			a0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\]
			a4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			a8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			ac: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			b0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			b4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			b8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\]
			bc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			c0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			c4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			c8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			cc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			d0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\]
			d4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			d8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			dc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			e0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			e4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			e8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\]
			ec: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			f0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			f4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			f8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			fc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			100: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\]
			104: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			108: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			10c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			110: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			114: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			118: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\]
			11c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			120: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			124: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			128: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			12c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			130: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\]
			134: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			138: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			13c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			140: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			144: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			148: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\]
			14c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			150: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			154: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			158: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			15c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			160: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\]
			164: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			168: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			16c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			170: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			174: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			178: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\]
			17c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			180: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			184: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			188: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			18c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			190: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900185 	ldfs	f0, \[r0, #532\]
			194: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			198: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			19c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			1a0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			1a4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			1a8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800185 	stfs	f0, \[r0, #532\]
			1ac: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1b0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1b4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1b8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1bc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1c0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100185 	ldfs	f0, \[r0, #-532\]
			1c4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1c8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1cc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1d0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1d4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1d8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000185 	stfs	f0, \[r0, #-532\]
			1dc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1e0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1e4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1e8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1ec: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1f0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed908185 	ldfd	f0, \[r0, #532\]
			1f4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			1f8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			1fc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			200: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			204: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			208: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed808185 	stfd	f0, \[r0, #532\]
			20c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			210: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			214: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			218: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			21c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			220: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed108185 	ldfd	f0, \[r0, #-532\]
			224: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			228: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			22c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			230: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			234: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			238: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed008185 	stfd	f0, \[r0, #-532\]
			23c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			240: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			244: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			248: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			24c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			250: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd00185 	ldfe	f0, \[r0, #532\]
			254: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			258: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			25c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			260: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			264: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			268: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc00185 	stfe	f0, \[r0, #532\]
			26c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			270: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			274: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			278: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			27c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			280: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed500185 	ldfe	f0, \[r0, #-532\]
			284: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			288: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			28c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			290: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			294: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			298: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed400185 	stfe	f0, \[r0, #-532\]
			29c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2a0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2a4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2a8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2ac: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2b0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd08185 	ldfp	f0, \[r0, #532\]
			2b4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2b8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2bc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2c0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2c4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2c8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc08185 	stfp	f0, \[r0, #532\]
			2cc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2d0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2d4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2d8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2dc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2e0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed508185 	ldfp	f0, \[r0, #-532\]
			2e4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2e8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2ec: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2f0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2f4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2f8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed408185 	stfp	f0, \[r0, #-532\]
			2fc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			300: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			304: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			308: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			30c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			310: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900a85 	flds	s0, \[r0, #532\]
			314: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			318: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			31c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			320: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			324: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			328: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800a85 	fsts	s0, \[r0, #532\]
			32c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			330: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			334: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			338: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			33c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			340: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100a85 	flds	s0, \[r0, #-532\]
			344: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			348: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			34c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			350: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			354: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			358: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000a85 	fsts	s0, \[r0, #-532\]
			35c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			360: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			364: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			368: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			36c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			370: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			374: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			378: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			37c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			380: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			384: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			388: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			38c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			390: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			394: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			398: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			39c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3a0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3a4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3a8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3ac: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3b0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3b4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3b8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			3bc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3c0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3c4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3c8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3cc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3d0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\]
			3d4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3d8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3dc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3e0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3e4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3e8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\]
			3ec: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3f0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3f4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3f8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			3fc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			400: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\]
			404: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			408: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			40c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			410: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			414: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			418: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\]
			41c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			420: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			424: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			428: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			42c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			430: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900485 	cfldrs	mvf0, \[r0, #532\]
			434: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			438: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			43c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			440: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			444: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			448: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800485 	cfstrs	mvf0, \[r0, #532\]
			44c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			450: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			454: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			458: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			45c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			460: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100485 	cfldrs	mvf0, \[r0, #-532\]
			464: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			468: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			46c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			470: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			474: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			478: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000485 	cfstrs	mvf0, \[r0, #-532\]
			47c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			480: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			484: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			488: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			48c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			490: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd00485 	cfldrd	mvd0, \[r0, #532\]
			494: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			498: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			49c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			4a0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			4a4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			4a8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc00485 	cfstrd	mvd0, \[r0, #532\]
			4ac: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4b0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4b4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4b8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4bc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4c0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed500485 	cfldrd	mvd0, \[r0, #-532\]
			4c4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4c8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4cc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4d0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4d4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4d8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed400485 	cfstrd	mvd0, \[r0, #-532\]
			4dc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4e0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4e4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4e8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4ec: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4f0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900585 	cfldr32	mvfx0, \[r0, #532\]
			4f4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			4f8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			4fc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			500: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			504: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			508: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800585 	cfstr32	mvfx0, \[r0, #532\]
			50c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			510: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			514: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			518: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			51c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			520: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100585 	cfldr32	mvfx0, \[r0, #-532\]
			524: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			528: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			52c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			530: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			534: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			538: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000585 	cfstr32	mvfx0, \[r0, #-532\]
			53c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			540: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			544: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			548: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			54c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			550: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd00585 	cfldr64	mvdx0, \[r0, #532\]
			554: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			558: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			55c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			560: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			564: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			568: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc00585 	cfstr64	mvdx0, \[r0, #532\]
			56c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			570: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			574: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			578: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			57c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			580: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed500585 	cfldr64	mvdx0, \[r0, #-532\]
			584: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			588: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			58c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			590: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			594: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			598: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed400585 	cfstr64	mvdx0, \[r0, #-532\]
			59c: R_ARM_LDC_SB_G2	f
