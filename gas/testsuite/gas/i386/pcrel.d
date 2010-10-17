#objdump: -drw
#name: i386 pcrel reloc

.*: +file format .*i386.*

Disassembly of section \.text:

0+000 <loc>:
  ( 0:	e9 30 12 00 00[	 ]*jmp    1235 .*1: R_386_PC32	\*ABS\*| 0:	e9 2f 12 00 00[	 ]*jmp    1234 .*1: DISP32	\*ABS\*)

0+005 <glob>:
  ( 5:	e9 fc ff ff ff[	 ]*jmp    6 .*6: R_386_PC32	ext| 5:	e9 f6 ff ff ff[	 ]*jmp    0 .*6: DISP32	ext)
  ( a:	e9 fc ff ff ff[	 ]*jmp    b .*b: R_386_PC32	weak| a:	e9 f1 ff ff ff[	 ]*jmp    0 .*b: DISP32	weak)
  ( f:	e9 fc ff ff ff[	 ]*jmp    10 .*10: R_386_PC32	comm| f:	e9 ec ff ff ff[	 ]*jmp    0 .*10: DISP32	comm| f:	e9 f0 ff ff ff       	jmp    4 .*10: DISP32	comm.*)
  (14:	eb ea         [	 ]*jmp    0 .*|14:	eb ea[	 ]*jmp    0 .*)
  (16:	e9 fc ff ff ff[	 ]*jmp    17 .*17: R_386_PC32	glob|16:	eb ed[	 ]*jmp    5 .*)
  (1b:	e9 72 98 00 00[	 ]*jmp    9892 .*1c: R_386_PC32	\*ABS\*|18:	e9 59 98 00 00[	 ]*jmp    9876 .*19: DISP32	\*ABS\*)
  (20:	e9 db 00 00 00[	 ]*jmp    100 .*|1d:	e9 de 00 00 00[	 ]*jmp    100 .*)
  (25:	e9 fc ff ff ff[	 ]*jmp    26 .*26: R_386_PC32	glob2|22:	e9 de 00 00 00[	 ]*jmp    105 .*)
  (2a:	e9 fc ff ff ff[	 ]*jmp    2b .*2b: R_386_PC32	\.data|27:	e9 d4 00 00 00[	 ]*jmp    100 .*28: DISP32	\.data.*|27:	e9 d4 ff ff ff       	jmp    0 .*28: DISP32	\.data)
  (2f:	e9 00 00 00 00[	 ]*jmp    34 .*30: R_386_PC32	\.data|2c:	e9 d3 00 00 00[	 ]*jmp    104 .*2d: DISP32	\.data.*|2c:	e9 d3 ff ff ff       	jmp    4 .*2d: DISP32	\.data)
  (34:	e9 fc ff ff ff[	 ]*jmp    35 .*35: R_386_PC32	\*ABS\*|31:	e9 ca ff ff ff[	 ]*jmp    0 .*32: DISP32	\*ABS\*)
  (39:	e9 c8 ed ff ff[	 ]*jmp    ffffee06 .*3a: R_386_PC32	ext|36:	e9 91 ed ff ff[	 ]*jmp    ffffedcc .*37: DISP32	ext)
  (3e:	e9 c8 ed ff ff[	 ]*jmp    ffffee0b .*3f: R_386_PC32	weak|3b:	e9 8c ed ff ff[	 ]*jmp    ffffedcc .*3c: DISP32	weak)
  (43:	e9 c8 ed ff ff[	 ]*jmp    ffffee10 .*44: R_386_PC32	comm|40:	e9 87 ed ff ff[	 ]*jmp    ffffedcc .*41: DISP32	comm|40:	e9 8b ed ff ff       	jmp    ffffedd0 .*41: DISP32	comm.*)
  (48:	e9 7f ed ff ff[	 ]*jmp    ffffedcc .*|45:	e9 82 ed ff ff[	 ]*jmp    ffffedcc .*)
  (4d:	e9 c8 ed ff ff[	 ]*jmp    ffffee1a .*4e: R_386_PC32	glob|4a:	e9 82 ed ff ff[	 ]*jmp    ffffedd1 .*)
  (52:	e9 3e 86 00 00[	 ]*jmp    8695 .*53: R_386_PC32	\*ABS\*|4f:	e9 ee 85 00 00[	 ]*jmp    8642 .*50: DISP32	\*ABS\*)
  (57:	e9 70 ee ff ff[	 ]*jmp    ffffeecc .*|54:	e9 73 ee ff ff[	 ]*jmp    ffffeecc .*)
  (5c:	e9 c8 ed ff ff[	 ]*jmp    ffffee29 .*5d: R_386_PC32	glob2|59:	e9 73 ee ff ff[	 ]*jmp    ffffeed1 .*)
  (61:	e9 c8 ed ff ff[	 ]*jmp    ffffee2e .*62: R_386_PC32	\.data|5e:	e9 69 ee ff ff[	 ]*jmp    ffffeecc .*5f: DISP32	\.data.*|5e:	e9 69 ed ff ff       	jmp    ffffedcc .*5f: DISP32	\.data)
  (66:	e9 cc ed ff ff[	 ]*jmp    ffffee37 .*67: R_386_PC32	\.data|63:	e9 68 ee ff ff[	 ]*jmp    ffffeed0 .*64: DISP32	\.data.*|63:	e9 68 ed ff ff       	jmp    ffffedd0 .*64: DISP32	\.data)
  (6b:	e9 ba 79 ff ff[	 ]*jmp    ffff7a2a .*6c: R_386_PC32	\*ABS\*|68:	e9 51 79 ff ff[	 ]*jmp    ffff79be .*69: DISP32	\*ABS\*)
  (70:	e9 86 67 ff ff[	 ]*jmp    ffff67fb .*71: R_386_PC32	ext|6d:	e9 18 67 ff ff[	 ]*jmp    ffff678a .*6e: DISP32	ext)
  (75:	e9 86 67 ff ff[	 ]*jmp    ffff6800 .*76: R_386_PC32	weak|72:	e9 13 67 ff ff[	 ]*jmp    ffff678a .*73: DISP32	weak)
  (7a:	e9 86 67 ff ff[	 ]*jmp    ffff6805 .*7b: R_386_PC32	comm|77:	e9 0e 67 ff ff[	 ]*jmp    ffff678a .*78: DISP32	comm|77:	e9 12 67 ff ff       	jmp    ffff678e .*78: DISP32	comm.*)
  (7f:	e9 06 67 ff ff[	 ]*jmp    ffff678a .*|7c:	e9 09 67 ff ff[	 ]*jmp    ffff678a .*)
  (84:	e9 06 67 ff ff[	 ]*jmp    ffff678f .*|81:	e9 09 67 ff ff[	 ]*jmp    ffff678f .*)
  (89:	e9 fc ff ff ff[	 ]*jmp    8a .*8a: R_386_PC32	\*ABS\*|86:	e9 75 ff ff ff[	 ]*jmp    0 .*87: DISP32	\*ABS\*)
  (8e:	e9 f7 67 ff ff[	 ]*jmp    ffff688a .*|8b:	e9 fa 67 ff ff[	 ]*jmp    ffff688a .*)
  (93:	e9 f7 67 ff ff[	 ]*jmp    ffff688f .*|90:	e9 fa 67 ff ff[	 ]*jmp    ffff688f .*)
  (98:	e9 86 67 ff ff[	 ]*jmp    ffff6823 .*99: R_386_PC32	\.data|95:	e9 f0 67 ff ff[	 ]*jmp    ffff688a .*96: DISP32	\.data.*|95:	e9 f0 66 ff ff       	jmp    ffff678a .*96: DISP32	\.data)
  (9d:	e9 8a 67 ff ff[	 ]*jmp    ffff682c .*9e: R_386_PC32	\.data|9a:	e9 ef 67 ff ff[	 ]*jmp    ffff688e .*9b: DISP32	\.data.*|9a:	e9 ef 66 ff ff       	jmp    ffff678e .*9b: DISP32	\.data)
  (a2:	e9 fc 00 00 00[	 ]*jmp    1a3 .*a3: R_386_PC32	\*ABS\*|9f:	e9 5c 00 00 00[	 ]*jmp    100 .*a0: DISP32	\*ABS\*)
  (a7:	e9 01 00 00 00[	 ]*jmp    ad .*a8: R_386_PC32	\*ABS\*|a4:	e9 5c ff ff ff[	 ]*jmp    5 .*a5: DISP32	\*ABS\*)
  (ac:	e9 01 ff ff ff[	 ]*jmp    ffffffb2 .*ad: R_386_PC32	\*ABS\*|a9:	e9 57 fe ff ff[	 ]*jmp    ffffff05 .*aa: DISP32	\*ABS\*)
  (b1:	e9 01 01 00 00[	 ]*jmp    1b7 .*b2: R_386_PC32	\*ABS\*|ae:	e9 52 00 00 00[	 ]*jmp    105 .*af: DISP32	\*ABS\*)
  (b6:	e9 01 00 00 00[	 ]*jmp    bc .*b7: R_386_PC32	\*ABS\*|b3:	e9 4d ff ff ff[	 ]*jmp    5 .*b4: DISP32	\*ABS\*)
	\.\.\.
