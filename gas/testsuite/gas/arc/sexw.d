#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 8c 00 18 	18008c00     sexw       r0,r1
   4:	00 0c 6e 1b 	1b6e0c00     sexw       fp,sp
   8:	00 8c 1f 18 	181f8c00     sexw       r0,0
   c:	ff 8d 3f 18 	183f8dff     sexw       r1,-1
  10:	00 0c e1 1f 	1fe10c00     sexw       0,r2
  14:	00 8c e1 1f 	1fe18c00     sexw       0,r3
  18:	ff 8c 9f 18 	189f8cff     sexw       r4,255
  1c:	00 8c e2 1f 	1fe28c00     sexw       0,r5
  20:	00 8d df 18 	18df8d00     sexw       r6,-256
  24:	00 8c e3 1f 	1fe38c00     sexw       0,r7
  28:	00 0c 1f 19 	191f0c00     sexw       r8,0x100
  2c:	00 01 00 00 
  30:	00 0c 3f 19 	193f0c00     sexw       r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 0c 7f 19 	197f0c00     sexw       r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 0c ff 1f 	1fff0c00     sexw       0,0x100
  44:	00 01 00 00 
  48:	00 0c 1f 18 	181f0c00     sexw       r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 8c 45 19 	19458c01     sexw.z     r10,r11
  54:	02 8c 86 19 	19868c02     sexw.nz    r12,r13
  58:	0b 0c df 19 	19df0c0b     sexw.lt    r14,0
  5c:	00 00 00 00 
  60:	09 0c ff 19 	19ff0c09     sexw.gt    r15,0x200
  64:	00 02 00 00 
  68:	00 8d 00 18 	18008d00     sexw.f     r0,r1
  6c:	01 8c 5e 18 	185e8c01     sexw.f     r2,1
  70:	00 0d e2 1f 	1fe20d00     sexw.f     0,r4
  74:	00 0d bf 18 	18bf0d00     sexw.f     r5,0x200
  78:	00 02 00 00 
  7c:	00 0d df 1f 	1fdf0d00     sexw.f     0,0x200
  80:	00 02 00 00 
  84:	01 8d 00 18 	18008d01     sexw.z.f   r0,r1
  88:	02 0d 3f 18 	183f0d02     sexw.nz.f  r1,0
  8c:	00 00 00 00 
  90:	0b 0d c1 1f 	1fc10d0b     sexw.lt.f  0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 0d 1f 18 	181f0d0c     sexw.le.f  r0,0x200
  9c:	00 02 00 00 
  a0:	04 0d df 1f 	1fdf0d04     sexw.n.f   0,0x200
  a4:	00 02 00 00 
