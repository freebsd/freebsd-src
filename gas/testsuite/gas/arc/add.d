#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 40 	40008400     add        r0,r1,r2
   4:	00 b8 4d 43 	434db800     add        gp,fp,sp
   8:	00 3e af 43 	43af3e00     add        ilink1,ilink2,blink
   c:	00 f8 1d 47 	471df800     add        r56,r59,lp_count
  10:	00 fe 00 40 	4000fe00     add        r0,r1,0
  14:	00 84 1f 40 	401f8400     add        r0,0,r2
  18:	00 84 e0 47 	47e08400     add        0,r1,r2
  1c:	ff ff 00 40 	4000ffff     add        r0,r1,-1
  20:	ff 85 1f 40 	401f85ff     add        r0,-1,r2
  24:	00 84 e0 47 	47e08400     add        0,r1,r2
  28:	ff fe 00 40 	4000feff     add        r0,r1,255
  2c:	ff 84 1f 40 	401f84ff     add        r0,255,r2
  30:	00 84 e0 47 	47e08400     add        0,r1,r2
  34:	00 ff 00 40 	4000ff00     add        r0,r1,-256
  38:	00 85 1f 40 	401f8500     add        r0,-256,r2
  3c:	00 84 e0 47 	47e08400     add        0,r1,r2
  40:	00 fc 00 40 	4000fc00     add        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 40 	401f0400     add        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 40 	401ffcff     add        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 40 	401f7eff     add        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 40 	4000fc00     add        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 40 	40008400     add        r0,r1,r2
  6c:	00 0a 62 40 	40620a00     add        r3,r4,r5
  70:	01 90 c3 40 	40c39001     add.z      r6,r7,r8
  74:	01 16 25 41 	41251601     add.z      r9,r10,r11
  78:	02 9c 86 41 	41869c02     add.nz     r12,r13,r14
  7c:	02 22 e8 41 	41e82202     add.nz     r15,r16,r17
  80:	03 a8 49 42 	4249a803     add.p      r18,r19,r20
  84:	03 2e ab 42 	42ab2e03     add.p      r21,r22,r23
  88:	04 b4 0c 43 	430cb404     add.n      r24,r25,gp
  8c:	04 3a 6e 43 	436e3a04     add.n      fp,sp,ilink1
  90:	05 c0 cf 43 	43cfc005     add.c      ilink2,blink,r32
  94:	05 46 31 44 	44314605     add.c      r33,r34,r35
  98:	05 cc 92 44 	4492cc05     add.c      r36,r37,r38
  9c:	06 52 f4 44 	44f45206     add.nc     r39,r40,r41
  a0:	06 d8 55 45 	4555d806     add.nc     r42,r43,r44
  a4:	06 5e b7 45 	45b75e06     add.nc     r45,r46,r47
  a8:	07 e4 18 46 	4618e407     add.v      r48,r49,r50
  ac:	07 6a 1a 47 	471a6a07     add.v      r56,r52,r53
  b0:	08 f0 1b 47 	471bf008     add.nv     r56,r55,r56
  b4:	08 76 1d 47 	471d7608     add.nv     r56,r58,r59
  b8:	09 00 9e 47 	479e0009     add.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 40 	40007c0a     add.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 40 	403f020b     add.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 40 	407f060d     add.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 47 	47df080e     add.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 47 	47c2fc0f     add.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 40 	40008500     add.f      r0,r1,r2
  e8:	01 fa 00 40 	4000fa01     add.f      r0,r1,1
  ec:	01 84 1e 40 	401e8401     add.f      r0,1,r2
  f0:	00 85 e0 47 	47e08500     add.f      0,r1,r2
  f4:	00 fd 00 40 	4000fd00     add.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 40 	401f0500     add.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 40 	40008501     add.z.f    r0,r1,r2
 108:	02 fd 00 40 	4000fd02     add.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 40 	401f050b     add.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 47 	47c08509     add.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 40 	4000fd0c     add.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 40 	401f050a     add.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
