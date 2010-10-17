#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#objdump: -dr
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Disassembly of section \.text:

0+538 <_start>:
 538:	38 7f 00 1c 	addi    r3,r31,28
 53c:	48 00 00 01 	bl      53c .*
 540:	38 7f 00 0c 	addi    r3,r31,12
 544:	48 00 00 01 	bl      544 .*
 548:	38 7f 00 24 	addi    r3,r31,36
 54c:	48 01 01 95 	bl      106e0 .*
 550:	38 7f 00 0c 	addi    r3,r31,12
 554:	48 01 01 8d 	bl      106e0 .*
 558:	39 23 80 20 	addi    r9,r3,-32736
 55c:	3d 23 00 00 	addis   r9,r3,0
 560:	81 49 80 24 	lwz     r10,-32732\(r9\)
 564:	81 3f 00 2c 	lwz     r9,44\(r31\)
 568:	7d 49 12 2e 	lhzx    r10,r9,r2
 56c:	89 42 00 00 	lbz     r10,0\(r2\)
 570:	3d 22 00 00 	addis   r9,r2,0
 574:	99 49 00 00 	stb     r10,0\(r9\)
 578:	38 7e 00 14 	addi    r3,r30,20
 57c:	48 00 00 01 	bl      57c .*
 580:	38 7e 00 0c 	addi    r3,r30,12
 584:	48 00 00 01 	bl      584 .*
 588:	91 43 80 04 	stw     r10,-32764\(r3\)
 58c:	3d 23 00 00 	addis   r9,r3,0
 590:	91 49 80 08 	stw     r10,-32760\(r9\)
 594:	81 3e 00 2c 	lwz     r9,44\(r30\)
 598:	7d 49 13 2e 	sthx    r10,r9,r2
 59c:	a1 42 00 00 	lhz     r10,0\(r2\)
 5a0:	3d 22 00 00 	addis   r9,r2,0
 5a4:	a9 49 00 00 	lha     r10,0\(r9\)
Disassembly of section \.got:

00010664 <\.got>:
   10664:	4e 80 00 21 	blrl
   10668:	00 01 05 c4 	\.long 0x105c4
	\.\.\.
Disassembly of section \.plt:

00010698 <\.plt>:
	\.\.\.
