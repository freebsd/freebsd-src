#objdump: -dr
#as: -big
#name: sh dynamic tls

.*: +file format .*

Disassembly of section .text:

0+000 <fn>:
   0:	2f c6 [ 	]*mov\.l	r12,@-r15
   2:	2f e6 [ 	]*mov\.l	r14,@-r15
   4:	4f 22 [ 	]*sts\.l	pr,@-r15
   6:	c7 14 [ 	]*mova	58 <fn\+0x58>,r0
   8:	dc 13 [ 	]*mov\.l	58 <fn\+0x58>,r12[ 	]+! 0x0 .*
   a:	3c 0c [ 	]*add	r0,r12
   c:	6e f3 [ 	]*mov	r15,r14
   e:	d4 04 [ 	]*mov\.l	20 <fn\+0x20>,r4[ 	]+! 0x0 .*
  10:	c7 04 [ 	]*mova	24 <fn\+0x24>,r0
  12:	d1 04 [ 	]*mov\.l	24 <fn\+0x24>,r1[ 	]+! 0x0 .*
  14:	31 0c [ 	]*add	r0,r1
  16:	41 0b [ 	]*jsr	@r1
  18:	34 cc [ 	]*add	r12,r4
  1a:	a0 05 [ 	]*bra	28 <fn\+0x28>
  1c:	00 09 [ 	]*nop	
  1e:	00 09 [ 	]*nop	
	\.\.\.
[ 	]+20: R_SH_TLS_GD_32	foo
[ 	]+24: R_SH_PLT32	__tls_get_addr
  28:	d4 03 [ 	]*mov\.l	38 <fn\+0x38>,r4[ 	]+! 0x0 .*
  2a:	c7 04 [ 	]*mova	3c <fn\+0x3c>,r0
  2c:	d1 03 [ 	]*mov\.l	3c <fn\+0x3c>,r1[ 	]+! 0x0 .*
  2e:	31 0c [ 	]*add	r0,r1
  30:	41 0b [ 	]*jsr	@r1
  32:	34 cc [ 	]*add	r12,r4
  34:	a0 04 [ 	]*bra	40 <fn\+0x40>
  36:	00 09 [ 	]*nop	
	\.\.\.
[ 	]+38: R_SH_TLS_LD_32	bar
[ 	]+3c: R_SH_PLT32	__tls_get_addr
  40:	e2 01 [ 	]*mov	#1,r2
  42:	d1 06 [ 	]*mov\.l	5c <fn\+0x5c>,r1[ 	]+! 0x0 .*
  44:	30 1c [ 	]*add	r1,r0
  46:	20 22 [ 	]*mov\.l	r2,@r0
  48:	d1 05 [ 	]*mov\.l	60 <fn\+0x60>,r1[ 	]+! 0x0 .*
  4a:	30 1c [ 	]*add	r1,r0
  4c:	6f e3 [ 	]*mov	r14,r15
  4e:	4f 26 [ 	]*lds\.l	@r15\+,pr
  50:	6e f6 [ 	]*mov\.l	@r15\+,r14
  52:	00 0b [ 	]*rts	
  54:	6c f6 [ 	]*mov\.l	@r15\+,r12
  56:	00 09 [ 	]*nop	
	\.\.\.
[ 	]+58: R_SH_GOTPC	_GLOBAL_OFFSET_TABLE_
[ 	]+5c: R_SH_TLS_LDO_32	bar
[ 	]+60: R_SH_TLS_LDO_32	baz
