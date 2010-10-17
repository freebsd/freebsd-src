#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 8a 00 18 	18008a00     sexb       r0,r1
   4:	00 0a 6e 1b 	1b6e0a00     sexb       fp,sp
   8:	00 8a 1f 18 	181f8a00     sexb       r0,0
   c:	ff 8b 3f 18 	183f8bff     sexb       r1,-1
  10:	00 0a e1 1f 	1fe10a00     sexb       0,r2
  14:	00 8a e1 1f 	1fe18a00     sexb       0,r3
  18:	ff 8a 9f 18 	189f8aff     sexb       r4,255
  1c:	00 8a e2 1f 	1fe28a00     sexb       0,r5
  20:	00 8b df 18 	18df8b00     sexb       r6,-256
  24:	00 8a e3 1f 	1fe38a00     sexb       0,r7
  28:	00 0a 1f 19 	191f0a00     sexb       r8,0x100
  2c:	00 01 00 00 
  30:	00 0a 3f 19 	193f0a00     sexb       r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 0a 7f 19 	197f0a00     sexb       r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 0a ff 1f 	1fff0a00     sexb       0,0x100
  44:	00 01 00 00 
  48:	00 0a 1f 18 	181f0a00     sexb       r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 8a 45 19 	19458a01     sexb.z     r10,r11
  54:	02 8a 86 19 	19868a02     sexb.nz    r12,r13
  58:	0b 0a df 19 	19df0a0b     sexb.lt    r14,0
  5c:	00 00 00 00 
  60:	09 0a ff 19 	19ff0a09     sexb.gt    r15,0x200
  64:	00 02 00 00 
  68:	00 8b 00 18 	18008b00     sexb.f     r0,r1
  6c:	01 8a 5e 18 	185e8a01     sexb.f     r2,1
  70:	00 0b e2 1f 	1fe20b00     sexb.f     0,r4
  74:	00 0b bf 18 	18bf0b00     sexb.f     r5,0x200
  78:	00 02 00 00 
  7c:	00 0b df 1f 	1fdf0b00     sexb.f     0,0x200
  80:	00 02 00 00 
  84:	01 8b 00 18 	18008b01     sexb.z.f   r0,r1
  88:	02 0b 3f 18 	183f0b02     sexb.nz.f  r1,0
  8c:	00 00 00 00 
  90:	0b 0b c1 1f 	1fc10b0b     sexb.lt.f  0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 0b 1f 18 	181f0b0c     sexb.le.f  r0,0x200
  9c:	00 02 00 00 
  a0:	04 0b df 1f 	1fdf0b04     sexb.n.f   0,0x200
  a4:	00 02 00 00 
