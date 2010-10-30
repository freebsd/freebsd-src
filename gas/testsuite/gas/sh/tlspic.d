#objdump: -dr
#as: -big
#name: sh pic tls

.*: +file format .*

Disassembly of section .text:

0+000 <fn>:
   0:	2f c6 [ 	]*mov\.l	r12,@-r15
   2:	2f e6 [ 	]*mov\.l	r14,@-r15
   4:	6e f3 [ 	]*mov	r15,r14
   6:	c7 08 [ 	]*mova	28 <fn\+0x28>,r0
   8:	dc 07 [ 	]*mov\.l	28 <fn\+0x28>,r12[ 	]+! 0 .*
   a:	3c 0c [ 	]*add	r0,r12
   c:	d0 02 [ 	]*mov\.l	18 <fn\+0x18>,r0[ 	]+! 0 .*
   e:	01 12 [ 	]*stc	gbr,r1
  10:	00 ce [ 	]*mov\.l	@\(r0,r12\),r0
  12:	a0 03 [ 	]*bra	1c <fn\+0x1c>
  14:	31 0c [ 	]*add	r0,r1
  16:	00 09 [ 	]*nop	
  18:	00 00 [ 	]*\.word 0x0000
[ 	]+18: R_SH_TLS_IE_32	foo
  1a:	00 00 [ 	]*\.word 0x0000
  1c:	60 13 [ 	]*mov	r1,r0
  1e:	6f e3 [ 	]*mov	r14,r15
  20:	6e f6 [ 	]*mov\.l	@r15\+,r14
  22:	00 0b [ 	]*rts	
  24:	6c f6 [ 	]*mov\.l	@r15\+,r12
  26:	00 09 [ 	]*nop	
  28:	00 00 [ 	]*\.word 0x0+0
[ 	]+28: R_SH_GOTPC	_GLOBAL_OFFSET_TABLE_
	\.\.\.
