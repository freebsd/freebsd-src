#name: br-isac.d
#objdump: -d
#as: -march=isac -pcrel

.*:     file format .*

Disassembly of section .text:

0+ <foo>:
   0:	4e71           	nop
   2:	61ff ffff fffc 	bsrl 0 <foo>
   8:	60f6           	bras 0 <foo>
   a:	6000 0000      	braw c <foo\+0xc>
   e:	61f0           	bsrs 0 <foo>
  10:	61ff 0000 0000 	bsrl 12 <foo\+0x12>
  16:	4e71           	nop
