#objdump: -Dr
#name: PowerPC test 2

.*: +file format elf32-powerpc

Disassembly of section \.text:

0+0000000 <foo>:
   0:	60 00 00 00 	nop
   4:	60 00 00 00 	nop
   8:	60 00 00 00 	nop
   c:	48 00 00 04 	b       10 <foo\+0x10>
  10:	48 00 00 08 	b       18 <foo\+0x18>
  14:	48 00 00 00 	b       14 <foo\+0x14>
			14: R_PPC_REL24	x
  18:	48 00 00 04 	b       1c <foo\+0x1c>
			18: R_PPC_REL24	\.data\+0x4
  1c:	48 00 00 00 	b       1c <foo\+0x1c>
			1c: R_PPC_REL24	z
  20:	48 00 00 14 	b       34 <foo\+0x34>
			20: R_PPC_REL24	z\+0x14
  24:	48 00 00 04 	b       28 <foo\+0x28>
  28:	48 00 00 00 	b       28 <foo\+0x28>
			28: R_PPC_REL24	a
  2c:	48 00 00 50 	b       7c <apfour>
  30:	48 00 00 04 	b       34 <foo\+0x34>
			30: R_PPC_REL24	a\+0x4
  34:	48 00 00 4c 	b       80 <apfour\+0x4>
  38:	48 00 00 00 	b       38 <foo\+0x38>
			38: R_PPC_LOCAL24PC	a
  3c:	48 00 00 40 	b       7c <apfour>
  40:	00 00 00 40 	\.long 0x40
			40: R_PPC_ADDR32	\.text\+0x40
  44:	00 00 00 4c 	\.long 0x4c
			44: R_PPC_ADDR32	\.text\+0x4c
  48:	00 00 00 00 	\.long 0x0
			48: R_PPC_REL32	x
  4c:	00 00 00 04 	\.long 0x4
			4c: R_PPC_REL32	x\+0x4
  50:	00 00 00 00 	\.long 0x0
			50: R_PPC_REL32	z
  54:	00 00 00 04 	\.long 0x4
			54: R_PPC_REL32	\.data\+0x4
  58:	00 00 00 00 	\.long 0x0
			58: R_PPC_ADDR32	x
  5c:	00 00 00 04 	\.long 0x4
			5c: R_PPC_ADDR32	\.data\+0x4
  60:	00 00 00 00 	\.long 0x0
			60: R_PPC_ADDR32	z
  64:	ff ff ff fc 	fnmsub  f31,f31,f31,f31
			64: R_PPC_ADDR32	x\+0xf+ffffffc
  68:	00 00 00 00 	\.long 0x0
			68: R_PPC_ADDR32	\.data
  6c:	ff ff ff fc 	fnmsub  f31,f31,f31,f31
			6c: R_PPC_ADDR32	z\+0xf+ffffffc
  70:	00 00 00 08 	\.long 0x8
  74:	00 00 00 08 	\.long 0x8

0+0000078 <a>:
  78:	00 00 00 00 	\.long 0x0
			78: R_PPC_ADDR32	a

0+000007c <apfour>:
  7c:	00 00 00 7c 	\.long 0x7c
			7c: R_PPC_ADDR32	\.text\+0x7c
  80:	00 00 00 7c 	\.long 0x7c
			80: R_PPC_ADDR32	\.text\+0x7c
  84:	ff ff ff fc 	fnmsub  f31,f31,f31,f31
  88:	00 00 00 7e 	\.long 0x7e
			88: R_PPC_ADDR32	\.text\+0x7e
  8c:	00 00 00 00 	\.long 0x0
  90:	60 00 00 00 	nop
  94:	40 a5 ff fc 	ble-    cr1,90 <apfour\+0x14>
  98:	41 a9 ff f8 	bgt-    cr2,90 <apfour\+0x14>
  9c:	40 8d ff f4 	ble\+    cr3,90 <apfour\+0x14>
  a0:	41 91 ff f0 	bgt\+    cr4,90 <apfour\+0x14>
  a4:	40 95 00 10 	ble-    cr5,b4 <nop>
  a8:	41 99 00 0c 	bgt-    cr6,b4 <nop>
  ac:	40 bd 00 08 	ble\+    cr7,b4 <nop>
  b0:	41 a1 00 04 	bgt\+    b4 <nop>
Disassembly of section \.data:

0+0000000 <x>:
   0:	00 00 00 00 	\.long 0x0

0+0000004 <y>:
   4:	00 00 00 00 	\.long 0x0
