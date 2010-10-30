#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#name: Group relocation tests (ldrs)

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			0: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			4: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			8: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			c: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			10: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			14: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			18: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			1c: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			20: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			24: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			28: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			2c: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			30: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			34: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			38: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			3c: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			40: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			44: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			48: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			4c: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			50: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			54: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			58: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			5c: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			60: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			64: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			68: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			6c: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			70: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			74: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			78: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			7c: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			80: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			84: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			88: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			8c: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			90: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			94: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			98: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			9c: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			a0: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			a4: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			a8: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			ac: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			b0: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			b4: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			b8: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			bc: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			c0: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			c4: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			c8: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			cc: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			d0: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			d4: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			d8: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			dc: R_ARM_LDRS_PC_G1	f
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			e0: R_ARM_LDRS_PC_G2	f
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			e4: R_ARM_LDRS_SB_G0	f
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			e8: R_ARM_LDRS_SB_G1	f
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			ec: R_ARM_LDRS_SB_G2	f
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			f0: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			f4: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			f8: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			fc: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fdf 	ldrd	r0, \[r0, #255\]
			100: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			104: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			108: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			10c: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			110: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fff 	strd	r0, \[r0, #255\]
			114: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			118: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			11c: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			120: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			124: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fbf 	ldrh	r0, \[r0, #255\]
			128: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			12c: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			130: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			134: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			138: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1c00fbf 	strh	r0, \[r0, #255\]
			13c: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			140: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			144: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			148: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			14c: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fff 	ldrsh	r0, \[r0, #255\]
			150: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			154: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			158: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			15c: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			160: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1d00fdf 	ldrsb	r0, \[r0, #255\]
			164: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			168: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			16c: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			170: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			174: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fdf 	ldrd	r0, \[r0, #-255\]
			178: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			17c: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			180: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			184: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			188: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fff 	strd	r0, \[r0, #-255\]
			18c: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			190: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			194: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			198: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			19c: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fbf 	ldrh	r0, \[r0, #-255\]
			1a0: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			1a4: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			1a8: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			1ac: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			1b0: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1400fbf 	strh	r0, \[r0, #-255\]
			1b4: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			1b8: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			1bc: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			1c0: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			1c4: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fff 	ldrsh	r0, \[r0, #-255\]
			1c8: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			1cc: R_ARM_LDRS_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			1d0: R_ARM_LDRS_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			1d4: R_ARM_LDRS_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			1d8: R_ARM_LDRS_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e1500fdf 	ldrsb	r0, \[r0, #-255\]
			1dc: R_ARM_LDRS_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e3a00000 	mov	r0, #0	; 0x0
