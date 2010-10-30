#objdump: -dr
#as: -big
#name: sh non-pic tls

.*: +file format .*

Disassembly of section .text:

0+000 <fn>:
   0:	2f e6 [ 	]*mov\.l	r14,@-r15
   2:	6e f3 [ 	]*mov	r15,r14
   4:	01 12 [ 	]*stc	gbr,r1
   6:	d0 02 [ 	]*mov\.l	10 <fn\+0x10>,r0[ 	]+! 0 .*
   8:	30 1c [ 	]*add	r1,r0
   a:	6f e3 [ 	]*mov	r14,r15
   c:	00 0b [ 	]*rts	
   e:	6e f6 [ 	]*mov\.l	@r15\+,r14
  10:	00 00 [ 	]*\.word 0x0+0
[ 	]+10: R_SH_TLS_LE_32	foo
	\.\.\.
