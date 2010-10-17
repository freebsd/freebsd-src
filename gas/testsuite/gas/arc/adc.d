#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 48 	48008400     adc        r0,r1,r2
   4:	00 b8 4d 4b 	4b4db800     adc        gp,fp,sp
   8:	00 3e af 4b 	4baf3e00     adc        ilink1,ilink2,blink
   c:	00 f8 1d 4f 	4f1df800     adc        r56,r59,lp_count
  10:	00 fe 00 48 	4800fe00     adc        r0,r1,0
  14:	00 84 1f 48 	481f8400     adc        r0,0,r2
  18:	00 84 e0 4f 	4fe08400     adc        0,r1,r2
  1c:	ff ff 00 48 	4800ffff     adc        r0,r1,-1
  20:	ff 85 1f 48 	481f85ff     adc        r0,-1,r2
  24:	00 84 e0 4f 	4fe08400     adc        0,r1,r2
  28:	ff fe 00 48 	4800feff     adc        r0,r1,255
  2c:	ff 84 1f 48 	481f84ff     adc        r0,255,r2
  30:	00 84 e0 4f 	4fe08400     adc        0,r1,r2
  34:	00 ff 00 48 	4800ff00     adc        r0,r1,-256
  38:	00 85 1f 48 	481f8500     adc        r0,-256,r2
  3c:	00 84 e0 4f 	4fe08400     adc        0,r1,r2
  40:	00 fc 00 48 	4800fc00     adc        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 48 	481f0400     adc        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 48 	481ffcff     adc        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 48 	481f7eff     adc        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 48 	4800fc00     adc        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 48 	48008400     adc        r0,r1,r2
  6c:	00 0a 62 48 	48620a00     adc        r3,r4,r5
  70:	01 90 c3 48 	48c39001     adc.z      r6,r7,r8
  74:	01 16 25 49 	49251601     adc.z      r9,r10,r11
  78:	02 9c 86 49 	49869c02     adc.nz     r12,r13,r14
  7c:	02 22 e8 49 	49e82202     adc.nz     r15,r16,r17
  80:	03 a8 49 4a 	4a49a803     adc.p      r18,r19,r20
  84:	03 2e ab 4a 	4aab2e03     adc.p      r21,r22,r23
  88:	04 b4 0c 4b 	4b0cb404     adc.n      r24,r25,gp
  8c:	04 3a 6e 4b 	4b6e3a04     adc.n      fp,sp,ilink1
  90:	05 c0 cf 4b 	4bcfc005     adc.c      ilink2,blink,r32
  94:	05 46 31 4c 	4c314605     adc.c      r33,r34,r35
  98:	05 cc 92 4c 	4c92cc05     adc.c      r36,r37,r38
  9c:	06 52 f4 4c 	4cf45206     adc.nc     r39,r40,r41
  a0:	06 d8 55 4d 	4d55d806     adc.nc     r42,r43,r44
  a4:	06 5e b7 4d 	4db75e06     adc.nc     r45,r46,r47
  a8:	07 e4 18 4e 	4e18e407     adc.v      r48,r49,r50
  ac:	07 6a 1a 4f 	4f1a6a07     adc.v      r56,r52,r53
  b0:	08 f0 1b 4f 	4f1bf008     adc.nv     r56,r55,r56
  b4:	08 76 1d 4f 	4f1d7608     adc.nv     r56,r58,r59
  b8:	09 00 9e 4f 	4f9e0009     adc.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 48 	48007c0a     adc.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 48 	483f020b     adc.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 48 	487f060d     adc.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 4f 	4fdf080e     adc.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 4f 	4fc2fc0f     adc.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 48 	48008500     adc.f      r0,r1,r2
  e8:	01 fa 00 48 	4800fa01     adc.f      r0,r1,1
  ec:	01 84 1e 48 	481e8401     adc.f      r0,1,r2
  f0:	00 85 e0 4f 	4fe08500     adc.f      0,r1,r2
  f4:	00 fd 00 48 	4800fd00     adc.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 48 	481f0500     adc.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 48 	48008501     adc.z.f    r0,r1,r2
 108:	02 fd 00 48 	4800fd02     adc.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 48 	481f050b     adc.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 4f 	4fc08509     adc.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 48 	4800fd0c     adc.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 48 	481f050a     adc.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
