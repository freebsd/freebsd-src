#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#name: Group relocation tests (alu)

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			0: R_ARM_ALU_PC_G0	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			4: R_ARM_ALU_PC_G1	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			8: R_ARM_ALU_PC_G2	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			c: R_ARM_ALU_PC_G0_NC	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			10: R_ARM_ALU_PC_G1_NC	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			14: R_ARM_ALU_SB_G0	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			18: R_ARM_ALU_SB_G1	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			1c: R_ARM_ALU_SB_G2	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			20: R_ARM_ALU_SB_G0_NC	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			24: R_ARM_ALU_SB_G1_NC	f
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			28: R_ARM_ALU_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			2c: R_ARM_ALU_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			30: R_ARM_ALU_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			34: R_ARM_ALU_PC_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			38: R_ARM_ALU_PC_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			3c: R_ARM_ALU_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			40: R_ARM_ALU_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			44: R_ARM_ALU_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			48: R_ARM_ALU_SB_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2800c01 	add	r0, r0, #256	; 0x100
			4c: R_ARM_ALU_SB_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			50: R_ARM_ALU_PC_G0	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			54: R_ARM_ALU_PC_G1	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			58: R_ARM_ALU_PC_G2	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			5c: R_ARM_ALU_PC_G0_NC	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			60: R_ARM_ALU_PC_G1_NC	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			64: R_ARM_ALU_SB_G0	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			68: R_ARM_ALU_SB_G1	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			6c: R_ARM_ALU_SB_G2	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			70: R_ARM_ALU_SB_G0_NC	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			74: R_ARM_ALU_SB_G1_NC	f
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			78: R_ARM_ALU_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			7c: R_ARM_ALU_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			80: R_ARM_ALU_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			84: R_ARM_ALU_PC_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			88: R_ARM_ALU_PC_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			8c: R_ARM_ALU_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			90: R_ARM_ALU_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			94: R_ARM_ALU_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			98: R_ARM_ALU_SB_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2900c01 	adds	r0, r0, #256	; 0x100
			9c: R_ARM_ALU_SB_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			a0: R_ARM_ALU_PC_G0	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			a4: R_ARM_ALU_PC_G1	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			a8: R_ARM_ALU_PC_G2	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			ac: R_ARM_ALU_PC_G0_NC	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			b0: R_ARM_ALU_PC_G1_NC	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			b4: R_ARM_ALU_SB_G0	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			b8: R_ARM_ALU_SB_G1	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			bc: R_ARM_ALU_SB_G2	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			c0: R_ARM_ALU_SB_G0_NC	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			c4: R_ARM_ALU_SB_G1_NC	f
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			c8: R_ARM_ALU_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			cc: R_ARM_ALU_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			d0: R_ARM_ALU_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			d4: R_ARM_ALU_PC_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			d8: R_ARM_ALU_PC_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			dc: R_ARM_ALU_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			e0: R_ARM_ALU_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			e4: R_ARM_ALU_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			e8: R_ARM_ALU_SB_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2400c01 	sub	r0, r0, #256	; 0x100
			ec: R_ARM_ALU_SB_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			f0: R_ARM_ALU_PC_G0	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			f4: R_ARM_ALU_PC_G1	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			f8: R_ARM_ALU_PC_G2	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			fc: R_ARM_ALU_PC_G0_NC	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			100: R_ARM_ALU_PC_G1_NC	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			104: R_ARM_ALU_SB_G0	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			108: R_ARM_ALU_SB_G1	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			10c: R_ARM_ALU_SB_G2	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			110: R_ARM_ALU_SB_G0_NC	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			114: R_ARM_ALU_SB_G1_NC	f
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			118: R_ARM_ALU_PC_G0	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			11c: R_ARM_ALU_PC_G1	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			120: R_ARM_ALU_PC_G2	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			124: R_ARM_ALU_PC_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			128: R_ARM_ALU_PC_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			12c: R_ARM_ALU_SB_G0	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			130: R_ARM_ALU_SB_G1	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			134: R_ARM_ALU_SB_G2	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			138: R_ARM_ALU_SB_G0_NC	localsym
0[0-9a-f]+ <[^>]+> e2500c01 	subs	r0, r0, #256	; 0x100
			13c: R_ARM_ALU_SB_G1_NC	localsym
0[0-9a-f]+ <[^>]+> e3a00000 	mov	r0, #0	; 0x0
