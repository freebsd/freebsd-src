#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 88 00 18 	18008800     rrc        r0,r1
   4:	00 08 6e 1b 	1b6e0800     rrc        fp,sp
   8:	00 88 1f 18 	181f8800     rrc        r0,0
   c:	ff 89 3f 18 	183f89ff     rrc        r1,-1
  10:	00 08 e1 1f 	1fe10800     rrc        0,r2
  14:	00 88 e1 1f 	1fe18800     rrc        0,r3
  18:	ff 88 9f 18 	189f88ff     rrc        r4,255
  1c:	00 88 e2 1f 	1fe28800     rrc        0,r5
  20:	00 89 df 18 	18df8900     rrc        r6,-256
  24:	00 88 e3 1f 	1fe38800     rrc        0,r7
  28:	00 08 1f 19 	191f0800     rrc        r8,0x100
  2c:	00 01 00 00 
  30:	00 08 3f 19 	193f0800     rrc        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 08 7f 19 	197f0800     rrc        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 08 ff 1f 	1fff0800     rrc        0,0x100
  44:	00 01 00 00 
  48:	00 08 1f 18 	181f0800     rrc        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 88 45 19 	19458801     rrc.z      r10,r11
  54:	02 88 86 19 	19868802     rrc.nz     r12,r13
  58:	0b 08 df 19 	19df080b     rrc.lt     r14,0
  5c:	00 00 00 00 
  60:	09 08 ff 19 	19ff0809     rrc.gt     r15,0x200
  64:	00 02 00 00 
  68:	00 89 00 18 	18008900     rrc.f      r0,r1
  6c:	01 88 5e 18 	185e8801     rrc.f      r2,1
  70:	00 09 e2 1f 	1fe20900     rrc.f      0,r4
  74:	00 09 bf 18 	18bf0900     rrc.f      r5,0x200
  78:	00 02 00 00 
  7c:	00 09 df 1f 	1fdf0900     rrc.f      0,0x200
  80:	00 02 00 00 
  84:	01 89 00 18 	18008901     rrc.z.f    r0,r1
  88:	02 09 3f 18 	183f0902     rrc.nz.f   r1,0
  8c:	00 00 00 00 
  90:	0b 09 c1 1f 	1fc1090b     rrc.lt.f   0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 09 1f 18 	181f090c     rrc.le.f   r0,0x200
  9c:	00 02 00 00 
  a0:	04 09 df 1f 	1fdf0904     rrc.n.f    0,0x200
  a4:	00 02 00 00 
