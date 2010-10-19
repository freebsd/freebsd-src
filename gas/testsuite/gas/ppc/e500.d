#as: -mppc -me500
#objdump: -dr -Me500
#name: e500 tests

.*: +file format elf(32)?(64)?-powerpc.*

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
  30:	10 a0 22 cf 	efscfd  r5,r4
  34:	10 a4 02 e4 	efdabs  r5,r4
  38:	10 a4 02 e5 	efdnabs r5,r4
  3c:	10 a4 02 e6 	efdneg  r5,r4
  40:	10 a4 1a e0 	efdadd  r5,r4,r3
  44:	10 a4 1a e1 	efdsub  r5,r4,r3
  48:	10 a4 1a e8 	efdmul  r5,r4,r3
  4c:	10 a4 1a e9 	efddiv  r5,r4,r3
  50:	12 84 1a ec 	efdcmpgt cr5,r4,r3
  54:	12 84 1a ed 	efdcmplt cr5,r4,r3
  58:	12 84 1a ee 	efdcmpeq cr5,r4,r3
  5c:	12 84 1a fc 	efdtstgt cr5,r4,r3
  60:	12 84 1a fc 	efdtstgt cr5,r4,r3
  64:	12 84 1a fd 	efdtstlt cr5,r4,r3
  68:	12 84 1a fe 	efdtsteq cr5,r4,r3
  6c:	10 a0 22 f1 	efdcfsi r5,r4
  70:	10 a0 22 e3 	efdcfsid r5,r4
  74:	10 a0 22 f0 	efdcfui r5,r4
  78:	10 a0 22 e2 	efdcfuid r5,r4
  7c:	10 a0 22 f3 	efdcfsf r5,r4
  80:	10 a0 22 f2 	efdcfuf r5,r4
  84:	10 a0 22 f5 	efdctsi r5,r4
  88:	10 a0 22 eb 	efdctsidz r5,r4
  8c:	10 a0 22 fa 	efdctsiz r5,r4
  90:	10 a0 22 f4 	efdctui r5,r4
  94:	10 a0 22 ea 	efdctuidz r5,r4
  98:	10 a0 22 f8 	efdctuiz r5,r4
  9c:	10 a0 22 f7 	efdctsf r5,r4
  a0:	10 a0 22 f6 	efdctuf r5,r4
  a4:	10 a0 22 ef 	efdcfs  r5,r4
