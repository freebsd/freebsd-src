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
0+014 <[^>]*> 273900cc 	addiu	t9,t9,204
			14: R_MIPS_LO16	\.text
0+018 <[^>]*> 0320f809 	jalr	t9
0+01c <[^>]*> 00000000 	nop
0+020 <[^>]*> 8fbc0008 	lw	gp,8\(sp\)
0+024 <[^>]*> 00000000 	nop
0+028 <[^>]*> 0320f809 	jalr	t9
0+02c <[^>]*> 00000000 	nop
0+030 <[^>]*> 8fbc0008 	lw	gp,8\(sp\)
0+034 <[^>]*> 3c1c0000 	lui	gp,0x0
			34: R_MIPS_HI16	_gp_disp
0+038 <[^>]*> 279c0000 	addiu	gp,gp,0
			38: R_MIPS_LO16	_gp_disp
0+03c <[^>]*> 0399e021 	addu	gp,gp,t9
0+040 <[^>]*> 3c010001 	lui	at,0x1
0+044 <[^>]*> 003d0821 	addu	at,at,sp
0+048 <[^>]*> ac3c8000 	sw	gp,-32768\(at\)
0+04c <[^>]*> 8f990000 	lw	t9,0\(gp\)
			4c: R_MIPS_GOT16	\.text
0+050 <[^>]*> 273900cc 	addiu	t9,t9,204
			50: R_MIPS_LO16	\.text
0+054 <[^>]*> 0320f809 	jalr	t9
0+058 <[^>]*> 00000000 	nop
0+05c <[^>]*> 3c010001 	lui	at,0x1
0+060 <[^>]*> 003d0821 	addu	at,at,sp
0+064 <[^>]*> 8c3c8000 	lw	gp,-32768\(at\)
0+068 <[^>]*> 00000000 	nop
0+06c <[^>]*> 0320f809 	jalr	t9
0+070 <[^>]*> 00000000 	nop
0+074 <[^>]*> 3c010001 	lui	at,0x1
0+078 <[^>]*> 003d0821 	addu	at,at,sp
0+07c <[^>]*> 8c3c8000 	lw	gp,-32768\(at\)
0+080 <[^>]*> 3c1c0000 	lui	gp,0x0
			80: R_MIPS_HI16	_gp_disp
0+084 <[^>]*> 279c0000 	addiu	gp,gp,0
			84: R_MIPS_LO16	_gp_disp
0+088 <[^>]*> 0399e021 	addu	gp,gp,t9
0+08c <[^>]*> 3c010001 	lui	at,0x1
0+090 <[^>]*> 003d0821 	addu	at,at,sp
0+094 <[^>]*> ac3c0000 	sw	gp,0\(at\)
0+098 <[^>]*> 8f990000 	lw	t9,0\(gp\)
			98: R_MIPS_GOT16	\.text
0+09c <[^>]*> 273900cc 	addiu	t9,t9,204
			9c: R_MIPS_LO16	\.text
0+0a0 <[^>]*> 0320f809 	jalr	t9
0+0a4 <[^>]*> 00000000 	nop
0+0a8 <[^>]*> 3c010001 	lui	at,0x1
0+0ac <[^>]*> 003d0821 	addu	at,at,sp
0+0b0 <[^>]*> 8c3c0000 	lw	gp,0\(at\)
0+0b4 <[^>]*> 00000000 	nop
0+0b8 <[^>]*> 0320f809 	jalr	t9
0+0bc <[^>]*> 00000000 	nop
0+0c0 <[^>]*> 3c010001 	lui	at,0x1
0+0c4 <[^>]*> 003d0821 	addu	at,at,sp
0+0c8 <[^>]*> 8c3c0000 	lw	gp,0\(at\)
	\.\.\.
