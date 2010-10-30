#as:
#objdump:  -dr
#name:  cinv_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	0a 00       	cinv    \[i\]
   2:	0b 00       	cinv    \[i,u\]
   4:	0c 00       	cinv    \[d\]
   6:	0d 00       	cinv    \[d,u\]
   8:	0e 00       	cinv    \[d,i\]
   a:	0f 00       	cinv    \[d,i,u\]
