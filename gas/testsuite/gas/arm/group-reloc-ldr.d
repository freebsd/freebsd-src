#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#name: Group relocation tests (ldr)

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			0: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			4: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			8: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			c: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			10: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			14: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			18: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			1c: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			20: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			24: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			28: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			2c: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			30: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			34: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			38: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			3c: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			40: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			44: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			48: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			4c: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			50: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			54: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			58: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			5c: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			60: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			64: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			68: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			6c: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			70: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			74: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			78: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			7c: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			80: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			84: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			88: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			8c: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			90: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			94: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			98: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			9c: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			a0: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			a4: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			a8: R_ARM_LDR_PC_G0	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			ac: R_ARM_LDR_PC_G1	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			b0: R_ARM_LDR_PC_G2	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			b4: R_ARM_LDR_SB_G0	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			b8: R_ARM_LDR_SB_G1	f
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			bc: R_ARM_LDR_SB_G2	f
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			c0: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			c4: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			c8: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			cc: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			d0: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5900fff 	ldr	r0, \[r0, #4095\]
			d4: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			d8: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			dc: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			e0: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			e4: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			e8: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5800fff 	str	r0, \[r0, #4095\]
			ec: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			f0: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			f4: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			f8: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			fc: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			100: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5d00fff 	ldrb	r0, \[r0, #4095\]
			104: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			108: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			10c: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			110: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			114: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			118: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5c00fff 	strb	r0, \[r0, #4095\]
			11c: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			120: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			124: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			128: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			12c: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			130: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5100fff 	ldr	r0, \[r0, #-4095\]
			134: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			138: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			13c: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			140: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			144: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			148: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5000fff 	str	r0, \[r0, #-4095\]
			14c: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			150: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			154: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			158: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			15c: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			160: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5500fff 	ldrb	r0, \[r0, #-4095\]
			164: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			168: R_ARM_LDR_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			16c: R_ARM_LDR_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			170: R_ARM_LDR_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			174: R_ARM_LDR_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			178: R_ARM_LDR_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e5400fff 	strb	r0, \[r0, #-4095\]
			17c: R_ARM_LDR_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e3a00000 	mov	r0, #0	; 0x0
