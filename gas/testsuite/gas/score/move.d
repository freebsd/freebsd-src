#as:
#objdump: -d
#source: move.s

.*: +file format .*

Disassembly of section \.text:

00000000 <.text>:
   0:	00f3      	mv!		r0, r15
   2:	00f3      	mv!		r0, r15
   4:	0ff3      	mv!		r15, r15
   6:	0ff3      	mv!		r15, r15
   8:	0353      	mv!		r3, r5
   a:	0353      	mv!		r3, r5
   c:	0673      	mv!		r6, r7
   e:	0673      	mv!		r6, r7
  10:	810abc56 	mv		r8, r10
  14:	82b7bc56 	mv		r21, r23
	...
  20:	800fbc56 	mv		r0, r15
  24:	82fbbc56 	mv		r23, r27
  28:	0283      	mv!		r2, r8
  2a:	0283      	mv!		r2, r8
  2c:	0283      	mv!		r2, r8
  2e:	0283      	mv!		r2, r8
  30:	0f02      	mhfl!		r31, r0
  32:	0f02      	mhfl!		r31, r0
  34:	00f2      	mhfl!		r16, r15
  36:	00f2      	mhfl!		r16, r15
  38:	0752      	mhfl!		r23, r5
  3a:	0752      	mhfl!		r23, r5
  3c:	0a72      	mhfl!		r26, r7
  3e:	0a72      	mhfl!		r26, r7
  40:	838abc56 	mv		gp, r10
  44:	82b7bc56 	mv		r21, r23
	...
  50:	83e0bc56 	mv		r31, r0
  54:	82fbbc56 	mv		r23, r27
  58:	0682      	mhfl!		r22, r8
  5a:	0682      	mhfl!		r22, r8
  5c:	07f2      	mhfl!		r23, r15
  5e:	07f2      	mhfl!		r23, r15
  60:	00f1      	mlfh!		r0, r31
  62:	00f1      	mlfh!		r0, r31
  64:	0f01      	mlfh!		r15, r16
  66:	0f01      	mlfh!		r15, r16
  68:	0571      	mlfh!		r5, r23
  6a:	0571      	mlfh!		r5, r23
  6c:	07a1      	mlfh!		r7, r26
  6e:	07a1      	mlfh!		r7, r26
  70:	815cbc56 	mv		r10, gp
  74:	82b7bc56 	mv		r21, r23
	...
  80:	801fbc56 	mv		r0, r31
  84:	82fbbc56 	mv		r23, r27
  88:	0861      	mlfh!		r8, r22
  8a:	0861      	mlfh!		r8, r22
  8c:	0f71      	mlfh!		r15, r23
  8e:	0f71      	mlfh!		r15, r23
