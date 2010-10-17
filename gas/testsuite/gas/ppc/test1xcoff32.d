#objdump: -Drx
#as:
#name: PowerPC Test 1, 32 bit XCOFF

.*: +file format aixcoff-rs6000
.*
architecture: rs6000:6000, flags 0x00000031:
HAS_RELOC, HAS_SYMS, HAS_LOCALS
start address 0x0+0000

Sections:
Idx Name +Size +VMA +LMA +File off +Algn
  0 \.text +00000068  0+0000  0+0000  000000a8  2\*\*2
 +CONTENTS, ALLOC, LOAD, RELOC, CODE
  1 \.data +00000028  0+0068  0+0068  00000110  2\*\*3
 +CONTENTS, ALLOC, LOAD, RELOC, DATA
  2 \.bss  +00000000  0+0090  0+0090  00000000  2\*\*3
 +ALLOC
SYMBOL TABLE:
\[  0\]\(sec -2\)\(fl 0x00\)\(ty   0\)\(scl 103\) \(nx 1\) 0x00000000 fake
File 
\[  2\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000000 \.crazy_table
AUX val     8 prmhsh 0 snhsh 0 typ 1 algn 2 clss 1 stb 0 snstb 0
\[  4\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000008 
AUX val    96 prmhsh 0 snhsh 0 typ 1 algn 2 clss 0 stb 0 snstb 0
\[  6\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000008 reference_csect_relative_symbols
AUX indx    4 prmhsh 0 snhsh 0 typ 2 algn 0 clss 0 stb 0 snstb 0
\[  8\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000018 dubious_references_to_default_RW_csect
AUX indx    4 prmhsh 0 snhsh 0 typ 2 algn 0 clss 0 stb 0 snstb 0
\[ 10\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000028 reference_via_toc
AUX indx    4 prmhsh 0 snhsh 0 typ 2 algn 0 clss 0 stb 0 snstb 0
\[ 12\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000040 subtract_symbols
AUX indx    4 prmhsh 0 snhsh 0 typ 2 algn 0 clss 0 stb 0 snstb 0
\[ 14\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x0000005c load_addresses
AUX indx    4 prmhsh 0 snhsh 0 typ 2 algn 0 clss 0 stb 0 snstb 0
\[ 16\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000068 
AUX val    12 prmhsh 0 snhsh 0 typ 1 algn 2 clss 5 stb 0 snstb 0
\[ 18\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000074 TOC
AUX val     0 prmhsh 0 snhsh 0 typ 1 algn 2 clss 15 stb 0 snstb 0
\[ 20\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000074 ignored0
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 22\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000078 ignored1
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 24\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x0000007c ignored2
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 26\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000080 ignored3
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 28\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000084 ignored4
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 30\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x00000088 ignored5
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 32\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl 107\) \(nx 1\) 0x0000008c ignored6
AUX val     4 prmhsh 0 snhsh 0 typ 1 algn 2 clss 3 stb 0 snstb 0
\[ 34\]\(sec  0\)\(fl 0x00\)\(ty   0\)\(scl   2\) \(nx 1\) 0x00000000 esym0
AUX val     0 prmhsh 0 snhsh 0 typ 0 algn 0 clss 0 stb 0 snstb 0
\[ 36\]\(sec  0\)\(fl 0x00\)\(ty   0\)\(scl   2\) \(nx 1\) 0x00000000 esym1
AUX val     0 prmhsh 0 snhsh 0 typ 0 algn 0 clss 0 stb 0 snstb 0


Disassembly of section \.text:

0+0000 <\.crazy_table>:
   0:	00 be ef ed 	\.long 0xbeefed
   4:	00 be ef ed 	\.long 0xbeefed

0+0008 <reference_csect_relative_symbols>:
   8:	80 63 00 00 	l       r3,0\(r3\)
   c:	80 63 00 04 	l       r3,4\(r3\)
  10:	80 63 00 04 	l       r3,4\(r3\)
  14:	80 63 00 00 	l       r3,0\(r3\)

0+0018 <dubious_references_to_default_RW_csect>:
  18:	80 63 00 00 	l       r3,0\(r3\)
  1c:	80 63 00 04 	l       r3,4\(r3\)
  20:	80 63 00 04 	l       r3,4\(r3\)
  24:	80 63 00 08 	l       r3,8\(r3\)

0+0028 <reference_via_toc>:
  28:	80 62 00 0c 	l       r3,12\(r2\)
			2a: R_TOC	ignored0\+0xf+ff8c
  2c:	80 62 00 10 	l       r3,16\(r2\)
			2e: R_TOC	ignored1\+0xf+ff88
  30:	80 62 00 14 	l       r3,20\(r2\)
			32: R_TOC	ignored2\+0xf+ff84
  34:	80 62 00 18 	l       r3,24\(r2\)
			36: R_TOC	ignored3\+0xf+ff80
  38:	80 62 00 1c 	l       r3,28\(r2\)
			3a: R_TOC	ignored4\+0xf+ff7c
  3c:	80 62 00 20 	l       r3,32\(r2\)
			3e: R_TOC	ignored5\+0xf+ff78

0+0040 <subtract_symbols>:
  40:	38 60 00 04 	lil     r3,4
  44:	38 60 ff fc 	lil     r3,-4
  48:	38 60 00 04 	lil     r3,4
  4c:	38 60 ff fc 	lil     r3,-4
  50:	38 60 ff fc 	lil     r3,-4
  54:	38 60 00 04 	lil     r3,4
  58:	80 64 00 04 	l       r3,4\(r4\)

0+005c <load_addresses>:
  5c:	38 60 00 00 	lil     r3,0
  60:	38 60 00 04 	lil     r3,4
  64:	38 62 00 24 	cal     r3,36\(r2\)
			66: R_TOC	ignored6\+0xf+ff74
Disassembly of section \.data:

0+0068 <TOC-0xc>:
  68:	de ad be ef 	stfdu   f21,-16657\(r13\)
  6c:	ca fe ba be 	lfd     f23,-17730\(r30\)
  70:	00 00 ba ad 	\.long 0xbaad

0+0074 <TOC>:
  74:	00 00 00 68 	\.long 0x68
			74: R_POS	\.data\+0xf+ff98

0+0078 <ignored1>:
  78:	00 00 00 6c 	\.long 0x6c
			78: R_POS	\.data\+0xf+ff98

0+007c <ignored2>:
  7c:	00 00 00 6c 	\.long 0x6c
			7c: R_POS	\.data\+0xf+ff98

0+0080 <ignored3>:
  80:	00 00 00 70 	\.long 0x70
			80: R_POS	\.data\+0xf+ff98

0+0084 <ignored4>:
  84:	00 00 00 00 	\.long 0x0
			84: R_POS	esym0

0+0088 <ignored5>:
  88:	00 00 00 00 	\.long 0x0
			88: R_POS	esym1

0+008c <ignored6>:
  8c:	00 00 00 00 	\.long 0x0
			8c: R_POS	\.crazy_table
