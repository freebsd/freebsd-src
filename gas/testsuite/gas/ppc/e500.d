#as: -mppc -me500
#objdump: -dr -Me500
#name: e500 tests

.*: +file format elf(32)?(64)?-powerpc

Disassembly of section \.text:

0+0000000 <start>:
   0:	7c 43 25 de 	isel    r2,r3,r4,23
   4:	7c 85 33 0c 	dcblc   4,r5,r6
   8:	7c e8 49 4c 	dcbtls  7,r8,r9
   c:	7d 4b 61 0c 	dcbtstls 10,r11,r12
  10:	7d ae 7b cc 	icbtls  13,r14,r15
  14:	7e 11 91 cc 	icblc   16,r17,r18
  18:	7c 89 33 9c 	mtpmr   201,r4
  1c:	7c ab 32 9c 	mfpmr   r5,203
  20:	7c 00 04 0c 	bblels
  24:	7c 00 04 4c 	bbelr
  28:	7d 00 83 a6 	mtspefscr r8
  2c:	7d 20 82 a6 	mfspefscr r9
