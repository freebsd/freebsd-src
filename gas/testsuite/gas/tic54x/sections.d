#objdump: -d --headers
#name: c54x sections

.*: +file format .*c54x.*

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         0000001b  00000000  00000000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, RELOC, CODE
  1 .data         00000007  00000000  00000000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA
  2 .bss          00000014  00000000  00000000  0000....  2..0
                  ALLOC
  3 newvars       00000017  00000000  00000000  0000....  2..1
                  ALLOC, BLOCK
  4 vectors       00000002  00000000  00000000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, CODE, BLOCK
  5 clink         00000002  00000000  00000000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA, CLINK
  6 blksect       00000002  00000000  00000000  0000....  2..0
                  CONTENTS, ALLOC, LOAD, DATA, BLOCK
Disassembly of section .text:

00000000 <.text>:
   0:	1234.*

00000001 <add>:
   1:	100f.*

00000002 <aloop>:
   2:	f010.*
   3:	0001.*
   4:	f842.*
   5:	0002.*

00000006 <mpy>:
   6:	110a.*

00000007 <mloop>:
   7:	f166.*
   8:	000a.*
   9:	f868.*
   a:	0007.*

0000000b <space>:
	...

00000012 <bes>:
	...

00000013 <spacep>:
  13:	000b.*

00000014 <besp>:
  14:	0012.*

00000015 <pk1>:
	...

00000016 <endpk1>:
  16:	0000.*
	...

00000018 <endpk2>:
	...

00000019 <pk3>:
	...

0000001a <endpk3>:
	...
Disassembly of section vectors:

00000000 <vectors>:
   0:	f495.*
   1:	f495.*
