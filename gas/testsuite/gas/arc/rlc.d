#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 82 00 48 	48008200     rlc        r0,r1
   4:	00 38 6e 4b 	4b6e3800     rlc        fp,sp
   8:	00 fe 1f 48 	481ffe00     rlc        r0,0
   c:	ff ff 3f 48 	483fffff     rlc        r1,-1
  10:	00 04 e1 4f 	4fe10400     rlc        0,r2
  14:	00 86 e1 4f 	4fe18600     rlc        0,r3
  18:	ff fe 9f 48 	489ffeff     rlc        r4,255
  1c:	00 8a e2 4f 	4fe28a00     rlc        0,r5
  20:	00 ff df 48 	48dfff00     rlc        r6,-256
  24:	00 8e e3 4f 	4fe38e00     rlc        0,r7
  28:	00 7c 1f 49 	491f7c00     rlc        r8,0x100
  2c:	00 01 00 00 
  30:	00 7c 3f 49 	493f7c00     rlc        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 7c 7f 49 	497f7c00     rlc        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 7c ff 4f 	4fff7c00     rlc        0,0x100
  44:	00 01 00 00 
  48:	00 7c 1f 48 	481f7c00     rlc        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	00 82 00 48 	48008200     rlc        r0,r1
  54:	00 08 62 48 	48620800     rlc        r3,r4
  58:	01 8e c3 48 	48c38e01     rlc.z      r6,r7
  5c:	01 14 25 49 	49251401     rlc.z      r9,r10
  60:	02 9a 86 49 	49869a02     rlc.nz     r12,r13
  64:	02 20 e8 49 	49e82002     rlc.nz     r15,r16
  68:	03 a6 49 4a 	4a49a603     rlc.p      r18,r19
  6c:	03 2c ab 4a 	4aab2c03     rlc.p      r21,r22
  70:	04 b2 0c 4b 	4b0cb204     rlc.n      r24,r25
  74:	04 38 6e 4b 	4b6e3804     rlc.n      fp,sp
  78:	05 be cf 4b 	4bcfbe05     rlc.c      ilink2,blink
  7c:	05 44 31 4c 	4c314405     rlc.c      r33,r34
  80:	05 ca 92 4c 	4c92ca05     rlc.c      r36,r37
  84:	06 50 f4 4c 	4cf45006     rlc.nc     r39,r40
  88:	06 d6 55 4d 	4d55d606     rlc.nc     r42,r43
  8c:	06 5c b7 4d 	4db75c06     rlc.nc     r45,r46
  90:	07 e2 18 4e 	4e18e207     rlc.v      r48,r49
  94:	07 64 39 4e 	4e396407     rlc.v      r49,r50
  98:	08 ee 3b 4e 	4e3bee08     rlc.nv     r49,r55
  9c:	08 74 3d 4e 	4e3d7408     rlc.nv     r49,r58
  a0:	09 78 9e 4f 	4f9e7809     rlc.gt     lp_count,lp_count
  a4:	0a 7c 1f 48 	481f7c0a     rlc.ge     r0,0
  a8:	00 00 00 00 
  ac:	0c 7c df 4f 	4fdf7c0c     rlc.le     0,2
  b0:	02 00 00 00 
  b4:	0d 86 61 48 	4861860d     rlc.hi     r3,r3
  b8:	0e 08 82 48 	4882080e     rlc.ls     r4,r4
  bc:	0f 8a a2 48 	48a28a0f     rlc.pnz    r5,r5
  c0:	00 83 00 48 	48008300     rlc.f      r0,r1
  c4:	01 fa 5e 48 	485efa01     rlc.f      r2,1
  c8:	00 87 e1 4f 	4fe18700     rlc.f      0,r3
  cc:	00 09 e2 4f 	4fe20900     rlc.f      0,r4
  d0:	00 7d bf 48 	48bf7d00     rlc.f      r5,0x200
  d4:	00 02 00 00 
  d8:	00 7d df 4f 	4fdf7d00     rlc.f      0,0x200
  dc:	00 02 00 00 
  e0:	01 83 00 48 	48008301     rlc.z.f    r0,r1
  e4:	02 7d 3f 48 	483f7d02     rlc.nz.f   r1,0
  e8:	00 00 00 00 
