#as: -KPIC
#objdump: -dr
#name: VxWorks PIC

.*:     file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	2f 00 00 00 	sethi  %hi\(0\), %l7
			0: R_SPARC_HI22	__GOTT_BASE__
   4:	ee 05 e0 00 	ld  \[ %l7 \], %l7
			4: R_SPARC_LO10	__GOTT_BASE__
   8:	ee 05 e0 00 	ld  \[ %l7 \], %l7
			8: R_SPARC_LO10	__GOTT_INDEX__
   c:	03 00 00 00 	sethi  %hi\(0\), %g1
			c: R_SPARC_HI22	__GOTT_BASE__
  10:	82 10 60 00 	mov  %g1, %g1	! 0x0
			10: R_SPARC_LO10	__GOTT_BASE__
  14:	03 00 00 00 	sethi  %hi\(0\), %g1
			14: R_SPARC_HI22	__GOTT_INDEX__
  18:	82 10 60 00 	mov  %g1, %g1	! 0x0
			18: R_SPARC_LO10	__GOTT_INDEX__
  1c:	03 00 00 00 	sethi  %hi\(0\), %g1
			1c: R_SPARC_GOT22	__GOT_BASE__
  20:	82 10 60 00 	mov  %g1, %g1	! 0x0
			20: R_SPARC_GOT10	__GOT_BASE__
