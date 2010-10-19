#objdump:-dw
#name: Math operations

.*: +file format .*

Disassembly of section .text:
0+000 <foo>:
   0:	01 4a [ 	]*ADD  #01h
   2:	02 4a [ 	]*ADD  #02h
   4:	03 4a [ 	]*ADD  #03h
   6:	04 4a [ 	]*ADD  #04h
   8:	05 4a [ 	]*ADD  #05h
   a:	09 ca [ 	]*ADD  A\[0\]
   c:	19 ca [ 	]*ADD  A\[1\]
   e:	29 ca [ 	]*ADD  A\[2\]
  10:	39 ca [ 	]*ADD  A\[3\]
  12:	49 ca [ 	]*ADD  A\[4\]
  14:	31 6a [ 	]*ADDC  #31h
  16:	32 6a [ 	]*ADDC  #32h
  18:	33 6a [ 	]*ADDC  #33h
  1a:	09 ea [ 	]*ADDC  A\[0\]
  1c:	19 ea [ 	]*ADDC  A\[1\]
  1e:	29 ea [ 	]*ADDC  A\[2\]
  20:	39 ea [ 	]*ADDC  A\[3\]
  22:	01 5a [ 	]*SUB  #01h
  24:	02 5a [ 	]*SUB  #02h
  26:	03 5a [ 	]*SUB  #03h
  28:	04 5a [ 	]*SUB  #04h
  2a:	05 5a [ 	]*SUB  #05h
  2c:	09 da [ 	]*SUB  A\[0\]
  2e:	19 da [ 	]*SUB  A\[1\]
  30:	29 da [ 	]*SUB  A\[2\]
  32:	39 da [ 	]*SUB  A\[3\]
  34:	49 da [ 	]*SUB  A\[4\]
  36:	31 7a [ 	]*SUBB  #31h
  38:	32 7a [ 	]*SUBB  #32h
  3a:	33 7a [ 	]*SUBB  #33h
  3c:	09 fa [ 	]*SUBB  A\[0\]
  3e:	19 fa [ 	]*SUBB  A\[1\]
  40:	29 fa [ 	]*SUBB  A\[2\]
  42:	39 fa [ 	]*SUBB  A\[3\]
