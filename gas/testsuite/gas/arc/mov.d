#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 82 00 60 	60008200     mov        r0,r1
   4:	00 38 6e 63 	636e3800     mov        fp,sp
   8:	00 fe 1f 60 	601ffe00     mov        r0,0
   c:	ff ff 3f 60 	603fffff     mov        r1,-1
  10:	00 04 e1 67 	67e10400     mov        0,r2
  14:	00 86 e1 67 	67e18600     mov        0,r3
  18:	ff fe 9f 60 	609ffeff     mov        r4,255
  1c:	00 8a e2 67 	67e28a00     mov        0,r5
  20:	00 ff df 60 	60dfff00     mov        r6,-256
  24:	00 8e e3 67 	67e38e00     mov        0,r7
  28:	00 7c 1f 61 	611f7c00     mov        r8,0x100
  2c:	00 01 00 00 
  30:	00 7c 3f 61 	613f7c00     mov        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 7c 7f 61 	617f7c00     mov        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 7c ff 67 	67ff7c00     mov        0,0x100
  44:	00 01 00 00 
  48:	00 7c 1f 60 	601f7c00     mov        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	00 82 00 60 	60008200     mov        r0,r1
  54:	00 08 62 60 	60620800     mov        r3,r4
  58:	01 8e c3 60 	60c38e01     mov.z      r6,r7
  5c:	01 14 25 61 	61251401     mov.z      r9,r10
  60:	02 9a 86 61 	61869a02     mov.nz     r12,r13
  64:	02 20 e8 61 	61e82002     mov.nz     r15,r16
  68:	03 a6 49 62 	6249a603     mov.p      r18,r19
  6c:	03 2c ab 62 	62ab2c03     mov.p      r21,r22
  70:	04 b2 0c 63 	630cb204     mov.n      r24,r25
  74:	04 38 6e 63 	636e3804     mov.n      fp,sp
  78:	05 be cf 63 	63cfbe05     mov.c      ilink2,blink
  7c:	05 44 31 64 	64314405     mov.c      r33,r34
  80:	05 ca 92 64 	6492ca05     mov.c      r36,r37
  84:	06 50 f4 64 	64f45006     mov.nc     r39,r40
  88:	06 d6 55 65 	6555d606     mov.nc     r42,r43
  8c:	06 5c b7 65 	65b75c06     mov.nc     r45,r46
  90:	07 e2 18 66 	6618e207     mov.v      r48,r49
  94:	07 64 39 66 	66396407     mov.v      r49,r50
  98:	08 ee 3b 66 	663bee08     mov.nv     r49,r55
  9c:	08 74 3d 66 	663d7408     mov.nv     r49,r58
  a0:	09 78 9e 67 	679e7809     mov.gt     lp_count,lp_count
  a4:	0a 7c 1f 60 	601f7c0a     mov.ge     r0,0
  a8:	00 00 00 00 
  ac:	0c 7c df 67 	67df7c0c     mov.le     0,2
  b0:	02 00 00 00 
  b4:	0d 86 61 60 	6061860d     mov.hi     r3,r3
  b8:	0e 08 82 60 	6082080e     mov.ls     r4,r4
  bc:	0f 8a a2 60 	60a28a0f     mov.pnz    r5,r5
  c0:	00 83 00 60 	60008300     mov.f      r0,r1
  c4:	01 fa 5e 60 	605efa01     mov.f      r2,1
  c8:	00 87 e1 67 	67e18700     mov.f      0,r3
  cc:	00 09 e2 67 	67e20900     mov.f      0,r4
  d0:	00 7d bf 60 	60bf7d00     mov.f      r5,0x200
  d4:	00 02 00 00 
  d8:	00 7d df 67 	67df7d00     mov.f      0,0x200
  dc:	00 02 00 00 
  e0:	01 83 00 60 	60008301     mov.z.f    r0,r1
  e4:	02 7d 3f 60 	603f7d02     mov.nz.f   r1,0
  e8:	00 00 00 00 
