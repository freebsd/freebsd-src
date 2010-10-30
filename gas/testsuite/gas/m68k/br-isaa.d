#name: br-isaa.d
#objdump: -d
#as: -march=isaa -pcrel

.*:     file format .*

Disassembly of section .text:

0+ <foo>:
   0:	4e71           	nop
   2:	60fc           	bras 0 <foo>
   4:	6000 0000      	braw 6 <foo\+0x6>
   8:	61f6           	bsrs 0 <foo>
   a:	6100 0000      	bsrw c <foo\+0xc>
   e:	4e71           	nop
