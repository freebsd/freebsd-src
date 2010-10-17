#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 90 00 18 	18009000     extw       r0,r1
   4:	00 10 6e 1b 	1b6e1000     extw       fp,sp
   8:	00 90 1f 18 	181f9000     extw       r0,0
   c:	ff 91 3f 18 	183f91ff     extw       r1,-1
  10:	00 10 e1 1f 	1fe11000     extw       0,r2
  14:	00 90 e1 1f 	1fe19000     extw       0,r3
  18:	ff 90 9f 18 	189f90ff     extw       r4,255
  1c:	00 90 e2 1f 	1fe29000     extw       0,r5
  20:	00 91 df 18 	18df9100     extw       r6,-256
  24:	00 90 e3 1f 	1fe39000     extw       0,r7
  28:	00 10 1f 19 	191f1000     extw       r8,0x100
  2c:	00 01 00 00 
  30:	00 10 3f 19 	193f1000     extw       r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 10 7f 19 	197f1000     extw       r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 10 ff 1f 	1fff1000     extw       0,0x100
  44:	00 01 00 00 
  48:	00 10 1f 18 	181f1000     extw       r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 90 45 19 	19459001     extw.z     r10,r11
  54:	02 90 86 19 	19869002     extw.nz    r12,r13
  58:	0b 10 df 19 	19df100b     extw.lt    r14,0
  5c:	00 00 00 00 
  60:	09 10 ff 19 	19ff1009     extw.gt    r15,0x200
  64:	00 02 00 00 
  68:	00 91 00 18 	18009100     extw.f     r0,r1
  6c:	01 90 5e 18 	185e9001     extw.f     r2,1
  70:	00 11 e2 1f 	1fe21100     extw.f     0,r4
  74:	00 11 bf 18 	18bf1100     extw.f     r5,0x200
  78:	00 02 00 00 
  7c:	00 11 df 1f 	1fdf1100     extw.f     0,0x200
  80:	00 02 00 00 
  84:	01 91 00 18 	18009101     extw.z.f   r0,r1
  88:	02 11 3f 18 	183f1102     extw.nz.f  r1,0
  8c:	00 00 00 00 
  90:	0b 11 c1 1f 	1fc1110b     extw.lt.f  0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 11 1f 18 	181f110c     extw.le.f  r0,0x200
  9c:	00 02 00 00 
  a0:	04 11 df 1f 	1fdf1104     extw.n.f   0,0x200
  a4:	00 02 00 00 
