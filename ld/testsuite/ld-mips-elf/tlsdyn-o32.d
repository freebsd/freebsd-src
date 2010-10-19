
.*:     file format elf32-tradbigmips

Disassembly of section .text:

.* <__start>:
  .*:	3c1c0fc0 	lui	gp,0xfc0
  .*:	279c7bb0 	addiu	gp,gp,31664
  .*:	0399e021 	addu	gp,gp,t9
  .*:	27bdfff0 	addiu	sp,sp,-16
  .*:	afbe0008 	sw	s8,8\(sp\)
  .*:	03a0f021 	move	s8,sp
  .*:	afbc0000 	sw	gp,0\(sp\)
  .*:	8f99802c 	lw	t9,-32724\(gp\)
  .*:	27848038 	addiu	a0,gp,-32712
  .*:	0320f809 	jalr	t9
  .*:	00000000 	nop
  .*:	8fdc0000 	lw	gp,0\(s8\)
  .*:	00000000 	nop
  .*:	8f99802c 	lw	t9,-32724\(gp\)
  .*:	27848048 	addiu	a0,gp,-32696
  .*:	0320f809 	jalr	t9
  .*:	00000000 	nop
  .*:	8fdc0000 	lw	gp,0\(s8\)
  .*:	00000000 	nop
  .*:	8f99802c 	lw	t9,-32724\(gp\)
  .*:	27848030 	addiu	a0,gp,-32720
  .*:	0320f809 	jalr	t9
  .*:	00000000 	nop
  .*:	8fdc0000 	lw	gp,0\(s8\)
  .*:	00401021 	move	v0,v0
  .*:	3c030000 	lui	v1,0x0
  .*:	24638000 	addiu	v1,v1,-32768
  .*:	00621821 	addu	v1,v1,v0
  .*:	7c02283b 	rdhwr	v0,\$5
  .*:	8f838044 	lw	v1,-32700\(gp\)
  .*:	00000000 	nop
  .*:	00621821 	addu	v1,v1,v0
  .*:	8f838040 	lw	v1,-32704\(gp\)
  .*:	00000000 	nop
  .*:	00621821 	addu	v1,v1,v0
  .*:	7c02283b 	rdhwr	v0,\$5
  .*:	3c030000 	lui	v1,0x0
  .*:	24639004 	addiu	v1,v1,-28668
  .*:	00621821 	addu	v1,v1,v0
  .*:	03c0e821 	move	sp,s8
  .*:	8fbe0008 	lw	s8,8\(sp\)
  .*:	03e00008 	jr	ra
  .*:	27bd0010 	addiu	sp,sp,16

.* <__tls_get_addr>:
  .*:	03e00008 	jr	ra
  .*:	00000000 	nop
	...
Disassembly of section .MIPS.stubs:

.* <.MIPS.stubs>:
	...
