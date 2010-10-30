#as: -big
#objdump: -drj.text
#name: PC-relative loads

.*:     file format .*sh.*

Disassembly of section \.text:

00000000 <code>:
   0:	8b 01       	bf	6 <foo>
   2:	d0 02       	mov\.l	c <bar>,r0	! 6 .*
   4:	90 02       	mov\.w	c <bar>,r0	! 0 .*

00000006 <foo>:
   6:	af fe       	bra	6 <foo>
   8:	00 09       	nop	
   a:	00 09       	nop	

0000000c <bar>:
   c:	00 00       	.*[ 	]*.*
   e:	00 06       	.*[ 	]*.*
  10:	00 0a       	.*[ 	]*.*
  12:	0c 00       	.*[ 	]*.*
