#as: -mips2 -mvxworks-pic -xgot -mabi=32 -EB
#source: vxworks1.s
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

00000000 <\.text>:
#
# la $4,local
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT_HI16	\.data
.*:	009c2021 	addu	a0,a0,gp
.*:	8c840000 	lw	a0,0\(a0\)
			.*: R_MIPS_GOT_LO16	\.data
#
# la $4,global
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT_HI16	global
.*:	009c2021 	addu	a0,a0,gp
.*:	8c840000 	lw	a0,0\(a0\)
			.*: R_MIPS_GOT_LO16	global
#
# lw $4,local
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT_HI16	\.data
.*:	009c2021 	addu	a0,a0,gp
.*:	8c840000 	lw	a0,0\(a0\)
			.*: R_MIPS_GOT_LO16	\.data
.*:	8c840000 	lw	a0,0\(a0\)
#
# lw $4,global
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT_HI16	global
.*:	009c2021 	addu	a0,a0,gp
.*:	8c840000 	lw	a0,0\(a0\)
			.*: R_MIPS_GOT_LO16	global
.*:	8c840000 	lw	a0,0\(a0\)
#
# sw $4,local
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	\.data
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	\.data
.*:	ac240000 	sw	a0,0\(at\)
#
# sw $4,global
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	global
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	global
.*:	ac240000 	sw	a0,0\(at\)
#
# ulw $4,local
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	\.data
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	\.data
.*:	88240000 	lwl	a0,0\(at\)
.*:	98240003 	lwr	a0,3\(at\)
#
# ulw $4,global
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	global
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	global
.*:	88240000 	lwl	a0,0\(at\)
.*:	98240003 	lwr	a0,3\(at\)
#
# usw $4,local
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	\.data
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	\.data
.*:	a8240000 	swl	a0,0\(at\)
.*:	b8240003 	swr	a0,3\(at\)
#
# usw $4,global
#
.*:	3c010000 	lui	at,0x0
			.*: R_MIPS_GOT_HI16	global
.*:	003c0821 	addu	at,at,gp
.*:	8c210000 	lw	at,0\(at\)
			.*: R_MIPS_GOT_LO16	global
.*:	a8240000 	swl	a0,0\(at\)
.*:	b8240003 	swr	a0,3\(at\)
	\.\.\.
