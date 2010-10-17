#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 82 00 18 	18008200     asr        r0,r1
   4:	00 02 6e 1b 	1b6e0200     asr        fp,sp
   8:	00 82 1f 18 	181f8200     asr        r0,0
   c:	ff 83 3f 18 	183f83ff     asr        r1,-1
  10:	00 02 e1 1f 	1fe10200     asr        0,r2
  14:	00 82 e1 1f 	1fe18200     asr        0,r3
  18:	ff 82 9f 18 	189f82ff     asr        r4,255
  1c:	00 82 e2 1f 	1fe28200     asr        0,r5
  20:	00 83 df 18 	18df8300     asr        r6,-256
  24:	00 82 e3 1f 	1fe38200     asr        0,r7
  28:	00 02 1f 19 	191f0200     asr        r8,0x100
  2c:	00 01 00 00 
  30:	00 02 3f 19 	193f0200     asr        r9,0xffff_feff
  34:	ff fe ff ff 
  38:	00 02 7f 19 	197f0200     asr        r11,0x4242_4242
  3c:	42 42 42 42 
  40:	00 02 ff 1f 	1fff0200     asr        0,0x100
  44:	00 01 00 00 
  48:	00 02 1f 18 	181f0200     asr        r0,0
  4c:	00 00 00 00 
			4c: R_ARC_32	foo
  50:	01 82 45 19 	19458201     asr.z      r10,r11
  54:	02 82 86 19 	19868202     asr.nz     r12,r13
  58:	0b 02 df 19 	19df020b     asr.lt     r14,0
  5c:	00 00 00 00 
  60:	09 02 ff 19 	19ff0209     asr.gt     r15,0x200
  64:	00 02 00 00 
  68:	00 83 00 18 	18008300     asr.f      r0,r1
  6c:	01 82 5e 18 	185e8201     asr.f      r2,1
  70:	00 03 e2 1f 	1fe20300     asr.f      0,r4
  74:	00 03 bf 18 	18bf0300     asr.f      r5,0x200
  78:	00 02 00 00 
  7c:	00 03 df 1f 	1fdf0300     asr.f      0,0x200
  80:	00 02 00 00 
  84:	01 83 00 18 	18008301     asr.z.f    r0,r1
  88:	02 03 3f 18 	183f0302     asr.nz.f   r1,0
  8c:	00 00 00 00 
  90:	0b 03 c1 1f 	1fc1030b     asr.lt.f   0,r2
  94:	00 00 00 00 	00000000                
  98:	0c 03 1f 18 	181f030c     asr.le.f   r0,0x200
  9c:	00 02 00 00 
  a0:	04 03 df 1f 	1fdf0304     asr.n.f    0,0x200
  a4:	00 02 00 00 
