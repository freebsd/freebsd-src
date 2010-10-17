#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 60 	60008400     and        r0,r1,r2
   4:	00 b8 4d 63 	634db800     and        gp,fp,sp
   8:	00 3e af 63 	63af3e00     and        ilink1,ilink2,blink
   c:	00 f8 1d 67 	671df800     and        r56,r59,lp_count
  10:	00 fe 00 60 	6000fe00     and        r0,r1,0
  14:	00 84 1f 60 	601f8400     and        r0,0,r2
  18:	00 84 e0 67 	67e08400     and        0,r1,r2
  1c:	ff ff 00 60 	6000ffff     and        r0,r1,-1
  20:	ff 85 1f 60 	601f85ff     and        r0,-1,r2
  24:	00 84 e0 67 	67e08400     and        0,r1,r2
  28:	ff fe 00 60 	6000feff     and        r0,r1,255
  2c:	ff 84 1f 60 	601f84ff     and        r0,255,r2
  30:	00 84 e0 67 	67e08400     and        0,r1,r2
  34:	00 ff 00 60 	6000ff00     and        r0,r1,-256
  38:	00 85 1f 60 	601f8500     and        r0,-256,r2
  3c:	00 84 e0 67 	67e08400     and        0,r1,r2
  40:	00 fc 00 60 	6000fc00     and        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 60 	601f0400     and        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 60 	601ffcff     and        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 60 	601f7eff     and        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 60 	6000fc00     and        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 60 	60008400     and        r0,r1,r2
  6c:	00 0a 62 60 	60620a00     and        r3,r4,r5
  70:	01 90 c3 60 	60c39001     and.z      r6,r7,r8
  74:	01 16 25 61 	61251601     and.z      r9,r10,r11
  78:	02 9c 86 61 	61869c02     and.nz     r12,r13,r14
  7c:	02 22 e8 61 	61e82202     and.nz     r15,r16,r17
  80:	03 a8 49 62 	6249a803     and.p      r18,r19,r20
  84:	03 2e ab 62 	62ab2e03     and.p      r21,r22,r23
  88:	04 b4 0c 63 	630cb404     and.n      r24,r25,gp
  8c:	04 3a 6e 63 	636e3a04     and.n      fp,sp,ilink1
  90:	05 c0 cf 63 	63cfc005     and.c      ilink2,blink,r32
  94:	05 46 31 64 	64314605     and.c      r33,r34,r35
  98:	05 cc 92 64 	6492cc05     and.c      r36,r37,r38
  9c:	06 52 f4 64 	64f45206     and.nc     r39,r40,r41
  a0:	06 d8 55 65 	6555d806     and.nc     r42,r43,r44
  a4:	06 5e b7 65 	65b75e06     and.nc     r45,r46,r47
  a8:	07 e4 18 66 	6618e407     and.v      r48,r49,r50
  ac:	07 6a 1a 67 	671a6a07     and.v      r56,r52,r53
  b0:	08 f0 1b 67 	671bf008     and.nv     r56,r55,r56
  b4:	08 76 1d 67 	671d7608     and.nv     r56,r58,r59
  b8:	09 00 9e 67 	679e0009     and.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 60 	60007c0a     and.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 60 	603f020b     and.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 60 	607f060d     and.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 67 	67df080e     and.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 67 	67c2fc0f     and.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 60 	60008500     and.f      r0,r1,r2
  e8:	01 fa 00 60 	6000fa01     and.f      r0,r1,1
  ec:	01 84 1e 60 	601e8401     and.f      r0,1,r2
  f0:	00 85 e0 67 	67e08500     and.f      0,r1,r2
  f4:	00 fd 00 60 	6000fd00     and.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 60 	601f0500     and.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 60 	60008501     and.z.f    r0,r1,r2
 108:	02 fd 00 60 	6000fd02     and.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 60 	601f050b     and.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 67 	67c08509     and.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 60 	6000fd0c     and.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 60 	601f050a     and.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
