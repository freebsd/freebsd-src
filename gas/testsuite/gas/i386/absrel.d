#objdump: -drw
#name: i386 abs reloc

.*: +file format .*i386.*

Disassembly of section \.text:

0+000 <loc>:
   0:	a1 34 12 00 00[	 ]*mov    0x1234,%eax

0+005 <glob>:
   5:	a1 00 00 00 00[	 ]*mov    0x0,%eax	6: (R_386_|dir)?32	ext
   a:	a1 00 00 00 00[	 ]*mov    0x0,%eax	b: (R_386_|dir)?32	weak
   f:	(a1 00 00 00 00[	 ]*mov    0x0,%eax	10: (R_386_|dir)?32	comm.*|a1 04 00 00 00[	 ]*mov    0x4,%eax	10: dir32	comm.*)
  14:	a1 00 00 00 00[	 ]*mov    0x0,%eax	15: (R_386_|dir)?32	\.text
  19:	(a1 00 00 00 00[	 ]*mov    0x0,%eax	1a: R_386_32	glob|a1 05 00 00 00[	 ]*mov    0x5,%eax	1a: (dir)?32	\.text)
  1e:	a1 76 98 00 00[	 ]*mov    0x9876,%eax
  23:	a1 00 01 00 00[	 ]*mov    0x100,%eax	24: (R_386_|dir)?32	\.text
  28:	(a1 00 00 00 00[	 ]*mov    0x0,%eax	29: R_386_32	glob2|a1 05 01 00 00[	 ]*mov    0x105,%eax	29: (dir)?32	\.text)
  2d:	(a1 00 00 00 00[	 ]*mov    0x0,%eax	2e: (R_386_|dir)32	\.data|a1 00 01 00 00[	 ]*mov    0x100,%eax	2e: 32	\.data.*)
  32:	(a1 04 00 00 00[	 ]*mov    0x4,%eax	33: (R_386_|dir)32	\.data|a1 04 01 00 00[	 ]*mov    0x104,%eax	33: 32	\.data.*)
  37:	a1 00 00 00 00[	 ]*mov    0x0,%eax
  3c:	a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	3d: (R_386_|dir)?32	ext
  41:	a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	42: (R_386_|dir)?32	weak
  46:	(a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	47: (R_386_|dir)?32	comm.*|a1 d0 ed ff ff[	 ]*mov    0xffffedd0,%eax	47: dir32	comm.*)
  4b:	a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	4c: (R_386_|dir)?32	\.text
  50:	(a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	51: R_386_32	glob|a1 d1 ed ff ff[	 ]*mov    0xffffedd1,%eax	51: (dir)?32	\.text)
  55:	a1 42 86 00 00[	 ]*mov    0x8642,%eax
  5a:	a1 cc ee ff ff[	 ]*mov    0xffffeecc,%eax	5b: (R_386_|dir)?32	\.text
  5f:	(a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	60: R_386_32	glob2|a1 d1 ee ff ff[	 ]*mov    0xffffeed1,%eax	60: (dir)?32	\.text)
  64:	(a1 cc ed ff ff[	 ]*mov    0xffffedcc,%eax	65: (R_386_|dir)32	\.data|a1 cc ee ff ff[	 ]*mov    0xffffeecc,%eax	65: 32	\.data.*)
  69:	(a1 d0 ed ff ff[	 ]*mov    0xffffedd0,%eax	6a: (R_386_|dir)32	\.data|a1 d0 ee ff ff[	 ]*mov    0xffffeed0,%eax	6a: 32	\.data.*)
  6e:	a1 be 79 ff ff[	 ]*mov    0xffff79be,%eax
  73:	a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	74: (R_386_|dir)?32	ext
  78:	a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	79: (R_386_|dir)?32	weak
  7d:	(a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	7e: (R_386_|dir)?32	comm.*|a1 8e 67 ff ff[	 ]*mov    0xffff678e,%eax	7e: dir32	comm.*)
  82:	a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	83: (R_386_|dir)?32	\.text
  87:	(a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	88: R_386_32	glob|a1 8f 67 ff ff[	 ]*mov    0xffff678f,%eax	88: (dir)?32	\.text)
  8c:	a1 00 00 00 00[	 ]*mov    0x0,%eax
  91:	a1 8a 68 ff ff[	 ]*mov    0xffff688a,%eax	92: (R_386_|dir)?32	\.text
  96:	(a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	97: R_386_32	glob2|a1 8f 68 ff ff[	 ]*mov    0xffff688f,%eax	97: (dir)?32	\.text)
  9b:	(a1 8a 67 ff ff[	 ]*mov    0xffff678a,%eax	9c: (R_386_|dir)32	\.data|a1 8a 68 ff ff[	 ]*mov    0xffff688a,%eax	9c: 32	\.data.*)
  a0:	(a1 8e 67 ff ff[	 ]*mov    0xffff678e,%eax	a1: (R_386_|dir)32	\.data|a1 8e 68 ff ff[	 ]*mov    0xffff688e,%eax	a1: 32	\.data.*)
  a5:	a1 00 01 00 00[	 ]*mov    0x100,%eax
  aa:	(a1 ab 00 00 00[	 ]*mov    0xab,%eax	ab: R_386_PC32	glob|a1 05 00 00 00[	 ]*mov    0x5,%eax)
  af:	(a1 b0 ff ff ff[	 ]*mov    0xffffffb0,%eax	b0: R_386_PC32	glob|a1 05 ff ff ff[	 ]*mov    0xffffff05,%eax)
  b4:	(a1 b5 00 00 00[	 ]*mov    0xb5,%eax	b5: R_386_PC32	glob2|a1 05 01 00 00[	 ]*mov    0x105,%eax)
  b9:	(a1 ba ff ff ff[	 ]*mov    0xffffffba,%eax	ba: R_386_PC32	glob2|a1 05 00 00 00[	 ]*mov    0x5,%eax)
	\.\.\.
