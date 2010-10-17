#as: -a32 -m601 -maltivec
#objdump: -dr
#name: AltiVec tests

.*: +file format elf32-powerpc

Disassembly of section \.text:

00000000 <start>:
   0:	7c 60 06 6c 	dss     3
   4:	7e 00 06 6c 	dssall
   8:	7c 25 22 ac 	dst     r5,r4,1
   c:	7e 08 3a ac 	dstt    r8,r7,0
  10:	7c 65 32 ec 	dstst   r5,r6,3
  14:	7e 44 2a ec 	dststt  r4,r5,2
