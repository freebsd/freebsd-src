#as:
#objdump: -d
#source: rD_rA_rB.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	2020      	add!		r0, r2
   2:	2020      	add!		r0, r2
   4:	2540      	add!		r5, r4
   6:	2540      	add!		r5, r4
   8:	2f40      	add!		r15, r4
   a:	2f40      	add!		r15, r4
   c:	2f30      	add!		r15, r3
   e:	2f30      	add!		r15, r3
  10:	2830      	add!		r8, r3
  12:	2830      	add!		r8, r3
  14:	81ef9811 	add.c		r15, r15, r6
  18:	83579011 	add.c		r26, r23, r4
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	0029      	addc!		r0, r2
  22:	0029      	addc!		r0, r2
  24:	0549      	addc!		r5, r4
  26:	0549      	addc!		r5, r4
  28:	0f49      	addc!		r15, r4
  2a:	0f49      	addc!		r15, r4
  2c:	0f39      	addc!		r15, r3
  2e:	0f39      	addc!		r15, r3
  30:	0839      	addc!		r8, r3
  32:	0839      	addc!		r8, r3
  34:	81ef9813 	addc.c		r15, r15, r6
  38:	83579013 	addc.c		r26, r23, r4
  3c:	0000      	nop!
  3e:	0000      	nop!
  40:	2021      	sub!		r0, r2
  42:	2021      	sub!		r0, r2
  44:	2541      	sub!		r5, r4
  46:	2541      	sub!		r5, r4
  48:	2f41      	sub!		r15, r4
  4a:	2f41      	sub!		r15, r4
  4c:	2f31      	sub!		r15, r3
  4e:	2f31      	sub!		r15, r3
  50:	2831      	sub!		r8, r3
  52:	2831      	sub!		r8, r3
  54:	81ef9815 	sub.c		r15, r15, r6
  58:	83579015 	sub.c		r26, r23, r4
  5c:	0000      	nop!
  5e:	0000      	nop!
  60:	2024      	and!		r0, r2
  62:	2024      	and!		r0, r2
  64:	2544      	and!		r5, r4
  66:	2544      	and!		r5, r4
  68:	2f44      	and!		r15, r4
  6a:	2f44      	and!		r15, r4
  6c:	2f34      	and!		r15, r3
  6e:	2f34      	and!		r15, r3
  70:	2834      	and!		r8, r3
  72:	2834      	and!		r8, r3
  74:	81ef9821 	and.c		r15, r15, r6
  78:	83579021 	and.c		r26, r23, r4
  7c:	0000      	nop!
  7e:	0000      	nop!
  80:	2025      	or!		r0, r2
  82:	2025      	or!		r0, r2
  84:	2545      	or!		r5, r4
  86:	2545      	or!		r5, r4
  88:	2f45      	or!		r15, r4
  8a:	2f45      	or!		r15, r4
  8c:	2f35      	or!		r15, r3
  8e:	2f35      	or!		r15, r3
  90:	2835      	or!		r8, r3
  92:	2835      	or!		r8, r3
  94:	81ef9823 	or.c		r15, r15, r6
  98:	83579023 	or.c		r26, r23, r4
  9c:	0000      	nop!
  9e:	0000      	nop!
  a0:	2027      	xor!		r0, r2
  a2:	2027      	xor!		r0, r2
  a4:	2547      	xor!		r5, r4
  a6:	2547      	xor!		r5, r4
  a8:	2f47      	xor!		r15, r4
  aa:	2f47      	xor!		r15, r4
  ac:	2f37      	xor!		r15, r3
  ae:	2f37      	xor!		r15, r3
  b0:	2837      	xor!		r8, r3
  b2:	2837      	xor!		r8, r3
  b4:	81ef9827 	xor.c		r15, r15, r6
  b8:	83579027 	xor.c		r26, r23, r4
  bc:	0000      	nop!
  be:	0000      	nop!
  c0:	002b      	sra!		r0, r2
  c2:	002b      	sra!		r0, r2
  c4:	054b      	sra!		r5, r4
  c6:	054b      	sra!		r5, r4
  c8:	0f4b      	sra!		r15, r4
  ca:	0f4b      	sra!		r15, r4
  cc:	0f3b      	sra!		r15, r3
  ce:	0f3b      	sra!		r15, r3
  d0:	083b      	sra!		r8, r3
  d2:	083b      	sra!		r8, r3
  d4:	81ef9837 	sra.c		r15, r15, r6
  d8:	83579037 	sra.c		r26, r23, r4
  dc:	0000      	nop!
  de:	0000      	nop!
  e0:	002a      	srl!		r0, r2
  e2:	002a      	srl!		r0, r2
  e4:	054a      	srl!		r5, r4
  e6:	054a      	srl!		r5, r4
  e8:	0f4a      	srl!		r15, r4
  ea:	0f4a      	srl!		r15, r4
  ec:	0f3a      	srl!		r15, r3
  ee:	0f3a      	srl!		r15, r3
  f0:	083a      	srl!		r8, r3
  f2:	083a      	srl!		r8, r3
  f4:	81ef9835 	srl.c		r15, r15, r6
  f8:	83579035 	srl.c		r26, r23, r4
  fc:	0000      	nop!
  fe:	0000      	nop!
 100:	0028      	sll!		r0, r2
 102:	0028      	sll!		r0, r2
 104:	0548      	sll!		r5, r4
 106:	0548      	sll!		r5, r4
 108:	0f48      	sll!		r15, r4
 10a:	0f48      	sll!		r15, r4
 10c:	0f38      	sll!		r15, r3
 10e:	0f38      	sll!		r15, r3
 110:	0838      	sll!		r8, r3
 112:	0838      	sll!		r8, r3
 114:	81ef9831 	sll.c		r15, r15, r6
 118:	83579031 	sll.c		r26, r23, r4
 11c:	0000      	nop!
 11e:	0000      	nop!
 120:	80008811 	add.c		r0, r0, r2
 124:	82958811 	add.c		r20, r21, r2
 128:	81ef9011 	add.c		r15, r15, r4
 12c:	83359011 	add.c		r25, r21, r4
 130:	81ef8c11 	add.c		r15, r15, r3
 134:	83368c11 	add.c		r25, r22, r3
 138:	2870      	add!		r8, r7
 13a:	2870      	add!		r8, r7
 13c:	2640      	add!		r6, r4
 13e:	2640      	add!		r6, r4
 140:	2740      	add!		r7, r4
 142:	2740      	add!		r7, r4
	...
 150:	80008813 	addc.c		r0, r0, r2
 154:	82958813 	addc.c		r20, r21, r2
 158:	81ef9013 	addc.c		r15, r15, r4
 15c:	83359013 	addc.c		r25, r21, r4
 160:	81ef8c13 	addc.c		r15, r15, r3
 164:	83368c13 	addc.c		r25, r22, r3
 168:	0879      	addc!		r8, r7
 16a:	0879      	addc!		r8, r7
 16c:	0649      	addc!		r6, r4
 16e:	0649      	addc!		r6, r4
 170:	0749      	addc!		r7, r4
 172:	0749      	addc!		r7, r4
	...
 180:	80008815 	sub.c		r0, r0, r2
 184:	82958815 	sub.c		r20, r21, r2
 188:	81ef9015 	sub.c		r15, r15, r4
 18c:	83359015 	sub.c		r25, r21, r4
 190:	81ef8c15 	sub.c		r15, r15, r3
 194:	83368c15 	sub.c		r25, r22, r3
 198:	2871      	sub!		r8, r7
 19a:	2871      	sub!		r8, r7
 19c:	2641      	sub!		r6, r4
 19e:	2641      	sub!		r6, r4
 1a0:	2741      	sub!		r7, r4
 1a2:	2741      	sub!		r7, r4
	...
 1b0:	80008821 	and.c		r0, r0, r2
 1b4:	82958821 	and.c		r20, r21, r2
 1b8:	81ef9021 	and.c		r15, r15, r4
 1bc:	83359021 	and.c		r25, r21, r4
 1c0:	81ef8c21 	and.c		r15, r15, r3
 1c4:	83368c21 	and.c		r25, r22, r3
 1c8:	2874      	and!		r8, r7
 1ca:	2874      	and!		r8, r7
 1cc:	2644      	and!		r6, r4
 1ce:	2644      	and!		r6, r4
 1d0:	2744      	and!		r7, r4
 1d2:	2744      	and!		r7, r4
	...
 1e0:	80008823 	or.c		r0, r0, r2
 1e4:	82958823 	or.c		r20, r21, r2
 1e8:	81ef9023 	or.c		r15, r15, r4
 1ec:	83359023 	or.c		r25, r21, r4
 1f0:	81ef8c23 	or.c		r15, r15, r3
 1f4:	83368c23 	or.c		r25, r22, r3
 1f8:	2875      	or!		r8, r7
 1fa:	2875      	or!		r8, r7
 1fc:	2645      	or!		r6, r4
 1fe:	2645      	or!		r6, r4
 200:	2745      	or!		r7, r4
 202:	2745      	or!		r7, r4
	...
 210:	80008827 	xor.c		r0, r0, r2
 214:	82958827 	xor.c		r20, r21, r2
 218:	81ef9027 	xor.c		r15, r15, r4
 21c:	83359027 	xor.c		r25, r21, r4
 220:	81ef8c27 	xor.c		r15, r15, r3
 224:	83368c27 	xor.c		r25, r22, r3
 228:	2877      	xor!		r8, r7
 22a:	2877      	xor!		r8, r7
 22c:	2647      	xor!		r6, r4
 22e:	2647      	xor!		r6, r4
 230:	2747      	xor!		r7, r4
 232:	2747      	xor!		r7, r4
	...
 240:	80008837 	sra.c		r0, r0, r2
 244:	82958837 	sra.c		r20, r21, r2
 248:	81ef9037 	sra.c		r15, r15, r4
 24c:	83359037 	sra.c		r25, r21, r4
 250:	81ef8c37 	sra.c		r15, r15, r3
 254:	83368c37 	sra.c		r25, r22, r3
 258:	087b      	sra!		r8, r7
 25a:	087b      	sra!		r8, r7
 25c:	064b      	sra!		r6, r4
 25e:	064b      	sra!		r6, r4
 260:	074b      	sra!		r7, r4
 262:	074b      	sra!		r7, r4
	...
 270:	80008835 	srl.c		r0, r0, r2
 274:	82958835 	srl.c		r20, r21, r2
 278:	81ef9035 	srl.c		r15, r15, r4
 27c:	83359035 	srl.c		r25, r21, r4
 280:	81ef8c35 	srl.c		r15, r15, r3
 284:	83368c35 	srl.c		r25, r22, r3
 288:	087a      	srl!		r8, r7
 28a:	087a      	srl!		r8, r7
 28c:	064a      	srl!		r6, r4
 28e:	064a      	srl!		r6, r4
 290:	074a      	srl!		r7, r4
 292:	074a      	srl!		r7, r4
	...
 2a0:	80008831 	sll.c		r0, r0, r2
 2a4:	82958831 	sll.c		r20, r21, r2
 2a8:	81ef9031 	sll.c		r15, r15, r4
 2ac:	83359031 	sll.c		r25, r21, r4
 2b0:	81ef8c31 	sll.c		r15, r15, r3
 2b4:	83368c31 	sll.c		r25, r22, r3
 2b8:	0878      	sll!		r8, r7
 2ba:	0878      	sll!		r8, r7
 2bc:	0648      	sll!		r6, r4
 2be:	0648      	sll!		r6, r4
 2c0:	0748      	sll!		r7, r4
 2c2:	0748      	sll!		r7, r4
#pass
