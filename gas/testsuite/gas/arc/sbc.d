#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 58 	58008400     sbc        r0,r1,r2
   4:	00 b8 4d 5b 	5b4db800     sbc        gp,fp,sp
   8:	00 3e af 5b 	5baf3e00     sbc        ilink1,ilink2,blink
   c:	00 f8 1d 5f 	5f1df800     sbc        r56,r59,lp_count
  10:	00 fe 00 58 	5800fe00     sbc        r0,r1,0
  14:	00 84 1f 58 	581f8400     sbc        r0,0,r2
  18:	00 84 e0 5f 	5fe08400     sbc        0,r1,r2
  1c:	ff ff 00 58 	5800ffff     sbc        r0,r1,-1
  20:	ff 85 1f 58 	581f85ff     sbc        r0,-1,r2
  24:	00 84 e0 5f 	5fe08400     sbc        0,r1,r2
  28:	ff fe 00 58 	5800feff     sbc        r0,r1,255
  2c:	ff 84 1f 58 	581f84ff     sbc        r0,255,r2
  30:	00 84 e0 5f 	5fe08400     sbc        0,r1,r2
  34:	00 ff 00 58 	5800ff00     sbc        r0,r1,-256
  38:	00 85 1f 58 	581f8500     sbc        r0,-256,r2
  3c:	00 84 e0 5f 	5fe08400     sbc        0,r1,r2
  40:	00 fc 00 58 	5800fc00     sbc        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 58 	581f0400     sbc        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 58 	581ffcff     sbc        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 58 	581f7eff     sbc        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 58 	5800fc00     sbc        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 58 	58008400     sbc        r0,r1,r2
  6c:	00 0a 62 58 	58620a00     sbc        r3,r4,r5
  70:	01 90 c3 58 	58c39001     sbc.z      r6,r7,r8
  74:	01 16 25 59 	59251601     sbc.z      r9,r10,r11
  78:	02 9c 86 59 	59869c02     sbc.nz     r12,r13,r14
  7c:	02 22 e8 59 	59e82202     sbc.nz     r15,r16,r17
  80:	03 a8 49 5a 	5a49a803     sbc.p      r18,r19,r20
  84:	03 2e ab 5a 	5aab2e03     sbc.p      r21,r22,r23
  88:	04 b4 0c 5b 	5b0cb404     sbc.n      r24,r25,gp
  8c:	04 3a 6e 5b 	5b6e3a04     sbc.n      fp,sp,ilink1
  90:	05 c0 cf 5b 	5bcfc005     sbc.c      ilink2,blink,r32
  94:	05 46 31 5c 	5c314605     sbc.c      r33,r34,r35
  98:	05 cc 92 5c 	5c92cc05     sbc.c      r36,r37,r38
  9c:	06 52 f4 5c 	5cf45206     sbc.nc     r39,r40,r41
  a0:	06 d8 55 5d 	5d55d806     sbc.nc     r42,r43,r44
  a4:	06 5e b7 5d 	5db75e06     sbc.nc     r45,r46,r47
  a8:	07 e4 18 5e 	5e18e407     sbc.v      r48,r49,r50
  ac:	07 6a 1a 5f 	5f1a6a07     sbc.v      r56,r52,r53
  b0:	08 f0 1b 5f 	5f1bf008     sbc.nv     r56,r55,r56
  b4:	08 76 1d 5f 	5f1d7608     sbc.nv     r56,r58,r59
  b8:	09 00 9e 5f 	5f9e0009     sbc.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 58 	58007c0a     sbc.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 58 	583f020b     sbc.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 58 	587f060d     sbc.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 5f 	5fdf080e     sbc.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 5f 	5fc2fc0f     sbc.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 58 	58008500     sbc.f      r0,r1,r2
  e8:	01 fa 00 58 	5800fa01     sbc.f      r0,r1,1
  ec:	01 84 1e 58 	581e8401     sbc.f      r0,1,r2
  f0:	00 85 e0 5f 	5fe08500     sbc.f      0,r1,r2
  f4:	00 fd 00 58 	5800fd00     sbc.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 58 	581f0500     sbc.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 58 	58008501     sbc.z.f    r0,r1,r2
 108:	02 fd 00 58 	5800fd02     sbc.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 58 	581f050b     sbc.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 5f 	5fc08509     sbc.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 58 	5800fd0c     sbc.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 58 	581f050a     sbc.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
