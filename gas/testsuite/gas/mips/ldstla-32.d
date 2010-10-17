#objdump: -d
#as: -32
#name: MIPS ld-st-la constants (ABI o32)
#source: ldstla-32.s

.*: +file format elf32-.*mips

Disassembly of section \.text:

00000000 <\.text>:
   0:	3c0189ac 	lui	at,0x89ac
   4:	00610821 	addu	at,v1,at
   8:	8c22cdef 	lw	v0,-12817\(at\)
   c:	8c23cdf3 	lw	v1,-12813\(at\)
  10:	3c012345 	lui	at,0x2345
  14:	00610821 	addu	at,v1,at
  18:	8c226789 	lw	v0,26505\(at\)
  1c:	8c23678d 	lw	v1,26509\(at\)
  20:	3c018000 	lui	at,0x8000
  24:	00610821 	addu	at,v1,at
  28:	8c220000 	lw	v0,0\(at\)
  2c:	8c230004 	lw	v1,4\(at\)
  30:	3c010000 	lui	at,0x0
  34:	00610821 	addu	at,v1,at
  38:	8c220000 	lw	v0,0\(at\)
  3c:	8c230004 	lw	v1,4\(at\)
  40:	3c018000 	lui	at,0x8000
  44:	00610821 	addu	at,v1,at
  48:	8c22ffff 	lw	v0,-1\(at\)
  4c:	8c230003 	lw	v1,3\(at\)
  50:	3c01abce 	lui	at,0xabce
  54:	00610821 	addu	at,v1,at
  58:	8c22ef01 	lw	v0,-4351\(at\)
  5c:	8c23ef05 	lw	v1,-4347\(at\)
  60:	3c010123 	lui	at,0x123
  64:	00610821 	addu	at,v1,at
  68:	8c224567 	lw	v0,17767\(at\)
  6c:	8c23456b 	lw	v1,17771\(at\)
  70:	3c0189ac 	lui	at,0x89ac
  74:	00610821 	addu	at,v1,at
  78:	ac22cdef 	sw	v0,-12817\(at\)
  7c:	ac23cdf3 	sw	v1,-12813\(at\)
  80:	3c012345 	lui	at,0x2345
  84:	00610821 	addu	at,v1,at
  88:	ac226789 	sw	v0,26505\(at\)
  8c:	ac23678d 	sw	v1,26509\(at\)
  90:	3c018000 	lui	at,0x8000
  94:	00610821 	addu	at,v1,at
  98:	ac220000 	sw	v0,0\(at\)
  9c:	ac230004 	sw	v1,4\(at\)
  a0:	3c010000 	lui	at,0x0
  a4:	00610821 	addu	at,v1,at
  a8:	ac220000 	sw	v0,0\(at\)
  ac:	ac230004 	sw	v1,4\(at\)
  b0:	3c018000 	lui	at,0x8000
  b4:	00610821 	addu	at,v1,at
  b8:	ac22ffff 	sw	v0,-1\(at\)
  bc:	ac230003 	sw	v1,3\(at\)
  c0:	3c01abce 	lui	at,0xabce
  c4:	00610821 	addu	at,v1,at
  c8:	ac22ef01 	sw	v0,-4351\(at\)
  cc:	ac23ef05 	sw	v1,-4347\(at\)
  d0:	3c010123 	lui	at,0x123
  d4:	00610821 	addu	at,v1,at
  d8:	ac224567 	sw	v0,17767\(at\)
  dc:	ac23456b 	sw	v1,17771\(at\)
  e0:	3c028000 	lui	v0,0x8000
  e4:	00431021 	addu	v0,v0,v1
  e8:	8c420000 	lw	v0,0\(v0\)
  ec:	3c020123 	lui	v0,0x123
  f0:	00431021 	addu	v0,v0,v1
  f4:	8c424567 	lw	v0,17767\(v0\)
  f8:	3c010123 	lui	at,0x123
  fc:	00230821 	addu	at,at,v1
 100:	ac224567 	sw	v0,17767\(at\)
 104:	3c027fff 	lui	v0,0x7fff
 108:	3442ffff 	ori	v0,v0,0xffff
 10c:	3c020123 	lui	v0,0x123
 110:	34424567 	ori	v0,v0,0x4567
	\.\.\.
