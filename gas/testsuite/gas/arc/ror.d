#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 86 00 18 	18008600     ror        r0,r1
   4:	00 06 6e 1b 	1b6e0600     ror        fp,sp
   8:	00 86 1f 18 	181f8600     ror        r0,0
   c:	ff 87 3f 18 	183f87ff     ror        r1,-1
  10:	00 06 e1 1f 	1fe10600     ror        0,r2
  14:	00 86 e1 1f 	1fe18600     ror        0,r3
  18:	ff 86 9f 18 	189f86ff     ror        r4,255
  1c:	00 86 e2 1f 	1fe28600     ror        0,r5
  20:	00 87 df 18 	18df8700     ror        r6,-256
  24:	00 86 e3 1f 	1fe38600     ror        0,r7
  28:	00 06 1f 19 	191f0600     ror        r8,0x100
  2c:	00 01 00 00 
  30:	00 06 3f 19 	193f0600     ror        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 06 7f 19 	197f0600     ror        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 06 ff 1f 	1fff0600     ror        0,0x100
  44:	00 01 00 00 
  48:	00 06 1f 18 	181f0600     ror        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 86 45 19 	19458601     ror.z      r10,r11
  54:	02 86 86 19 	19868602     ror.nz     r12,r13
  58:	0b 06 df 19 	19df060b     ror.lt     r14,0
  5c:	00 00 00 00 
  60:	09 06 ff 19 	19ff0609     ror.gt     r15,0x200
  64:	00 02 00 00 
  68:	00 87 00 18 	18008700     ror.f      r0,r1
  6c:	01 86 5e 18 	185e8601     ror.f      r2,1
  70:	00 07 e2 1f 	1fe20700     ror.f      0,r4
  74:	00 07 bf 18 	18bf0700     ror.f      r5,0x200
  78:	00 02 00 00 
  7c:	00 07 df 1f 	1fdf0700     ror.f      0,0x200
  80:	00 02 00 00 
  84:	01 87 00 18 	18008701     ror.z.f    r0,r1
  88:	02 07 3f 18 	183f0702     ror.nz.f   r1,0
  8c:	00 00 00 00 
  90:	0b 07 c1 1f 	1fc1070b     ror.lt.f   0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 07 1f 18 	181f070c     ror.le.f   r0,0x200
  9c:	00 02 00 00 
  a0:	04 07 df 1f 	1fdf0704     ror.n.f    0,0x200
  a4:	00 02 00 00 
