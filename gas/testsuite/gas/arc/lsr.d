#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 18 	18008400     lsr        r0,r1
   4:	00 04 6e 1b 	1b6e0400     lsr        fp,sp
   8:	00 84 1f 18 	181f8400     lsr        r0,0
   c:	ff 85 3f 18 	183f85ff     lsr        r1,-1
  10:	00 04 e1 1f 	1fe10400     lsr        0,r2
  14:	00 84 e1 1f 	1fe18400     lsr        0,r3
  18:	ff 84 9f 18 	189f84ff     lsr        r4,255
  1c:	00 84 e2 1f 	1fe28400     lsr        0,r5
  20:	00 85 df 18 	18df8500     lsr        r6,-256
  24:	00 84 e3 1f 	1fe38400     lsr        0,r7
  28:	00 04 1f 19 	191f0400     lsr        r8,0x100
  2c:	00 01 00 00 
  30:	00 04 3f 19 	193f0400     lsr        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 04 7f 19 	197f0400     lsr        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 04 ff 1f 	1fff0400     lsr        0,0x100
  44:	00 01 00 00 
  48:	00 04 1f 18 	181f0400     lsr        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 84 45 19 	19458401     lsr.z      r10,r11
  54:	02 84 86 19 	19868402     lsr.nz     r12,r13
  58:	0b 04 df 19 	19df040b     lsr.lt     r14,0
  5c:	00 00 00 00 
  60:	09 04 ff 19 	19ff0409     lsr.gt     r15,0x200
  64:	00 02 00 00 
  68:	00 85 00 18 	18008500     lsr.f      r0,r1
  6c:	01 84 5e 18 	185e8401     lsr.f      r2,1
  70:	00 05 e2 1f 	1fe20500     lsr.f      0,r4
  74:	00 05 bf 18 	18bf0500     lsr.f      r5,0x200
  78:	00 02 00 00 
  7c:	00 05 df 1f 	1fdf0500     lsr.f      0,0x200
  80:	00 02 00 00 
  84:	01 85 00 18 	18008501     lsr.z.f    r0,r1
  88:	02 05 3f 18 	183f0502     lsr.nz.f   r1,0
  8c:	00 00 00 00 
  90:	0b 05 c1 1f 	1fc1050b     lsr.lt.f   0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 05 1f 18 	181f050c     lsr.le.f   r0,0x200
  9c:	00 02 00 00 
  a0:	04 05 df 1f 	1fdf0504     lsr.n.f    0,0x200
  a4:	00 02 00 00 
