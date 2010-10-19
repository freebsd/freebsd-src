#as: -EB -march=mips1 -mabi=32
#objdump: -dr
#name: MIPS ELF TLS o32

dump.o:     file format elf32-.*bigmips

Disassembly of section .text:

00000000 <fn>:
   0:	3c1c0000 	lui	gp,0x0
			0: R_MIPS_HI16	_gp_disp
   4:	279c0000 	addiu	gp,gp,0
			4: R_MIPS_LO16	_gp_disp
   8:	0399e021 	addu	gp,gp,t9
   c:	27bdfff0 	addiu	sp,sp,-16
  10:	afbe0008 	sw	s8,8\(sp\)
  14:	03a0f021 	move	s8,sp
  18:	afbc0000 	sw	gp,0\(sp\)
  1c:	8f990000 	lw	t9,0\(gp\)
			1c: R_MIPS_CALL16	__tls_get_addr
  20:	27840000 	addiu	a0,gp,0
			20: R_MIPS_TLS_GD	tlsvar_gd
  24:	0320f809 	jalr	t9
  28:	00000000 	nop
  2c:	8fdc0000 	lw	gp,0\(s8\)
  30:	00000000 	nop
  34:	8f990000 	lw	t9,0\(gp\)
			34: R_MIPS_CALL16	__tls_get_addr
  38:	27840000 	addiu	a0,gp,0
			38: R_MIPS_TLS_LDM	tlsvar_ld
  3c:	0320f809 	jalr	t9
  40:	00000000 	nop
  44:	8fdc0000 	lw	gp,0\(s8\)
  48:	00401021 	move	v0,v0
  4c:	3c030000 	lui	v1,0x0
			4c: R_MIPS_TLS_DTPREL_HI16	tlsvar_ld
  50:	24630000 	addiu	v1,v1,0
			50: R_MIPS_TLS_DTPREL_LO16	tlsvar_ld
  54:	00621821 	addu	v1,v1,v0
  58:	7c02283b 	0x7c02283b
  5c:	8f830000 	lw	v1,0\(gp\)
			5c: R_MIPS_TLS_GOTTPREL	tlsvar_ie
  60:	00000000 	nop
  64:	00621821 	addu	v1,v1,v0
  68:	7c02283b 	0x7c02283b
  6c:	3c030000 	lui	v1,0x0
			6c: R_MIPS_TLS_TPREL_HI16	tlsvar_le
  70:	34630000 	ori	v1,v1,0x0
			70: R_MIPS_TLS_TPREL_LO16	tlsvar_le
  74:	00621821 	addu	v1,v1,v0
  78:	03c0e821 	move	sp,s8
  7c:	8fbe0008 	lw	s8,8\(sp\)
  80:	03e00008 	jr	ra
  84:	27bd0010 	addiu	sp,sp,16
#pass
