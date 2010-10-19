#objdump: -Drx
#name: PowerPC Test 1, 64 bit elf

.*: +file format elf64-powerpc
.*
architecture: powerpc:common64, flags 0x00000011:
HAS_RELOC, HAS_SYMS
start address 0x0000000000000000

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         00000090  0000000000000000  0000000000000000  .*
                  CONTENTS, ALLOC, LOAD, RELOC, READONLY, CODE
  1 \.data         00000030  0000000000000000  0000000000000000  .*
                  CONTENTS, ALLOC, LOAD, RELOC, DATA
  2 \.bss          00000000  0000000000000000  0000000000000000  .*
                  ALLOC
  3 \.toc          00000030  0000000000000000  0000000000000000  .*
                  CONTENTS, ALLOC, LOAD, RELOC, DATA
SYMBOL TABLE:
0000000000000000 l    d  \.text	0000000000000000 (|\.text)
0000000000000000 l    d  \.data	0000000000000000 (|\.data)
0000000000000000 l    d  \.bss	0000000000000000 (|\.bss)
0000000000000000 l       \.data	0000000000000000 dsym0
0000000000000008 l       \.data	0000000000000000 dsym1
0000000000000000 l    d  \.toc	0000000000000000 (|\.toc)
0000000000000008 l       \.data	0000000000000000 usym0
0000000000000010 l       \.data	0000000000000000 usym1
0000000000000010 l       \.data	0000000000000000 datpt
0000000000000014 l       \.data	0000000000000000 dat0
0000000000000018 l       \.data	0000000000000000 dat1
000000000000001c l       \.data	0000000000000000 dat2
0000000000000020 l       \.data	0000000000000000 dat3
0000000000000028 l       \.data	0000000000000000 dat4
0000000000000000         \*UND\*	0000000000000000 esym0
0000000000000000         \*UND\*	0000000000000000 esym1
0000000000000000         \*UND\*	0000000000000000 jk


Disassembly of section \.text:

0000000000000000 <\.text>:
   0:	e8 63 00 00 	ld      r3,0\(r3\)
			2: R_PPC64_ADDR16_LO_DS	\.data
   4:	e8 63 00 08 	ld      r3,8\(r3\)
			6: R_PPC64_ADDR16_LO_DS	\.data\+0x8
   8:	e8 63 00 08 	ld      r3,8\(r3\)
			a: R_PPC64_ADDR16_LO_DS	\.data\+0x8
   c:	e8 63 00 10 	ld      r3,16\(r3\)
			e: R_PPC64_ADDR16_LO_DS	\.data\+0x10
  10:	e8 63 00 00 	ld      r3,0\(r3\)
			12: R_PPC64_ADDR16_LO_DS	esym0
  14:	e8 63 00 00 	ld      r3,0\(r3\)
			16: R_PPC64_ADDR16_LO_DS	esym1
  18:	e8 62 00 00 	ld      r3,0\(r2\)
			1a: R_PPC64_TOC16_DS	\.toc
  1c:	e8 62 00 08 	ld      r3,8\(r2\)
			1e: R_PPC64_TOC16_DS	\.toc\+0x8
  20:	e8 62 00 10 	ld      r3,16\(r2\)
			22: R_PPC64_TOC16_DS	\.toc\+0x10
  24:	e8 62 00 18 	ld      r3,24\(r2\)
			26: R_PPC64_TOC16_DS	\.toc\+0x18
  28:	e8 62 00 20 	ld      r3,32\(r2\)
			2a: R_PPC64_TOC16_DS	\.toc\+0x20
  2c:	e8 62 00 28 	ld      r3,40\(r2\)
			2e: R_PPC64_TOC16_DS	\.toc\+0x28
  30:	3c 80 00 28 	lis     r4,40
			32: R_PPC64_TOC16_HA	\.toc\+0x28
  34:	e8 62 00 28 	ld      r3,40\(r2\)
			36: R_PPC64_TOC16_LO_DS	\.toc\+0x28
  38:	38 60 00 08 	li      r3,8
  3c:	38 60 ff f8 	li      r3,-8
  40:	38 60 00 08 	li      r3,8
  44:	38 60 ff f8 	li      r3,-8
  48:	38 60 ff f8 	li      r3,-8
  4c:	38 60 00 08 	li      r3,8
  50:	38 60 00 00 	li      r3,0
			52: R_PPC64_ADDR16_LO	\.data
  54:	38 60 00 00 	li      r3,0
			56: R_PPC64_ADDR16_HI	\.data
  58:	38 60 00 00 	li      r3,0
			5a: R_PPC64_ADDR16_HA	\.data
  5c:	38 60 00 00 	li      r3,0
			5e: R_PPC64_ADDR16_HIGHER	\.data
  60:	38 60 00 00 	li      r3,0
			62: R_PPC64_ADDR16_HIGHERA	\.data
  64:	38 60 00 00 	li      r3,0
			66: R_PPC64_ADDR16_HIGHEST	\.data
  68:	38 60 00 00 	li      r3,0
			6a: R_PPC64_ADDR16_HIGHESTA	\.data
  6c:	38 60 ff f8 	li      r3,-8
  70:	38 60 ff ff 	li      r3,-1
  74:	38 60 00 00 	li      r3,0
  78:	38 60 ff ff 	li      r3,-1
  7c:	38 60 00 00 	li      r3,0
  80:	38 60 ff ff 	li      r3,-1
  84:	38 60 00 00 	li      r3,0
  88:	e8 64 00 08 	ld      r3,8\(r4\)
  8c:	e8 60 00 00 	ld      r3,0\(0\)
			8e: R_PPC64_ADDR16_LO_DS	\.text
