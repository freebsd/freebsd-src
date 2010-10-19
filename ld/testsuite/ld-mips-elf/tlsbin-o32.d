.*:     file format elf32-tradbigmips

Disassembly of section .text:

004000d0 <__start>:
  4000d0:	3c1c0fc0 	lui	gp,0xfc0
  4000d4:	279c7f30 	addiu	gp,gp,32560
  4000d8:	0399e021 	addu	gp,gp,t9
  4000dc:	27bdfff0 	addiu	sp,sp,-16
  4000e0:	afbe0008 	sw	s8,8\(sp\)
  4000e4:	03a0f021 	move	s8,sp
  4000e8:	afbc0000 	sw	gp,0\(sp\)
  4000ec:	8f99802c 	lw	t9,-32724\(gp\)
  4000f0:	2784803c 	addiu	a0,gp,-32708
  4000f4:	0320f809 	jalr	t9
  4000f8:	00000000 	nop
  4000fc:	8fdc0000 	lw	gp,0\(s8\)
  400100:	00000000 	nop
  400104:	8f99802c 	lw	t9,-32724\(gp\)
  400108:	27848034 	addiu	a0,gp,-32716
  40010c:	0320f809 	jalr	t9
  400110:	00000000 	nop
  400114:	8fdc0000 	lw	gp,0\(s8\)
  400118:	00401021 	move	v0,v0
  40011c:	3c030000 	lui	v1,0x0
  400120:	24638000 	addiu	v1,v1,-32768
  400124:	00621821 	addu	v1,v1,v0
  400128:	7c02283b 	rdhwr	v0,\$5
  40012c:	8f838030 	lw	v1,-32720\(gp\)
  400130:	00000000 	nop
  400134:	00621821 	addu	v1,v1,v0
  400138:	7c02283b 	rdhwr	v0,\$5
  40013c:	3c030000 	lui	v1,0x0
  400140:	24639004 	addiu	v1,v1,-28668
  400144:	00621821 	addu	v1,v1,v0
  400148:	03c0e821 	move	sp,s8
  40014c:	8fbe0008 	lw	s8,8\(sp\)
  400150:	03e00008 	jr	ra
  400154:	27bd0010 	addiu	sp,sp,16

00400158 <__tls_get_addr>:
  400158:	03e00008 	jr	ra
  40015c:	00000000 	nop
