#as: -EL -marc8
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <condcodeTest>:
   0:	12 02 00 40 	40000212     add.isbusy r0,r0,r1
   4:	00 02 60 45 	45600200     add        rwscreg,r0,r1
   8:	00 d8 00 40 	4000d800     add        r0,r1,roscreg
   c:	00 02 a0 45 	45a00200     add        woscreg,r0,r1