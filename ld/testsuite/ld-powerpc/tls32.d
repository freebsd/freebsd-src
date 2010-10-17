#source: tls32.s
#source: tlslib32.s
#as: -a32
#ld: -melf32ppc
#objdump: -dr
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Disassembly of section \.text:

0+1800094 <_start>:
 1800094:	3c 62 00 00 	addis   r3,r2,0
 1800098:	38 63 90 3c 	addi    r3,r3,-28612
 180009c:	3c 62 00 00 	addis   r3,r2,0
 18000a0:	38 63 10 00 	addi    r3,r3,4096
 18000a4:	3c 62 00 00 	addis   r3,r2,0
 18000a8:	38 63 90 20 	addi    r3,r3,-28640
 18000ac:	3c 62 00 00 	addis   r3,r2,0
 18000b0:	38 63 10 00 	addi    r3,r3,4096
 18000b4:	39 23 80 24 	addi    r9,r3,-32732
 18000b8:	3d 23 00 00 	addis   r9,r3,0
 18000bc:	81 49 80 28 	lwz     r10,-32728\(r9\)
 18000c0:	3d 22 00 00 	addis   r9,r2,0
 18000c4:	a1 49 90 30 	lhz     r10,-28624\(r9\)
 18000c8:	89 42 90 34 	lbz     r10,-28620\(r2\)
 18000cc:	3d 22 00 00 	addis   r9,r2,0
 18000d0:	99 49 90 38 	stb     r10,-28616\(r9\)
 18000d4:	3c 62 00 00 	addis   r3,r2,0
 18000d8:	38 63 90 00 	addi    r3,r3,-28672
 18000dc:	3c 62 00 00 	addis   r3,r2,0
 18000e0:	38 63 10 00 	addi    r3,r3,4096
 18000e4:	91 43 80 04 	stw     r10,-32764\(r3\)
 18000e8:	3d 23 00 00 	addis   r9,r3,0
 18000ec:	91 49 80 08 	stw     r10,-32760\(r9\)
 18000f0:	3d 22 00 00 	addis   r9,r2,0
 18000f4:	b1 49 90 30 	sth     r10,-28624\(r9\)
 18000f8:	a1 42 90 14 	lhz     r10,-28652\(r2\)
 18000fc:	3d 22 00 00 	addis   r9,r2,0
 1800100:	a9 49 90 18 	lha     r10,-28648\(r9\)

0+1800104 <__tls_get_addr>:
 1800104:	4e 80 00 20 	blr
Disassembly of section \.got:

0+1810128 <_GLOBAL_OFFSET_TABLE_-0x4>:
 1810128:	4e 80 00 21 	blrl

0+181012c <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
