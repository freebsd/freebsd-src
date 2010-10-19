#source: tls32.s
#as: -a32
#ld: -melf32ppc tmpdir/libtlslib32.so
#objdump: -dr
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Disassembly of section \.text:

.* <_start>:
.*:	80 7f ff f0 	lwz     r3,-16\(r31\)
.*:	7c 63 12 14 	add     r3,r3,r2
.*:	38 7f ff f4 	addi    r3,r31,-12
.*:	48 01 01 85 	bl      .*<__tls_get_addr@plt>
.*:	3c 62 00 00 	addis   r3,r2,0
.*:	38 63 90 1c 	addi    r3,r3,-28644
.*:	3c 62 00 00 	addis   r3,r2,0
.*:	38 63 10 00 	addi    r3,r3,4096
.*:	39 23 80 20 	addi    r9,r3,-32736
.*:	3d 23 00 00 	addis   r9,r3,0
.*:	81 49 80 24 	lwz     r10,-32732\(r9\)
.*:	3d 22 00 00 	addis   r9,r2,0
.*:	a1 49 90 2c 	lhz     r10,-28628\(r9\)
.*:	89 42 90 30 	lbz     r10,-28624\(r2\)
.*:	3d 22 00 00 	addis   r9,r2,0
.*:	99 49 90 34 	stb     r10,-28620\(r9\)
.*:	3c 62 00 00 	addis   r3,r2,0
.*:	38 63 90 00 	addi    r3,r3,-28672
.*:	3c 62 00 00 	addis   r3,r2,0
.*:	38 63 10 00 	addi    r3,r3,4096
.*:	91 43 80 04 	stw     r10,-32764\(r3\)
.*:	3d 23 00 00 	addis   r9,r3,0
.*:	91 49 80 08 	stw     r10,-32760\(r9\)
.*:	3d 22 00 00 	addis   r9,r2,0
.*:	b1 49 90 2c 	sth     r10,-28628\(r9\)
.*:	a1 42 90 14 	lhz     r10,-28652\(r2\)
.*:	3d 22 00 00 	addis   r9,r2,0
.*:	a9 49 90 18 	lha     r10,-28648\(r9\)
Disassembly of section \.got:

.* <_GLOBAL_OFFSET_TABLE_-0x10>:
	\.\.\.
.*:	4e 80 00 21 	blrl

.* <_GLOBAL_OFFSET_TABLE_>:
.*:	01 81 02 b4 00 00 00 00 00 00 00 00  .*
