#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 8e 00 18 	18008e00     extb       r0,r1
   4:	00 0e 6e 1b 	1b6e0e00     extb       fp,sp
   8:	00 8e 1f 18 	181f8e00     extb       r0,0
   c:	ff 8f 3f 18 	183f8fff     extb       r1,-1
  10:	00 0e e1 1f 	1fe10e00     extb       0,r2
  14:	00 8e e1 1f 	1fe18e00     extb       0,r3
  18:	ff 8e 9f 18 	189f8eff     extb       r4,255
  1c:	00 8e e2 1f 	1fe28e00     extb       0,r5
  20:	00 8f df 18 	18df8f00     extb       r6,-256
  24:	00 8e e3 1f 	1fe38e00     extb       0,r7
  28:	00 0e 1f 19 	191f0e00     extb       r8,0x100
  2c:	00 01 00 00 
  30:	00 0e 3f 19 	193f0e00     extb       r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 0e 7f 19 	197f0e00     extb       r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 0e ff 1f 	1fff0e00     extb       0,0x100
  44:	00 01 00 00 
  48:	00 0e 1f 18 	181f0e00     extb       r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 8e 45 19 	19458e01     extb.z     r10,r11
  54:	02 8e 86 19 	19868e02     extb.nz    r12,r13
  58:	0b 0e df 19 	19df0e0b     extb.lt    r14,0
  5c:	00 00 00 00 
  60:	09 0e ff 19 	19ff0e09     extb.gt    r15,0x200
  64:	00 02 00 00 
  68:	00 8f 00 18 	18008f00     extb.f     r0,r1
  6c:	01 8e 5e 18 	185e8e01     extb.f     r2,1
  70:	00 0f e2 1f 	1fe20f00     extb.f     0,r4
  74:	00 0f bf 18 	18bf0f00     extb.f     r5,0x200
  78:	00 02 00 00 
  7c:	00 0f df 1f 	1fdf0f00     extb.f     0,0x200
  80:	00 02 00 00 
  84:	01 8f 00 18 	18008f01     extb.z.f   r0,r1
  88:	02 0f 3f 18 	183f0f02     extb.nz.f  r1,0
  8c:	00 00 00 00 
  90:	0b 0f c1 1f 	1fc10f0b     extb.lt.f  0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 0f 1f 18 	181f0f0c     extb.le.f  r0,0x200
  9c:	00 02 00 00 
  a0:	04 0f df 1f 	1fdf0f04     extb.n.f   0,0x200
  a4:	00 02 00 00 
