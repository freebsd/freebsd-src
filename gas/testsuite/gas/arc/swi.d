#as: -EL -marc8
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <main>:
   0:	00 84 00 40 	40008400     add        r0,r1,r2
   4:	02 fe ff 1f 	1ffffe02     swi        
   8:	00 0a 62 50 	50620a00     sub        r3,r4,r5
