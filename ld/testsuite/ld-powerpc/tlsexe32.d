#source: tls32.s
#as: -a32
#ld: -melf32ppc tmpdir/libtlslib32.so
#objdump: -dr
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Disassembly of section \.text:

0180028c <_start>:
 180028c:	80 7f 00 0c 	lwz     r3,12\(r31\)
 1800290:	7c 63 12 14 	add     r3,r3,r2
 1800294:	38 7f 00 10 	addi    r3,r31,16
 1800298:	48 01 01 85 	bl      181041c .*
 180029c:	3c 62 00 00 	addis   r3,r2,0
 18002a0:	38 63 90 1c 	addi    r3,r3,-28644
 18002a4:	3c 62 00 00 	addis   r3,r2,0
 18002a8:	38 63 10 00 	addi    r3,r3,4096
 18002ac:	39 23 80 20 	addi    r9,r3,-32736
 18002b0:	3d 23 00 00 	addis   r9,r3,0
 18002b4:	81 49 80 24 	lwz     r10,-32732\(r9\)
 18002b8:	3d 22 00 00 	addis   r9,r2,0
 18002bc:	a1 49 90 2c 	lhz     r10,-28628\(r9\)
 18002c0:	89 42 90 30 	lbz     r10,-28624\(r2\)
 18002c4:	3d 22 00 00 	addis   r9,r2,0
 18002c8:	99 49 90 34 	stb     r10,-28620\(r9\)
 18002cc:	3c 62 00 00 	addis   r3,r2,0
 18002d0:	38 63 90 00 	addi    r3,r3,-28672
 18002d4:	3c 62 00 00 	addis   r3,r2,0
 18002d8:	38 63 10 00 	addi    r3,r3,4096
 18002dc:	91 43 80 04 	stw     r10,-32764\(r3\)
 18002e0:	3d 23 00 00 	addis   r9,r3,0
 18002e4:	91 49 80 08 	stw     r10,-32760\(r9\)
 18002e8:	3d 22 00 00 	addis   r9,r2,0
 18002ec:	b1 49 90 2c 	sth     r10,-28628\(r9\)
 18002f0:	a1 42 90 14 	lhz     r10,-28652\(r2\)
 18002f4:	3d 22 00 00 	addis   r9,r2,0
 18002f8:	a9 49 90 18 	lha     r10,-28648\(r9\)
Disassembly of section \.got:

018103b8 <\.got>:
 18103b8:	4e 80 00 21 	blrl
 18103bc:	01 81 03 18 	\.long 0x1810318
	\.\.\.
Disassembly of section \.plt:

018103d4 <\.plt>:
	\.\.\.
