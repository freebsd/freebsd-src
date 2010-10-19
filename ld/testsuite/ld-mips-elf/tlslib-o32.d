
.*:     file format elf32-tradbigmips

Disassembly of section .text:

.* <fn>:
 .*:	3c1c0005 	lui	gp,0x5
 .*:	279c80a0 	addiu	gp,gp,-32608
 .*:	0399e021 	addu	gp,gp,t9
 .*:	27bdfff0 	addiu	sp,sp,-16
 .*:	afbe0008 	sw	s8,8\(sp\)
 .*:	03a0f021 	move	s8,sp
 .*:	afbc0000 	sw	gp,0\(sp\)
 .*:	8f99802c 	lw	t9,-32724\(gp\)
 .*:	2784803c 	addiu	a0,gp,-32708
 .*:	0320f809 	jalr	t9
 .*:	00000000 	nop
 .*:	8fdc0000 	lw	gp,0\(s8\)
 .*:	00000000 	nop
 .*:	8f99802c 	lw	t9,-32724\(gp\)
 .*:	27848034 	addiu	a0,gp,-32716
 .*:	0320f809 	jalr	t9
 .*:	00000000 	nop
 .*:	8fdc0000 	lw	gp,0\(s8\)
 .*:	00401021 	move	v0,v0
 .*:	3c030000 	lui	v1,0x0
 .*:	24638000 	addiu	v1,v1,-32768
 .*:	00621821 	addu	v1,v1,v0
 .*:	7c02283b 	rdhwr	v0,\$5
 .*:	8f838030 	lw	v1,-32720\(gp\)
 .*:	00000000 	nop
 .*:	00621821 	addu	v1,v1,v0
 .*:	03c0e821 	move	sp,s8
 .*:	8fbe0008 	lw	s8,8\(sp\)
 .*:	03e00008 	jr	ra
 .*:	27bd0010 	addiu	sp,sp,16
	...
Disassembly of section .MIPS.stubs:

.* <.MIPS.stubs>:
 .*:	8f998010 	lw	t9,-32752\(gp\)
 .*:	03e07821 	move	t7,ra
 .*:	0320f809 	jalr	t9
 .*:	241800.* 	li	t8,.*
	...
