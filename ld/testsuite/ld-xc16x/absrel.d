
.*:     file format elf32-xc16x

Disassembly of section .text:

00000400 <_start>:
 400:	e0 f5       	mov r5,#0xf
 402:	e0 f6       	mov r6,#0xf

00000404 <.12>:
 404:	f2 f5 1c 04 	mov r5,0x41c
 408:	e0 d6       	mov r6,#0xd
 40a:	f2 f7 1c 04 	mov r7,0x41c
 40e:	e0 d8       	mov r8,#0xd

00000410 <.13>:
 410:	f2 f5 1c 04 	mov r5,0x41c
 414:	e0 f6       	mov r6,#0xf
 416:	f2 f7 1c 04 	mov r7,0x41c
 41a:	e0 f8       	mov r8,#0xf

0000041c <.end>:
.*:	ca 09 04 04 	calla- cc_nusr0,404 <.12>
.*:	ca 19 04 04 	calla- cc_nusr1,404 <.12>
.*:	ca 29 04 04 	calla- cc_usr0,404 <.12>
.*:	ea 09 04 04 	jmpa- cc_nusr0,404 <.12>
.*:	ea 19 04 04 	jmpa- cc_nusr1,404 <.12>
.*:	ea 29 04 04 	jmpa- cc_usr0,404 <.12>
