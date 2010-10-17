#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 82 00 40 	40008200     asl        r0,r1
   4:	00 38 6e 43 	436e3800     asl        fp,sp
   8:	00 fe 1f 40 	401ffe00     asl        r0,0
   c:	ff ff 3f 40 	403fffff     asl        r1,-1
  10:	00 04 e1 47 	47e10400     asl        0,r2
  14:	00 86 e1 47 	47e18600     asl        0,r3
  18:	ff fe 9f 40 	409ffeff     asl        r4,255
  1c:	00 8a e2 47 	47e28a00     asl        0,r5
  20:	00 ff df 40 	40dfff00     asl        r6,-256
  24:	00 8e e3 47 	47e38e00     asl        0,r7
  28:	00 7c 1f 41 	411f7c00     asl        r8,0x100
  2c:	00 01 00 00 
  30:	00 7c 3f 41 	413f7c00     asl        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 7c 7f 41 	417f7c00     asl        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 7c ff 47 	47ff7c00     asl        0,0x100
  44:	00 01 00 00 
  48:	00 7c 1f 40 	401f7c00     asl        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	00 82 00 40 	40008200     asl        r0,r1
  54:	00 08 62 40 	40620800     asl        r3,r4
  58:	01 8e c3 40 	40c38e01     asl.z      r6,r7
  5c:	01 14 25 41 	41251401     asl.z      r9,r10
  60:	02 9a 86 41 	41869a02     asl.nz     r12,r13
  64:	02 20 e8 41 	41e82002     asl.nz     r15,r16
  68:	03 a6 49 42 	4249a603     asl.p      r18,r19
  6c:	03 2c ab 42 	42ab2c03     asl.p      r21,r22
  70:	04 b2 0c 43 	430cb204     asl.n      r24,r25
  74:	04 38 6e 43 	436e3804     asl.n      fp,sp
  78:	05 be cf 43 	43cfbe05     asl.c      ilink2,blink
  7c:	05 44 31 44 	44314405     asl.c      r33,r34
  80:	05 ca 92 44 	4492ca05     asl.c      r36,r37
  84:	06 50 f4 44 	44f45006     asl.nc     r39,r40
  88:	06 d6 55 45 	4555d606     asl.nc     r42,r43
  8c:	06 5c b7 45 	45b75c06     asl.nc     r45,r46
  90:	07 e2 18 46 	4618e207     asl.v      r48,r49
  94:	07 64 39 46 	46396407     asl.v      r49,r50
  98:	08 ee 3b 46 	463bee08     asl.nv     r49,r55
  9c:	08 74 3d 46 	463d7408     asl.nv     r49,r58
  a0:	09 78 9e 47 	479e7809     asl.gt     lp_count,lp_count
  a4:	0a 7c 1f 40 	401f7c0a     asl.ge     r0,0
  a8:	00 00 00 00 
  ac:	0c 7c df 47 	47df7c0c     asl.le     0,2
  b0:	02 00 00 00 
  b4:	0d 86 61 40 	4061860d     asl.hi     r3,r3
  b8:	0e 08 82 40 	4082080e     asl.ls     r4,r4
  bc:	0f 8a a2 40 	40a28a0f     asl.pnz    r5,r5
  c0:	00 83 00 40 	40008300     asl.f      r0,r1
  c4:	01 fa 5e 40 	405efa01     asl.f      r2,1
  c8:	00 87 e1 47 	47e18700     asl.f      0,r3
  cc:	00 09 e2 47 	47e20900     asl.f      0,r4
  d0:	00 7d bf 40 	40bf7d00     asl.f      r5,0x200
  d4:	00 02 00 00 
  d8:	00 7d df 47 	47df7d00     asl.f      0,0x200
  dc:	00 02 00 00 
  e0:	01 83 00 40 	40008301     asl.z.f    r0,r1
  e4:	02 7d 3f 40 	403f7d02     asl.nz.f   r1,0
  e8:	00 00 00 00 
