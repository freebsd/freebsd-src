#as: -xnone -mtune=itanium1
#objdump: -dr
#name: ia64 tls

.*: +file format .*

Disassembly of section \.text:

0+000 <foo>:
   0:	0d 20 21 0a 80 05 	\[MFI\]       alloc r36=ar\.pfs,8,5,0
			2: LTOFF_TPREL22	x
   6:	00 00 00 02 00 00 	            nop\.f 0x0
   c:	04 08 00 90       	            addl r32=0,r1;;
  10:	0b 00 01 40 18 10 	\[MMI\]       ld8 r32=\[r32\];;
  16:	10 02 35 00 40 00 	            add r33=r32,r13
  1c:	00 00 04 00       	            nop\.i 0x0;;
  20:	0b 10 00 1a 00 21 	\[MMI\]       mov r2=r13;;
			21: TPREL22	y
  26:	10 02 08 00 48 00 	            addl r33=0,r2
  2c:	00 00 04 00       	            nop\.i 0x0;;
  30:	01 00 01 02 00 21 	\[MII\]       mov r32=r1
			31: LTOFF_DTPMOD22	z
			32: LTOFF_DTPREL22	z
  36:	50 02 04 00 48 c0 	            addl r37=0,r1
  3c:	04 08 00 90       	            addl r38=0,r1;;
  40:	19 28 01 4a 18 10 	\[MMB\]       ld8 r37=\[r37\]
			42: PCREL21B	__tls_get_addr
  46:	60 02 98 30 20 00 	            ld8 r38=\[r38\]
  4c:	08 00 00 50       	            br\.call\.sptk\.many b0=40 <foo\+0x40>;;
  50:	0b 08 00 40 00 21 	\[MMI\]       mov r1=r32;;
			51: LTOFF_DTPMOD22	a
			52: DTPREL22	a
  56:	50 02 04 00 48 c0 	            addl r37=0,r1
  5c:	04 00 00 90       	            mov r38=0;;
  60:	1d 28 01 4a 18 10 	\[MFB\]       ld8 r37=\[r37\]
			62: PCREL21B	__tls_get_addr
  66:	00 00 00 02 00 00 	            nop\.f 0x0
  6c:	08 00 00 50       	            br\.call\.sptk\.many b0=60 <foo\+0x60>;;
  70:	0b 08 00 40 00 21 	\[MMI\]       mov r1=r32;;
			71: LTOFF_DTPMOD22	b
  76:	50 02 04 00 48 c0 	            addl r37=0,r1
  7c:	04 00 00 84       	            mov r38=r0;;
  80:	1d 28 01 4a 18 10 	\[MFB\]       ld8 r37=\[r37\]
			82: PCREL21B	__tls_get_addr
  86:	00 00 00 02 00 00 	            nop\.f 0x0
  8c:	08 00 00 50       	            br\.call\.sptk\.many b0=80 <foo\+0x80>;;
  90:	02 08 00 40 00 21 	\[MII\]       mov r1=r32
			92: DTPREL22	b
  96:	20 00 20 00 42 20 	            mov r2=r8;;
  9c:	04 10 00 90       	            addl r33=0,r2
  a0:	1d 10 01 04 00 24 	\[MFB\]       addl r34=0,r2
			a0: DTPREL22	c
  a6:	00 00 00 02 00 80 	            nop\.f 0x0
  ac:	08 00 84 00       	            br\.ret\.sptk\.many b0;;
