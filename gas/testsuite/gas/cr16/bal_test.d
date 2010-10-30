#as:
#objdump:  -dr
#name:  bal_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	0f c0 22 f1 	bal	\(ra\),\*\+0xff122 <main\+0xff122>:m
   4:	ff c0 26 f1 	bal	\(ra\),\*\+0xfff12a <main\+0xfff12a>:m
   8:	00 c0 22 00 	bal	\(ra\),\*\+0x2a <main\+0x2a>:m
   c:	00 c0 22 01 	bal	\(ra\),\*\+0x12e <main\+0x12e>:m
  10:	00 c0 22 f1 	bal	\(ra\),\*\+0xf132 <main\+0xf132>:m
  14:	00 c0 2a 81 	bal	\(ra\),\*\+0x813e <main\+0x813e>:m
  18:	10 00 00 20 	bal	\(r1,r0\),\*\+0x13a <main\+0x13a>:l
  1c:	22 01 
  1e:	10 00 ac 2f 	bal	\(r11,r10\),\*\+0xcff140 <main\+0xcff140>:l
  22:	22 f1 
  24:	10 00 6a 2f 	bal	\(r7,r6\),\*\+0xaff146 <main\+0xaff146>:l
  28:	22 f1 
  2a:	10 00 38 2f 	bal	\(r4,r3\),\*\+0x8ff14c <main\+0x8ff14c>:l
  2e:	22 f1 
  30:	10 00 7f 2f 	bal	\(r8,r7\),\*\+0xfff152 <main\+0xfff152>:l
  34:	22 f1 
