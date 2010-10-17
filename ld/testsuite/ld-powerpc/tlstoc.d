#source: tlslib.s
#source: tlstoc.s
#as: -a64
#ld: -melf64ppc
#objdump: -dr
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Disassembly of section \.text:

.* <\.__tls_get_addr>:
.*	4e 80 00 20 	blr

.* <_start>:
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 90 40 	addi    r3,r3,-28608
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 10 00 	addi    r3,r3,4096
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 90 48 	addi    r3,r3,-28600
.*	3c 6d 00 00 	addis   r3,r13,0
.*	60 00 00 00 	nop
.*	38 63 10 00 	addi    r3,r3,4096
.*	39 23 80 50 	addi    r9,r3,-32688
.*	3d 23 00 00 	addis   r9,r3,0
.*	81 49 80 58 	lwz     r10,-32680\(r9\)
.*	e9 22 80 40 	ld      r9,-32704\(r2\)
.*	7d 49 18 2a 	ldx     r10,r9,r3
.*	3d 2d 00 00 	addis   r9,r13,0
.*	a1 49 90 68 	lhz     r10,-28568\(r9\)
.*	89 4d 90 70 	lbz     r10,-28560\(r13\)
.*	3d 2d 00 00 	addis   r9,r13,0
.*	99 49 90 78 	stb     r10,-28552\(r9\)
