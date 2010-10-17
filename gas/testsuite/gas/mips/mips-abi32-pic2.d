#objdump: -d -mmips:8000 -r --prefix-addresses --show-raw-insn
#as: -march=8000 -EB -mabi=32 -KPIC
#name: MIPS -mabi=32 test 2 (SVR4 PIC)

.*: +file format.*

Disassembly of section \.text:
0+000 <[^>]*> 3c1c0000 	lui	gp,0x0
			0: R_MIPS_HI16	_gp_disp
0+004 <[^>]*> 279c0000 	addiu	gp,gp,0
			4: R_MIPS_LO16	_gp_disp
0+008 <[^>]*> 0399e021 	addu	gp,gp,t9
0+00c <[^>]*> afbc0008 	sw	gp,8\(sp\)
0+010 <[^>]*> 8f990000 	lw	t9,0\(gp\)
			10: R_MIPS_GOT16	\.text
0+014 <[^>]*> 00000000 	nop
0+018 <[^>]*> 273900d8 	addiu	t9,t9,216
			18: R_MIPS_LO16	\.text
0+01c <[^>]*> 0320f809 	jalr	t9
0+020 <[^>]*> 00000000 	nop
0+024 <[^>]*> 8fbc0008 	lw	gp,8\(sp\)
0+028 <[^>]*> 00000000 	nop
0+02c <[^>]*> 0320f809 	jalr	t9
0+030 <[^>]*> 00000000 	nop
0+034 <[^>]*> 8fbc0008 	lw	gp,8\(sp\)
0+038 <[^>]*> 3c1c0000 	lui	gp,0x0
			38: R_MIPS_HI16	_gp_disp
0+03c <[^>]*> 279c0000 	addiu	gp,gp,0
			3c: R_MIPS_LO16	_gp_disp
0+040 <[^>]*> 0399e021 	addu	gp,gp,t9
0+044 <[^>]*> 3c010001 	lui	at,0x1
0+048 <[^>]*> 003d0821 	addu	at,at,sp
0+04c <[^>]*> ac3c8000 	sw	gp,-32768\(at\)
0+050 <[^>]*> 8f990000 	lw	t9,0\(gp\)
			50: R_MIPS_GOT16	\.text
0+054 <[^>]*> 00000000 	nop
0+058 <[^>]*> 273900d8 	addiu	t9,t9,216
			58: R_MIPS_LO16	\.text
0+05c <[^>]*> 0320f809 	jalr	t9
0+060 <[^>]*> 00000000 	nop
0+064 <[^>]*> 3c010001 	lui	at,0x1
0+068 <[^>]*> 003d0821 	addu	at,at,sp
0+06c <[^>]*> 8c3c8000 	lw	gp,-32768\(at\)
0+070 <[^>]*> 00000000 	nop
0+074 <[^>]*> 0320f809 	jalr	t9
0+078 <[^>]*> 00000000 	nop
0+07c <[^>]*> 3c010001 	lui	at,0x1
0+080 <[^>]*> 003d0821 	addu	at,at,sp
0+084 <[^>]*> 8c3c8000 	lw	gp,-32768\(at\)
0+088 <[^>]*> 3c1c0000 	lui	gp,0x0
			88: R_MIPS_HI16	_gp_disp
0+08c <[^>]*> 279c0000 	addiu	gp,gp,0
			8c: R_MIPS_LO16	_gp_disp
0+090 <[^>]*> 0399e021 	addu	gp,gp,t9
0+094 <[^>]*> 3c010001 	lui	at,0x1
0+098 <[^>]*> 003d0821 	addu	at,at,sp
0+09c <[^>]*> ac3c0000 	sw	gp,0\(at\)
0+0a0 <[^>]*> 8f990000 	lw	t9,0\(gp\)
			a0: R_MIPS_GOT16	\.text
0+0a4 <[^>]*> 00000000 	nop
0+0a8 <[^>]*> 273900d8 	addiu	t9,t9,216
			a8: R_MIPS_LO16	\.text
0+0ac <[^>]*> 0320f809 	jalr	t9
0+0b0 <[^>]*> 00000000 	nop
0+0b4 <[^>]*> 3c010001 	lui	at,0x1
0+0b8 <[^>]*> 003d0821 	addu	at,at,sp
0+0bc <[^>]*> 8c3c0000 	lw	gp,0\(at\)
0+0c0 <[^>]*> 00000000 	nop
0+0c4 <[^>]*> 0320f809 	jalr	t9
0+0c8 <[^>]*> 00000000 	nop
0+0cc <[^>]*> 3c010001 	lui	at,0x1
0+0d0 <[^>]*> 003d0821 	addu	at,at,sp
0+0d4 <[^>]*> 8c3c0000 	lw	gp,0\(at\)
	\.\.\.
