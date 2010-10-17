#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 50 	50008400     sub        r0,r1,r2
   4:	00 b8 4d 53 	534db800     sub        gp,fp,sp
   8:	00 3e af 53 	53af3e00     sub        ilink1,ilink2,blink
   c:	00 f8 1d 57 	571df800     sub        r56,r59,lp_count
  10:	00 fe 00 50 	5000fe00     sub        r0,r1,0
  14:	00 84 1f 50 	501f8400     sub        r0,0,r2
  18:	00 84 e0 57 	57e08400     sub        0,r1,r2
  1c:	ff ff 00 50 	5000ffff     sub        r0,r1,-1
  20:	ff 85 1f 50 	501f85ff     sub        r0,-1,r2
  24:	00 84 e0 57 	57e08400     sub        0,r1,r2
  28:	ff fe 00 50 	5000feff     sub        r0,r1,255
  2c:	ff 84 1f 50 	501f84ff     sub        r0,255,r2
  30:	00 84 e0 57 	57e08400     sub        0,r1,r2
  34:	00 ff 00 50 	5000ff00     sub        r0,r1,-256
  38:	00 85 1f 50 	501f8500     sub        r0,-256,r2
  3c:	00 84 e0 57 	57e08400     sub        0,r1,r2
  40:	00 fc 00 50 	5000fc00     sub        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 50 	501f0400     sub        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 50 	501ffcff     sub        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 50 	501f7eff     sub        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 50 	5000fc00     sub        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 50 	50008400     sub        r0,r1,r2
  6c:	00 0a 62 50 	50620a00     sub        r3,r4,r5
  70:	01 90 c3 50 	50c39001     sub.z      r6,r7,r8
  74:	01 16 25 51 	51251601     sub.z      r9,r10,r11
  78:	02 9c 86 51 	51869c02     sub.nz     r12,r13,r14
  7c:	02 22 e8 51 	51e82202     sub.nz     r15,r16,r17
  80:	03 a8 49 52 	5249a803     sub.p      r18,r19,r20
  84:	03 2e ab 52 	52ab2e03     sub.p      r21,r22,r23
  88:	04 b4 0c 53 	530cb404     sub.n      r24,r25,gp
  8c:	04 3a 6e 53 	536e3a04     sub.n      fp,sp,ilink1
  90:	05 c0 cf 53 	53cfc005     sub.c      ilink2,blink,r32
  94:	05 46 31 54 	54314605     sub.c      r33,r34,r35
  98:	05 cc 92 54 	5492cc05     sub.c      r36,r37,r38
  9c:	06 52 f4 54 	54f45206     sub.nc     r39,r40,r41
  a0:	06 d8 55 55 	5555d806     sub.nc     r42,r43,r44
  a4:	06 5e b7 55 	55b75e06     sub.nc     r45,r46,r47
  a8:	07 e4 18 56 	5618e407     sub.v      r48,r49,r50
  ac:	07 6a 1a 57 	571a6a07     sub.v      r56,r52,r53
  b0:	08 f0 1b 57 	571bf008     sub.nv     r56,r55,r56
  b4:	08 76 1d 57 	571d7608     sub.nv     r56,r58,r59
  b8:	09 00 9e 57 	579e0009     sub.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 50 	50007c0a     sub.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 50 	503f020b     sub.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 50 	507f060d     sub.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 57 	57df080e     sub.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 57 	57c2fc0f     sub.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 50 	50008500     sub.f      r0,r1,r2
  e8:	01 fa 00 50 	5000fa01     sub.f      r0,r1,1
  ec:	01 84 1e 50 	501e8401     sub.f      r0,1,r2
  f0:	00 85 e0 57 	57e08500     sub.f      0,r1,r2
  f4:	00 fd 00 50 	5000fd00     sub.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 50 	501f0500     sub.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 50 	50008501     sub.z.f    r0,r1,r2
 108:	02 fd 00 50 	5000fd02     sub.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 50 	501f050b     sub.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 57 	57c08509     sub.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 50 	5000fd0c     sub.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 50 	501f050a     sub.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
