#objdump: -dr
#name: V850E split LO16 tests
#as: -mv850e
#...
00000000 <.*>:
   0:	40 0e 00 00 	movhi	0, r0, r1
			2: R_V850_HI16_S	foo
   4:	01 16 00 00 	addi	0, r1, r2
			6: R_V850_LO16	foo
   8:	01 17 00 00 	ld\.b	0\[r1\],r2
			a: R_V850_LO16	foo
   c:	81 17 01 00 	ld\.bu	0\[r1\],r2
			c: R_V850_LO16_SPLIT_OFFSET	foo
  10:	a1 17 45 23 	ld\.bu	9029\[r1\],r2
  14:	81 17 57 34 	ld\.bu	13398\[r1\],r2
  18:	20 57 01 00 	ld.w	0\[r0\],r10
  1c:	20 57 79 56 	ld.w	22136\[r0\],r10
#pass
