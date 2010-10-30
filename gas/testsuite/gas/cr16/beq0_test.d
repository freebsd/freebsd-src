#as:
#objdump:  -dr
#name:  beq0_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	71 0c       	beq0b	r1,\*\+0x10 <main\+0x10>:s
   2:	b1 0c       	beq0b	r1,\*\+0x18 <main\+0x18>:s
   4:	e1 0c       	beq0b	r1,\*\+0x1e <main\+0x1e>:s
   6:	71 0e       	beq0w	r1,\*\+0x10 <main\+0x10>:s
   8:	b1 0e       	beq0w	r1,\*\+0x18 <main\+0x18>:s
   a:	e1 0e       	beq0w	r1,\*\+0x1e <main\+0x1e>:s
