#objdump: -d --headers
#name: c54x sections

.*: +file format .*c54x.*

Sections:
Idx Name          Size      VMA + LMA + File off  Algn
  0 .text         0000001b  0+000  0+000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, RELOC, CODE
  1 .data         00000007  0+000  0+000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA
  2 .bss          00000014  0+000  0+000  0000....  2..0
                  ALLOC
  3 newvars       00000017  0+000  0+000  0000....  2..1
                  ALLOC, BLOCK
  4 vectors       00000002  0+000  0+000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, CODE, BLOCK
  5 clink         00000002  0+000  0+000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA, CLINK
  6 blksect       00000002  0+000  0+000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA, BLOCK
Disassembly of section .text:

0+000 <.text>:
   0:	1234.*

0+001 <add>:
   1:	100f.*

0+002 <aloop>:
   2:	f010.*
   3:	0001.*
   4:	f842.*
   5:	0002.*

0+006 <mpy>:
   6:	110a.*

0+007 <mloop>:
   7:	f166.*
   8:	000a.*
   9:	f868.*
   a:	0007.*

0+00b <space>:
	...

0+012 <bes>:
	...

0+013 <spacep>:
  13:	000b.*

0+014 <besp>:
  14:	0012.*

0+015 <pk1>:
	...

0+016 <endpk1>:
  16:	0000.*
	...

0+018 <endpk2>:
	...

0+019 <pk3>:
	...

0+01a <endpk3>:
	...
Disassembly of section vectors:

0+000 <vectors>:
   0:	f495.*
   1:	f495.*
