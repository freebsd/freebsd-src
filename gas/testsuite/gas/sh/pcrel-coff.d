#as: -big
#objdump: -d -EB
#name: PC-relative loads

.*:     file format .*sh.*

Disassembly of section .text:

00000000 <code>:
   0:	d1 03       	mov\.l	10 <litpool>,r1	! fffffff0
   2:	d1 03       	mov\.l	10 <litpool>,r1	! fffffff0
   4:	c7 02       	mova	10 <litpool>,r0
   6:	61 02       	mov\.l	@r0,r1
   8:	d1 01       	mov\.l	10 <litpool>,r1	! fffffff0
   a:	01 03       	bsrf	r1
   c:	00 09       	nop	
   e:	00 09       	nop	

00000010 <litpool>:
  10:	ff ff       	\.word 0xffff
  12:	ff f0       	fadd	fr15,fr15
  14:	00 09       	nop	
  16:	00 09       	nop	
  18:	00 09       	nop	
  1a:	00 09       	nop	
  1c:	00 09       	nop	
  1e:	00 09       	nop	
