#name: br-isab.d
#objdump: -d
#as: -march=isab -pcrel

.*:     file format .*

Disassembly of section .text:

0+ <foo>:
   0:	4e71           	nop
   2:	61ff ffff fffc 	bsrl 0 <foo>
   8:	60f6           	bras 0 <foo>
   a:	60ff 0000 0000 	bral c <foo\+0xc>
  10:	61ee           	bsrs 0 <foo>
  12:	61ff 0000 0000 	bsrl 14 <foo\+0x14>
  18:	4e71           	nop
