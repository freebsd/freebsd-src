#as: -mips2 -mvxworks-pic -mabi=32 -EL
#source: vxworks1.s
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

00000000 <\.text>:
#
# la $4,local
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	\.data
#
# la $4,global
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	global
#
# lw $4,local
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	\.data
.*:	8c840000 	lw	a0,0\(a0\)
#
# lw $4,global
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	global
.*:	8c840000 	lw	a0,0\(a0\)
#
# sw $4,local
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	\.data
.*:	ac240000 	sw	a0,0\(at\)
#
# sw $4,global
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	global
.*:	ac240000 	sw	a0,0\(at\)
#
# ulw $4,local
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	\.data
.*:	88240003 	lwl	a0,3\(at\)
.*:	98240000 	lwr	a0,0\(at\)
#
# ulw $4,global
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	global
.*:	88240003 	lwl	a0,3\(at\)
.*:	98240000 	lwr	a0,0\(at\)
#
# usw $4,local
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	\.data
.*:	a8240003 	swl	a0,3\(at\)
.*:	b8240000 	swr	a0,0\(at\)
#
# usw $4,global
#
.*:	8f810000 	lw	at,0\(gp\)
			.*: R_MIPS_GOT16	global
.*:	a8240003 	swl	a0,3\(at\)
.*:	b8240000 	swr	a0,0\(at\)
	\.\.\.
