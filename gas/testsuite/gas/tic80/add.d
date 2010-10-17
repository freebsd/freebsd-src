#objdump: -d
#name: TIc80 signed and unsigned add instructions

.*: +file format .*tic80.*

Disassembly of section .text:

00000000 <.text>:
   0:	0a 00 fb 62.*
   4:	ff 3f ac 20.*
   8:	00 40 2c 21.*
   c:	00 10 7b 31 00 40 00 00.*
  14:	00 10 fb 41 ff bf ff ff.*
  1c:	00 10 bb 5a ff ff ff 7f.*
  24:	00 10 3b 6b 00 00 00 80.*
  2c:	0a 20 fb 62.*
  30:	ff bf ac 20.*
  34:	00 c0 2c 21.*
  38:	00 30 7b 31 00 40 00 00.*
  40:	00 30 fb 41 ff bf ff ff.*
  48:	00 30 bb 5a ff ff ff 7f.*
  50:	00 30 3b 6b 00 00 00 80.*
