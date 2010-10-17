#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 70 	70008400     bic        r0,r1,r2
   4:	00 b8 4d 73 	734db800     bic        gp,fp,sp
   8:	00 3e af 73 	73af3e00     bic        ilink1,ilink2,blink
   c:	00 f8 1d 77 	771df800     bic        r56,r59,lp_count
  10:	00 fe 00 70 	7000fe00     bic        r0,r1,0
  14:	00 84 1f 70 	701f8400     bic        r0,0,r2
  18:	00 84 e0 77 	77e08400     bic        0,r1,r2
  1c:	ff ff 00 70 	7000ffff     bic        r0,r1,-1
  20:	ff 85 1f 70 	701f85ff     bic        r0,-1,r2
  24:	00 84 e0 77 	77e08400     bic        0,r1,r2
  28:	ff fe 00 70 	7000feff     bic        r0,r1,255
  2c:	ff 84 1f 70 	701f84ff     bic        r0,255,r2
  30:	00 84 e0 77 	77e08400     bic        0,r1,r2
  34:	00 ff 00 70 	7000ff00     bic        r0,r1,-256
  38:	00 85 1f 70 	701f8500     bic        r0,-256,r2
  3c:	00 84 e0 77 	77e08400     bic        0,r1,r2
  40:	00 fc 00 70 	7000fc00     bic        r0,r1,0x100
  44:	00 01 00 00 
  48:	00 04 1f 70 	701f0400     bic        r0,0xffff_feff,r2
  4c:	ff fe ff ff 
  50:	ff fc 1f 70 	701ffcff     bic        r0,255,0x100
  54:	00 01 00 00 
  58:	ff 7e 1f 70 	701f7eff     bic        r0,0x100,255
  5c:	00 01 00 00 
  60:	00 fc 00 70 	7000fc00     bic        r0,r1,0
  64:	00 00 00 00 
			64: R_ARC_32	foo
  68:	00 84 00 70 	70008400     bic        r0,r1,r2
  6c:	00 0a 62 70 	70620a00     bic        r3,r4,r5
  70:	01 90 c3 70 	70c39001     bic.z      r6,r7,r8
  74:	01 16 25 71 	71251601     bic.z      r9,r10,r11
  78:	02 9c 86 71 	71869c02     bic.nz     r12,r13,r14
  7c:	02 22 e8 71 	71e82202     bic.nz     r15,r16,r17
  80:	03 a8 49 72 	7249a803     bic.p      r18,r19,r20
  84:	03 2e ab 72 	72ab2e03     bic.p      r21,r22,r23
  88:	04 b4 0c 73 	730cb404     bic.n      r24,r25,gp
  8c:	04 3a 6e 73 	736e3a04     bic.n      fp,sp,ilink1
  90:	05 c0 cf 73 	73cfc005     bic.c      ilink2,blink,r32
  94:	05 46 31 74 	74314605     bic.c      r33,r34,r35
  98:	05 cc 92 74 	7492cc05     bic.c      r36,r37,r38
  9c:	06 52 f4 74 	74f45206     bic.nc     r39,r40,r41
  a0:	06 d8 55 75 	7555d806     bic.nc     r42,r43,r44
  a4:	06 5e b7 75 	75b75e06     bic.nc     r45,r46,r47
  a8:	07 e4 18 76 	7618e407     bic.v      r48,r49,r50
  ac:	07 6a 1a 77 	771a6a07     bic.v      r56,r52,r53
  b0:	08 f0 1b 77 	771bf008     bic.nv     r56,r55,r56
  b4:	08 76 1d 77 	771d7608     bic.nv     r56,r58,r59
  b8:	09 00 9e 77 	779e0009     bic.gt     lp_count,lp_count,r0
  bc:	0a 7c 00 70 	70007c0a     bic.ge     r0,r0,0
  c0:	00 00 00 00 
  c4:	0b 02 3f 70 	703f020b     bic.lt     r1,1,r1
  c8:	01 00 00 00 
  cc:	0d 06 7f 70 	707f060d     bic.hi     r3,3,r3
  d0:	03 00 00 00 
  d4:	0e 08 df 77 	77df080e     bic.ls     0,4,r4
  d8:	04 00 00 00 
  dc:	0f fc c2 77 	77c2fc0f     bic.pnz    0,r5,5
  e0:	05 00 00 00 
  e4:	00 85 00 70 	70008500     bic.f      r0,r1,r2
  e8:	01 fa 00 70 	7000fa01     bic.f      r0,r1,1
  ec:	01 84 1e 70 	701e8401     bic.f      r0,1,r2
  f0:	00 85 e0 77 	77e08500     bic.f      0,r1,r2
  f4:	00 fd 00 70 	7000fd00     bic.f      r0,r1,0x200
  f8:	00 02 00 00 
  fc:	00 05 1f 70 	701f0500     bic.f      r0,0x200,r2
 100:	00 02 00 00 
 104:	01 85 00 70 	70008501     bic.z.f    r0,r1,r2
 108:	02 fd 00 70 	7000fd02     bic.nz.f   r0,r1,0
 10c:	00 00 00 00 
 110:	0b 05 1f 70 	701f050b     bic.lt.f   r0,0,r2
 114:	00 00 00 00 
 118:	09 85 c0 77 	77c08509     bic.gt.f   0,r1,r2
 11c:	00 00 00 00 	00000000                
 120:	0c fd 00 70 	7000fd0c     bic.le.f   r0,r1,0x200
 124:	00 02 00 00 
 128:	0a 05 1f 70 	701f050a     bic.ge.f   r0,0x200,r2
 12c:	00 02 00 00 
