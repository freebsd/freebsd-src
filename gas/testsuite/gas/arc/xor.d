#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 78 	78008400     xor        r0,r1,r2
   4:	00 b8 4d 7b 	7b4db800     xor        gp,fp,sp
   8:	00 3e af 7b 	7baf3e00     xor        ilink1,ilink2,blink
   c:	00 f8 1d 7f 	7f1df800     xor        r56,r59,lp_count
  10:	00 fe 00 78 	7800fe00     xor        r0,r1,0
  14:	00 84 1f 78 	781f8400     xor        r0,0,r2
  18:	00 84 e0 7f 	7fe08400     xor        0,r1,r2
  1c:	ff ff 00 78 	7800ffff     xor        r0,r1,-1
  20:	ff 85 1f 78 	781f85ff     xor        r0,-1,r2
  24:	00 84 e0 7f 	7fe08400     xor        0,r1,r2
  28:	ff fe 00 78 	7800feff     xor        r0,r1,255
  2c:	ff 84 1f 78 	781f84ff     xor        r0,255,r2
  30:	00 84 e0 7f 	7fe08400     xor        0,r1,r2
  34:	00 ff 00 78 	7800ff00     xor        r0,r1,-256
  38:	00 85 1f 78 	781f8500     xor        r0,-256,r2
  3c:	00 84 e0 7f 	7fe08400     xor        0,r1,r2
  40:	00 fc 00 78 	7800fc00     xor        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 78 	781f0400     xor        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 78 	781ffcff     xor        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 78 	781f7eff     xor        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 78 	7800fc00     xor        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 78 	78008400     xor        r0,r1,r2
  6c:	00 0a 62 78 	78620a00     xor        r3,r4,r5
  70:	01 90 c3 78 	78c39001     xor.z      r6,r7,r8
  74:	01 16 25 79 	79251601     xor.z      r9,r10,r11
  78:	02 9c 86 79 	79869c02     xor.nz     r12,r13,r14
  7c:	02 22 e8 79 	79e82202     xor.nz     r15,r16,r17
  80:	03 a8 49 7a 	7a49a803     xor.p      r18,r19,r20
  84:	03 2e ab 7a 	7aab2e03     xor.p      r21,r22,r23
  88:	04 b4 0c 7b 	7b0cb404     xor.n      r24,r25,gp
  8c:	04 3a 6e 7b 	7b6e3a04     xor.n      fp,sp,ilink1
  90:	05 c0 cf 7b 	7bcfc005     xor.c      ilink2,blink,r32
  94:	05 46 31 7c 	7c314605     xor.c      r33,r34,r35
  98:	05 cc 92 7c 	7c92cc05     xor.c      r36,r37,r38
  9c:	06 52 f4 7c 	7cf45206     xor.nc     r39,r40,r41
  a0:	06 d8 55 7d 	7d55d806     xor.nc     r42,r43,r44
  a4:	06 5e b7 7d 	7db75e06     xor.nc     r45,r46,r47
  a8:	07 e4 18 7e 	7e18e407     xor.v      r48,r49,r50
  ac:	07 6a 1a 7f 	7f1a6a07     xor.v      r56,r52,r53
  b0:	08 f0 1b 7f 	7f1bf008     xor.nv     r56,r55,r56
  b4:	08 76 1d 7f 	7f1d7608     xor.nv     r56,r58,r59
  b8:	09 00 9e 7f 	7f9e0009     xor.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 78 	78007c0a     xor.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 78 	783f020b     xor.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 78 	787f060d     xor.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 7f 	7fdf080e     xor.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 7f 	7fc2fc0f     xor.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 78 	78008500     xor.f      r0,r1,r2
  e8:	01 fa 00 78 	7800fa01     xor.f      r0,r1,1
  ec:	01 84 1e 78 	781e8401     xor.f      r0,1,r2
  f0:	00 85 e0 7f 	7fe08500     xor.f      0,r1,r2
  f4:	00 fd 00 78 	7800fd00     xor.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 78 	781f0500     xor.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 78 	78008501     xor.z.f    r0,r1,r2
 108:	02 fd 00 78 	7800fd02     xor.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 78 	781f050b     xor.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 7f 	7fc08509     xor.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 78 	7800fd0c     xor.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 78 	781f050a     xor.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