Disassembly of section \.data:

0000000000000000 <dsym0>:
   0:	00 00 00 00 	\.long 0x0
   4:	de ad be ef 	stfdu   f21,-16657\(r13\)

0000000000000008 <dsym1>:
   8:	00 00 00 00 	\.long 0x0
   c:	ca fe ba be 	lfd     f23,-17730\(r30\)

0000000000000010 <datpt>:
  10:	00 98 96 80 	\.long 0x989680
			10: R_PPC64_REL32	jk\+0x989680

0000000000000014 <dat0>:
  14:	ff ff ff fc 	fnmsub  f31,f31,f31,f31
			14: R_PPC64_REL32	jk\+0xfffffffffffffffc

0000000000000018 <dat1>:
  18:	00 00 00 00 	\.long 0x0
			18: R_PPC64_REL32	jk

000000000000001c <dat2>:
  1c:	00 00 00 04 	\.long 0x4
			1c: R_PPC64_REL32	jk\+0x4

0000000000000020 <dat3>:
  20:	00 00 00 00 	\.long 0x0
			20: R_PPC64_REL64	jk\+0x8
  24:	00 00 00 08 	\.long 0x8

0000000000000028 <dat4>:
  28:	00 00 00 00 	\.long 0x0
			28: R_PPC64_REL64	jk\+0x10
  2c:	00 00 00 10 	\.long 0x10
Disassembly of section \.toc:

0000000000000000 <\.toc>:
	\.\.\.
			0: R_PPC64_ADDR64	\.data
			8: R_PPC64_ADDR64	\.data\+0x8
   c:	00 00 00 08 	\.long 0x8
  10:	00 00 00 00 	\.long 0x0
			10: R_PPC64_ADDR64	\.data\+0x8
  14:	00 00 00 08 	\.long 0x8
  18:	00 00 00 00 	\.long 0x0
			18: R_PPC64_ADDR64	\.data\+0x10
  1c:	00 00 00 10 	\.long 0x10
	\.\.\.
			20: R_PPC64_ADDR64	esym0
			28: R_PPC64_ADDR64	esym1
