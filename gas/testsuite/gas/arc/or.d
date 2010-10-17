#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 68 	68008400     or         r0,r1,r2
   4:	00 b8 4d 6b 	6b4db800     or         gp,fp,sp
   8:	00 3e af 6b 	6baf3e00     or         ilink1,ilink2,blink
   c:	00 f8 1d 6f 	6f1df800     or         r56,r59,lp_count
  10:	00 fe 00 68 	6800fe00     or         r0,r1,0
  14:	00 84 1f 68 	681f8400     or         r0,0,r2
  18:	00 84 e0 6f 	6fe08400     or         0,r1,r2
  1c:	ff ff 00 68 	6800ffff     or         r0,r1,-1
  20:	ff 85 1f 68 	681f85ff     or         r0,-1,r2
  24:	00 84 e0 6f 	6fe08400     or         0,r1,r2
  28:	ff fe 00 68 	6800feff     or         r0,r1,255
  2c:	ff 84 1f 68 	681f84ff     or         r0,255,r2
  30:	00 84 e0 6f 	6fe08400     or         0,r1,r2
  34:	00 ff 00 68 	6800ff00     or         r0,r1,-256
  38:	00 85 1f 68 	681f8500     or         r0,-256,r2
  3c:	00 84 e0 6f 	6fe08400     or         0,r1,r2
  40:	00 fc 00 68 	6800fc00     or         r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 68 	681f0400     or         r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 68 	681ffcff     or         r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 68 	681f7eff     or         r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 68 	6800fc00     or         r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 68 	68008400     or         r0,r1,r2
  6c:	00 0a 62 68 	68620a00     or         r3,r4,r5
  70:	01 90 c3 68 	68c39001     or.z       r6,r7,r8
  74:	01 16 25 69 	69251601     or.z       r9,r10,r11
  78:	02 9c 86 69 	69869c02     or.nz      r12,r13,r14
  7c:	02 22 e8 69 	69e82202     or.nz      r15,r16,r17
  80:	03 a8 49 6a 	6a49a803     or.p       r18,r19,r20
  84:	03 2e ab 6a 	6aab2e03     or.p       r21,r22,r23
  88:	04 b4 0c 6b 	6b0cb404     or.n       r24,r25,gp
  8c:	04 3a 6e 6b 	6b6e3a04     or.n       fp,sp,ilink1
  90:	05 c0 cf 6b 	6bcfc005     or.c       ilink2,blink,r32
  94:	05 46 31 6c 	6c314605     or.c       r33,r34,r35
  98:	05 cc 92 6c 	6c92cc05     or.c       r36,r37,r38
  9c:	06 52 f4 6c 	6cf45206     or.nc      r39,r40,r41
  a0:	06 d8 55 6d 	6d55d806     or.nc      r42,r43,r44
  a4:	06 5e b7 6d 	6db75e06     or.nc      r45,r46,r47
  a8:	07 e4 18 6e 	6e18e407     or.v       r48,r49,r50
  ac:	07 6a 1a 6f 	6f1a6a07     or.v       r56,r52,r53
  b0:	08 f0 1b 6f 	6f1bf008     or.nv      r56,r55,r56
  b4:	08 76 1d 6f 	6f1d7608     or.nv      r56,r58,r59
  b8:	09 00 9e 6f 	6f9e0009     or.gt      lp_count,lp_count,r0
  bc:	0a 7c 00 68 	68007c0a     or.ge      r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 68 	683f020b     or.lt      r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 68 	687f060d     or.hi      r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 6f 	6fdf080e     or.ls      0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 6f 	6fc2fc0f     or.pnz     0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 68 	68008500     or.f       r0,r1,r2
  e8:	01 fa 00 68 	6800fa01     or.f       r0,r1,1
  ec:	01 84 1e 68 	681e8401     or.f       r0,1,r2
  f0:	00 85 e0 6f 	6fe08500     or.f       0,r1,r2
  f4:	00 fd 00 68 	6800fd00     or.f       r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 68 	681f0500     or.f       r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 68 	68008501     or.z.f     r0,r1,r2
 108:	02 fd 00 68 	6800fd02     or.nz.f    r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 68 	681f050b     or.lt.f    r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 6f 	6fc08509     or.gt.f    0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 68 	6800fd0c     or.le.f    r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 68 	681f050a     or.ge.f    r0,0x200,r2
 12c:	00 02 00 00 
