#as:
#objdump:  -dr
#name:  excp_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	c5 00       	excp	svc
   2:	c6 00       	excp	dvz
   4:	c7 00       	excp	flg
   6:	c8 00       	excp	bpt
   8:	c9 00       	excp	trc
   a:	ca 00       	excp	und
   c:	cc 00       	excp	iad
   e:	ce 00       	excp	dbg
  10:	cf 00       	excp	ise
