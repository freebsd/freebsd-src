#as: -mintel-syntax
#objdump: -d
#name: i860 dir-intel02

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	34 12 1f ec 	orh	0x1234,%r0,%r31
   4:	78 56 f8 e7 	or	0x5678,%r31,%r24
   8:	00 c0 28 91 	adds	%r24,%r9,%r8
   c:	f0 f0 05 ec 	orh	0xf0f0,%r0,%r5
  10:	5a 5a b8 e4 	or	0x5a5a,%r5,%r24
  14:	00 c0 28 91 	adds	%r24,%r9,%r8
