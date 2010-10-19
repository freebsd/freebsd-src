#name: mcf-emac
#objdump: -d --architecture=m68k:cfv4e
#as: -mcfv4e

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
       0:	a241 0280      	macw %d1l,%a1u,<<,%acc0
       4:	a2d0 d281      	macw %d1l,%a5u,<<,%a0@,%a1,%acc0
       8:	a490 b2a5      	macw %d5l,%a3u,<<,%a0@&,%d2,%acc0
       c:	a13c 00bc 614e 	movel #12345678,%acc0
      12:	a301           	movel %d1,%acc1
      14:	a33c 00bc 614e 	movel #12345678,%acc1
      1a:	a53c 00bc 614e 	movel #12345678,%acc2
      20:	a309           	movel %a1,%acc1
      22:	a73c 00bc 614e 	movel #12345678,%acc3
      28:	a8c3 0640      	macw %d3u,%a4l,>>,%acc1
      2c:	acc5 0040      	macw %d5u,%a6l,%acc1
      30:	a602 0800      	macl %d2,%d3,%acc0
      34:	a682 0800      	macl %d2,%d3,%acc1
      38:	a602 0810      	macl %d2,%d3,%acc2
      3c:	a682 0810      	macl %d2,%d3,%acc3
      40:	a1c1           	movclrl %acc0,%d1
      42:	a3ca           	movclrl %acc1,%a2
      44:	a5c3           	movclrl %acc2,%d3
      46:	a7cd           	movclrl %acc3,%a5
      48:	a381           	movel %acc1,%d1
      4a:	a78b           	movel %acc3,%a3
      4c:	a185           	movel %acc0,%d5
      4e:	a38f           	movel %acc1,%sp
      50:	a110           	movel %acc0,%acc0
      52:	a310           	movel %acc0,%acc1
      54:	a510           	movel %acc0,%acc2
      56:	a710           	movel %acc0,%acc3
      58:	a111           	movel %acc1,%acc0
      5a:	a311           	movel %acc1,%acc1
      5c:	a511           	movel %acc1,%acc2
      5e:	a711           	movel %acc1,%acc3
      60:	a112           	movel %acc2,%acc0
      62:	a312           	movel %acc2,%acc1
      64:	a512           	movel %acc2,%acc2
      66:	a712           	movel %acc2,%acc3
      68:	a113           	movel %acc3,%acc0
      6a:	a313           	movel %acc3,%acc1
      6c:	a513           	movel %acc3,%acc2
      6e:	a713           	movel %acc3,%acc3
      70:	ab88           	movel %accext01,%a0
      72:	af8f           	movel %accext23,%sp
      74:	a180           	movel %acc0,%d0
      76:	a389           	movel %acc1,%a1
      78:	a582           	movel %acc2,%d2
      7a:	a78b           	movel %acc3,%a3
      7c:	a4c9 0080      	macw %a1l,%a2u,%acc1
      80:	a449 0090      	macw %a1l,%a2u,%acc2
      84:	a4c9 0280      	macw %a1l,%a2u,<<,%acc1
      88:	a449 0290      	macw %a1l,%a2u,<<,%acc2
      8c:	a4c9 0680      	macw %a1l,%a2u,>>,%acc1
      90:	a449 0690      	macw %a1l,%a2u,>>,%acc2
      94:	a4c9 0280      	macw %a1l,%a2u,<<,%acc1
      98:	a449 0290      	macw %a1l,%a2u,<<,%acc2
      9c:	a4c9 0680      	macw %a1l,%a2u,>>,%acc1
      a0:	a449 0690      	macw %a1l,%a2u,>>,%acc2
      a4:	a689 0000      	macw %a1l,%d3l,%acc1
      a8:	a609 0010      	macw %a1l,%d3l,%acc2
      ac:	a689 0200      	macw %a1l,%d3l,<<,%acc1
      b0:	a609 0210      	macw %a1l,%d3l,<<,%acc2
      b4:	a689 0600      	macw %a1l,%d3l,>>,%acc1
      b8:	a609 0610      	macw %a1l,%d3l,>>,%acc2
      bc:	a689 0200      	macw %a1l,%d3l,<<,%acc1
      c0:	a609 0210      	macw %a1l,%d3l,<<,%acc2
      c4:	a689 0600      	macw %a1l,%d3l,>>,%acc1
      c8:	a609 0610      	macw %a1l,%d3l,>>,%acc2
      cc:	aec9 0080      	macw %a1l,%a7u,%acc1
      d0:	ae49 0090      	macw %a1l,%a7u,%acc2
      d4:	aec9 0280      	macw %a1l,%a7u,<<,%acc1
      d8:	ae49 0290      	macw %a1l,%a7u,<<,%acc2
      dc:	aec9 0680      	macw %a1l,%a7u,>>,%acc1
      e0:	ae49 0690      	macw %a1l,%a7u,>>,%acc2
      e4:	aec9 0280      	macw %a1l,%a7u,<<,%acc1
      e8:	ae49 0290      	macw %a1l,%a7u,<<,%acc2
      ec:	aec9 0680      	macw %a1l,%a7u,>>,%acc1
      f0:	ae49 0690      	macw %a1l,%a7u,>>,%acc2
      f4:	a289 0000      	macw %a1l,%d1l,%acc1
      f8:	a209 0010      	macw %a1l,%d1l,%acc2
      fc:	a289 0200      	macw %a1l,%d1l,<<,%acc1
     100:	a209 0210      	macw %a1l,%d1l,<<,%acc2
     104:	a289 0600      	macw %a1l,%d1l,>>,%acc1
     108:	a209 0610      	macw %a1l,%d1l,>>,%acc2
     10c:	a289 0200      	macw %a1l,%d1l,<<,%acc1
     110:	a209 0210      	macw %a1l,%d1l,<<,%acc2
     114:	a289 0600      	macw %a1l,%d1l,>>,%acc1
     118:	a209 0610      	macw %a1l,%d1l,>>,%acc2
     11c:	a4c2 00c0      	macw %d2u,%a2u,%acc1
     120:	a442 00d0      	macw %d2u,%a2u,%acc2
     124:	a4c2 02c0      	macw %d2u,%a2u,<<,%acc1
     128:	a442 02d0      	macw %d2u,%a2u,<<,%acc2
     12c:	a4c2 06c0      	macw %d2u,%a2u,>>,%acc1
     130:	a442 06d0      	macw %d2u,%a2u,>>,%acc2
     134:	a4c2 02c0      	macw %d2u,%a2u,<<,%acc1
     138:	a442 02d0      	macw %d2u,%a2u,<<,%acc2
     13c:	a4c2 06c0      	macw %d2u,%a2u,>>,%acc1
     140:	a442 06d0      	macw %d2u,%a2u,>>,%acc2
     144:	a682 0040      	macw %d2u,%d3l,%acc1
     148:	a602 0050      	macw %d2u,%d3l,%acc2
     14c:	a682 0240      	macw %d2u,%d3l,<<,%acc1
     150:	a602 0250      	macw %d2u,%d3l,<<,%acc2
     154:	a682 0640      	macw %d2u,%d3l,>>,%acc1
     158:	a602 0650      	macw %d2u,%d3l,>>,%acc2
     15c:	a682 0240      	macw %d2u,%d3l,<<,%acc1
     160:	a602 0250      	macw %d2u,%d3l,<<,%acc2
     164:	a682 0640      	macw %d2u,%d3l,>>,%acc1
     168:	a602 0650      	macw %d2u,%d3l,>>,%acc2
     16c:	aec2 00c0      	macw %d2u,%a7u,%acc1
     170:	ae42 00d0      	macw %d2u,%a7u,%acc2
     174:	aec2 02c0      	macw %d2u,%a7u,<<,%acc1
     178:	ae42 02d0      	macw %d2u,%a7u,<<,%acc2
     17c:	aec2 06c0      	macw %d2u,%a7u,>>,%acc1
     180:	ae42 06d0      	macw %d2u,%a7u,>>,%acc2
     184:	aec2 02c0      	macw %d2u,%a7u,<<,%acc1
     188:	ae42 02d0      	macw %d2u,%a7u,<<,%acc2
     18c:	aec2 06c0      	macw %d2u,%a7u,>>,%acc1
     190:	ae42 06d0      	macw %d2u,%a7u,>>,%acc2
     194:	a282 0040      	macw %d2u,%d1l,%acc1
     198:	a202 0050      	macw %d2u,%d1l,%acc2
     19c:	a282 0240      	macw %d2u,%d1l,<<,%acc1
     1a0:	a202 0250      	macw %d2u,%d1l,<<,%acc2
     1a4:	a282 0640      	macw %d2u,%d1l,>>,%acc1
     1a8:	a202 0650      	macw %d2u,%d1l,>>,%acc2
     1ac:	a282 0240      	macw %d2u,%d1l,<<,%acc1
     1b0:	a202 0250      	macw %d2u,%d1l,<<,%acc2
     1b4:	a282 0640      	macw %d2u,%d1l,>>,%acc1
     1b8:	a202 0650      	macw %d2u,%d1l,>>,%acc2
     1bc:	a4cd 0080      	macw %a5l,%a2u,%acc1
     1c0:	a44d 0090      	macw %a5l,%a2u,%acc2
     1c4:	a4cd 0280      	macw %a5l,%a2u,<<,%acc1
     1c8:	a44d 0290      	macw %a5l,%a2u,<<,%acc2
     1cc:	a4cd 0680      	macw %a5l,%a2u,>>,%acc1
     1d0:	a44d 0690      	macw %a5l,%a2u,>>,%acc2
     1d4:	a4cd 0280      	macw %a5l,%a2u,<<,%acc1
     1d8:	a44d 0290      	macw %a5l,%a2u,<<,%acc2
     1dc:	a4cd 0680      	macw %a5l,%a2u,>>,%acc1
     1e0:	a44d 0690      	macw %a5l,%a2u,>>,%acc2
     1e4:	a68d 0000      	macw %a5l,%d3l,%acc1
     1e8:	a60d 0010      	macw %a5l,%d3l,%acc2
     1ec:	a68d 0200      	macw %a5l,%d3l,<<,%acc1
     1f0:	a60d 0210      	macw %a5l,%d3l,<<,%acc2
     1f4:	a68d 0600      	macw %a5l,%d3l,>>,%acc1
     1f8:	a60d 0610      	macw %a5l,%d3l,>>,%acc2
     1fc:	a68d 0200      	macw %a5l,%d3l,<<,%acc1
     200:	a60d 0210      	macw %a5l,%d3l,<<,%acc2
     204:	a68d 0600      	macw %a5l,%d3l,>>,%acc1
     208:	a60d 0610      	macw %a5l,%d3l,>>,%acc2
     20c:	aecd 0080      	macw %a5l,%a7u,%acc1
     210:	ae4d 0090      	macw %a5l,%a7u,%acc2
     214:	aecd 0280      	macw %a5l,%a7u,<<,%acc1
     218:	ae4d 0290      	macw %a5l,%a7u,<<,%acc2
     21c:	aecd 0680      	macw %a5l,%a7u,>>,%acc1
     220:	ae4d 0690      	macw %a5l,%a7u,>>,%acc2
     224:	aecd 0280      	macw %a5l,%a7u,<<,%acc1
     228:	ae4d 0290      	macw %a5l,%a7u,<<,%acc2
     22c:	aecd 0680      	macw %a5l,%a7u,>>,%acc1
     230:	ae4d 0690      	macw %a5l,%a7u,>>,%acc2
     234:	a28d 0000      	macw %a5l,%d1l,%acc1
     238:	a20d 0010      	macw %a5l,%d1l,%acc2
     23c:	a28d 0200      	macw %a5l,%d1l,<<,%acc1
     240:	a20d 0210      	macw %a5l,%d1l,<<,%acc2
     244:	a28d 0600      	macw %a5l,%d1l,>>,%acc1
     248:	a20d 0610      	macw %a5l,%d1l,>>,%acc2
     24c:	a28d 0200      	macw %a5l,%d1l,<<,%acc1
     250:	a20d 0210      	macw %a5l,%d1l,<<,%acc2
     254:	a28d 0600      	macw %a5l,%d1l,>>,%acc1
     258:	a20d 0610      	macw %a5l,%d1l,>>,%acc2
     25c:	a4c6 00c0      	macw %d6u,%a2u,%acc1
     260:	a446 00d0      	macw %d6u,%a2u,%acc2
     264:	a4c6 02c0      	macw %d6u,%a2u,<<,%acc1
     268:	a446 02d0      	macw %d6u,%a2u,<<,%acc2
     26c:	a4c6 06c0      	macw %d6u,%a2u,>>,%acc1
     270:	a446 06d0      	macw %d6u,%a2u,>>,%acc2
     274:	a4c6 02c0      	macw %d6u,%a2u,<<,%acc1
     278:	a446 02d0      	macw %d6u,%a2u,<<,%acc2
     27c:	a4c6 06c0      	macw %d6u,%a2u,>>,%acc1
     280:	a446 06d0      	macw %d6u,%a2u,>>,%acc2
     284:	a686 0040      	macw %d6u,%d3l,%acc1
     288:	a606 0050      	macw %d6u,%d3l,%acc2
     28c:	a686 0240      	macw %d6u,%d3l,<<,%acc1
     290:	a606 0250      	macw %d6u,%d3l,<<,%acc2
     294:	a686 0640      	macw %d6u,%d3l,>>,%acc1
     298:	a606 0650      	macw %d6u,%d3l,>>,%acc2
     29c:	a686 0240      	macw %d6u,%d3l,<<,%acc1
     2a0:	a606 0250      	macw %d6u,%d3l,<<,%acc2
     2a4:	a686 0640      	macw %d6u,%d3l,>>,%acc1
     2a8:	a606 0650      	macw %d6u,%d3l,>>,%acc2
     2ac:	aec6 00c0      	macw %d6u,%a7u,%acc1
     2b0:	ae46 00d0      	macw %d6u,%a7u,%acc2
     2b4:	aec6 02c0      	macw %d6u,%a7u,<<,%acc1
     2b8:	ae46 02d0      	macw %d6u,%a7u,<<,%acc2
     2bc:	aec6 06c0      	macw %d6u,%a7u,>>,%acc1
     2c0:	ae46 06d0      	macw %d6u,%a7u,>>,%acc2
     2c4:	aec6 02c0      	macw %d6u,%a7u,<<,%acc1
     2c8:	ae46 02d0      	macw %d6u,%a7u,<<,%acc2
     2cc:	aec6 06c0      	macw %d6u,%a7u,>>,%acc1
     2d0:	ae46 06d0      	macw %d6u,%a7u,>>,%acc2
     2d4:	a286 0040      	macw %d6u,%d1l,%acc1
     2d8:	a206 0050      	macw %d6u,%d1l,%acc2
     2dc:	a286 0240      	macw %d6u,%d1l,<<,%acc1
     2e0:	a206 0250      	macw %d6u,%d1l,<<,%acc2
     2e4:	a286 0640      	macw %d6u,%d1l,>>,%acc1
     2e8:	a206 0650      	macw %d6u,%d1l,>>,%acc2
     2ec:	a286 0240      	macw %d6u,%d1l,<<,%acc1
     2f0:	a206 0250      	macw %d6u,%d1l,<<,%acc2
     2f4:	a286 0640      	macw %d6u,%d1l,>>,%acc1
     2f8:	a206 0650      	macw %d6u,%d1l,>>,%acc2
     2fc:	a213 a089      	macw %a1l,%a2u,%a3@,%d1,%acc1
     300:	a293 a099      	macw %a1l,%a2u,%a3@,%d1,%acc2
     304:	a653 a089      	macw %a1l,%a2u,%a3@,%a3,%acc1
     308:	a6d3 a099      	macw %a1l,%a2u,%a3@,%a3,%acc2
     30c:	a413 a089      	macw %a1l,%a2u,%a3@,%d2,%acc1
     310:	a493 a099      	macw %a1l,%a2u,%a3@,%d2,%acc2
     314:	ae53 a089      	macw %a1l,%a2u,%a3@,%sp,%acc1
     318:	aed3 a099      	macw %a1l,%a2u,%a3@,%sp,%acc2
     31c:	a213 a0a9      	macw %a1l,%a2u,%a3@&,%d1,%acc1
     320:	a293 a0b9      	macw %a1l,%a2u,%a3@&,%d1,%acc2
     324:	a653 a0a9      	macw %a1l,%a2u,%a3@&,%a3,%acc1
     328:	a6d3 a0b9      	macw %a1l,%a2u,%a3@&,%a3,%acc2
     32c:	a413 a0a9      	macw %a1l,%a2u,%a3@&,%d2,%acc1
     330:	a493 a0b9      	macw %a1l,%a2u,%a3@&,%d2,%acc2
     334:	ae53 a0a9      	macw %a1l,%a2u,%a3@&,%sp,%acc1
     338:	aed3 a0b9      	macw %a1l,%a2u,%a3@&,%sp,%acc2
     33c:	a21a a089      	macw %a1l,%a2u,%a2@\+,%d1,%acc1
     340:	a29a a099      	macw %a1l,%a2u,%a2@\+,%d1,%acc2
     344:	a65a a089      	macw %a1l,%a2u,%a2@\+,%a3,%acc1
     348:	a6da a099      	macw %a1l,%a2u,%a2@\+,%a3,%acc2
     34c:	a41a a089      	macw %a1l,%a2u,%a2@\+,%d2,%acc1
     350:	a49a a099      	macw %a1l,%a2u,%a2@\+,%d2,%acc2
     354:	ae5a a089      	macw %a1l,%a2u,%a2@\+,%sp,%acc1
     358:	aeda a099      	macw %a1l,%a2u,%a2@\+,%sp,%acc2
     35c:	a21a a0a9      	macw %a1l,%a2u,%a2@\+&,%d1,%acc1
     360:	a29a a0b9      	macw %a1l,%a2u,%a2@\+&,%d1,%acc2
     364:	a65a a0a9      	macw %a1l,%a2u,%a2@\+&,%a3,%acc1
     368:	a6da a0b9      	macw %a1l,%a2u,%a2@\+&,%a3,%acc2
     36c:	a41a a0a9      	macw %a1l,%a2u,%a2@\+&,%d2,%acc1
     370:	a49a a0b9      	macw %a1l,%a2u,%a2@\+&,%d2,%acc2
     374:	ae5a a0a9      	macw %a1l,%a2u,%a2@\+&,%sp,%acc1
     378:	aeda a0b9      	macw %a1l,%a2u,%a2@\+&,%sp,%acc2
     37c:	a22e a089 000a 	macw %a1l,%a2u,%fp@\(10\),%d1,%acc1
     382:	a2ae a099 000a 	macw %a1l,%a2u,%fp@\(10\),%d1,%acc2
     388:	a66e a089 000a 	macw %a1l,%a2u,%fp@\(10\),%a3,%acc1
     38e:	a6ee a099 000a 	macw %a1l,%a2u,%fp@\(10\),%a3,%acc2
     394:	a42e a089 000a 	macw %a1l,%a2u,%fp@\(10\),%d2,%acc1
     39a:	a4ae a099 000a 	macw %a1l,%a2u,%fp@\(10\),%d2,%acc2
     3a0:	ae6e a089 000a 	macw %a1l,%a2u,%fp@\(10\),%sp,%acc1
     3a6:	aeee a099 000a 	macw %a1l,%a2u,%fp@\(10\),%sp,%acc2
     3ac:	a22e a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d1,%acc1
     3b2:	a2ae a0b9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d1,%acc2
     3b8:	a66e a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%a3,%acc1
     3be:	a6ee a0b9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%a3,%acc2
     3c4:	a42e a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d2,%acc1
     3ca:	a4ae a0b9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d2,%acc2
     3d0:	ae6e a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%sp,%acc1
     3d6:	aeee a0b9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%sp,%acc2
     3dc:	a221 a089      	macw %a1l,%a2u,%a1@-,%d1,%acc1
     3e0:	a2a1 a099      	macw %a1l,%a2u,%a1@-,%d1,%acc2
     3e4:	a661 a089      	macw %a1l,%a2u,%a1@-,%a3,%acc1
     3e8:	a6e1 a099      	macw %a1l,%a2u,%a1@-,%a3,%acc2
     3ec:	a421 a089      	macw %a1l,%a2u,%a1@-,%d2,%acc1
     3f0:	a4a1 a099      	macw %a1l,%a2u,%a1@-,%d2,%acc2
     3f4:	ae61 a089      	macw %a1l,%a2u,%a1@-,%sp,%acc1
     3f8:	aee1 a099      	macw %a1l,%a2u,%a1@-,%sp,%acc2
     3fc:	a221 a0a9      	macw %a1l,%a2u,%a1@-&,%d1,%acc1
     400:	a2a1 a0b9      	macw %a1l,%a2u,%a1@-&,%d1,%acc2
     404:	a661 a0a9      	macw %a1l,%a2u,%a1@-&,%a3,%acc1
     408:	a6e1 a0b9      	macw %a1l,%a2u,%a1@-&,%a3,%acc2
     40c:	a421 a0a9      	macw %a1l,%a2u,%a1@-&,%d2,%acc1
     410:	a4a1 a0b9      	macw %a1l,%a2u,%a1@-&,%d2,%acc2
     414:	ae61 a0a9      	macw %a1l,%a2u,%a1@-&,%sp,%acc1
     418:	aee1 a0b9      	macw %a1l,%a2u,%a1@-&,%sp,%acc2
     41c:	a213 a289      	macw %a1l,%a2u,<<,%a3@,%d1,%acc1
     420:	a293 a299      	macw %a1l,%a2u,<<,%a3@,%d1,%acc2
     424:	a653 a289      	macw %a1l,%a2u,<<,%a3@,%a3,%acc1
     428:	a6d3 a299      	macw %a1l,%a2u,<<,%a3@,%a3,%acc2
     42c:	a413 a289      	macw %a1l,%a2u,<<,%a3@,%d2,%acc1
     430:	a493 a299      	macw %a1l,%a2u,<<,%a3@,%d2,%acc2
     434:	ae53 a289      	macw %a1l,%a2u,<<,%a3@,%sp,%acc1
     438:	aed3 a299      	macw %a1l,%a2u,<<,%a3@,%sp,%acc2
     43c:	a213 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d1,%acc1
     440:	a293 a2b9      	macw %a1l,%a2u,<<,%a3@&,%d1,%acc2
     444:	a653 a2a9      	macw %a1l,%a2u,<<,%a3@&,%a3,%acc1
     448:	a6d3 a2b9      	macw %a1l,%a2u,<<,%a3@&,%a3,%acc2
     44c:	a413 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d2,%acc1
     450:	a493 a2b9      	macw %a1l,%a2u,<<,%a3@&,%d2,%acc2
     454:	ae53 a2a9      	macw %a1l,%a2u,<<,%a3@&,%sp,%acc1
     458:	aed3 a2b9      	macw %a1l,%a2u,<<,%a3@&,%sp,%acc2
     45c:	a21a a289      	macw %a1l,%a2u,<<,%a2@\+,%d1,%acc1
     460:	a29a a299      	macw %a1l,%a2u,<<,%a2@\+,%d1,%acc2
     464:	a65a a289      	macw %a1l,%a2u,<<,%a2@\+,%a3,%acc1
     468:	a6da a299      	macw %a1l,%a2u,<<,%a2@\+,%a3,%acc2
     46c:	a41a a289      	macw %a1l,%a2u,<<,%a2@\+,%d2,%acc1
     470:	a49a a299      	macw %a1l,%a2u,<<,%a2@\+,%d2,%acc2
     474:	ae5a a289      	macw %a1l,%a2u,<<,%a2@\+,%sp,%acc1
     478:	aeda a299      	macw %a1l,%a2u,<<,%a2@\+,%sp,%acc2
     47c:	a21a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d1,%acc1
     480:	a29a a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%d1,%acc2
     484:	a65a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%a3,%acc1
     488:	a6da a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%a3,%acc2
     48c:	a41a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d2,%acc1
     490:	a49a a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%d2,%acc2
     494:	ae5a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%sp,%acc1
     498:	aeda a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%sp,%acc2
     49c:	a22e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1,%acc1
     4a2:	a2ae a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1,%acc2
     4a8:	a66e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3,%acc1
     4ae:	a6ee a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3,%acc2
     4b4:	a42e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2,%acc1
     4ba:	a4ae a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2,%acc2
     4c0:	ae6e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp,%acc1
     4c6:	aeee a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp,%acc2
     4cc:	a22e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1,%acc1
     4d2:	a2ae a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1,%acc2
     4d8:	a66e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3,%acc1
     4de:	a6ee a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3,%acc2
     4e4:	a42e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2,%acc1
     4ea:	a4ae a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2,%acc2
     4f0:	ae6e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp,%acc1
     4f6:	aeee a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp,%acc2
     4fc:	a221 a289      	macw %a1l,%a2u,<<,%a1@-,%d1,%acc1
     500:	a2a1 a299      	macw %a1l,%a2u,<<,%a1@-,%d1,%acc2
     504:	a661 a289      	macw %a1l,%a2u,<<,%a1@-,%a3,%acc1
     508:	a6e1 a299      	macw %a1l,%a2u,<<,%a1@-,%a3,%acc2
     50c:	a421 a289      	macw %a1l,%a2u,<<,%a1@-,%d2,%acc1
     510:	a4a1 a299      	macw %a1l,%a2u,<<,%a1@-,%d2,%acc2
     514:	ae61 a289      	macw %a1l,%a2u,<<,%a1@-,%sp,%acc1
     518:	aee1 a299      	macw %a1l,%a2u,<<,%a1@-,%sp,%acc2
     51c:	a221 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d1,%acc1
     520:	a2a1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%d1,%acc2
     524:	a661 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%a3,%acc1
     528:	a6e1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%a3,%acc2
     52c:	a421 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d2,%acc1
     530:	a4a1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%d2,%acc2
     534:	ae61 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%sp,%acc1
     538:	aee1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%sp,%acc2
     53c:	a213 a689      	macw %a1l,%a2u,>>,%a3@,%d1,%acc1
     540:	a293 a699      	macw %a1l,%a2u,>>,%a3@,%d1,%acc2
     544:	a653 a689      	macw %a1l,%a2u,>>,%a3@,%a3,%acc1
     548:	a6d3 a699      	macw %a1l,%a2u,>>,%a3@,%a3,%acc2
     54c:	a413 a689      	macw %a1l,%a2u,>>,%a3@,%d2,%acc1
     550:	a493 a699      	macw %a1l,%a2u,>>,%a3@,%d2,%acc2
     554:	ae53 a689      	macw %a1l,%a2u,>>,%a3@,%sp,%acc1
     558:	aed3 a699      	macw %a1l,%a2u,>>,%a3@,%sp,%acc2
     55c:	a213 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d1,%acc1
     560:	a293 a6b9      	macw %a1l,%a2u,>>,%a3@&,%d1,%acc2
     564:	a653 a6a9      	macw %a1l,%a2u,>>,%a3@&,%a3,%acc1
     568:	a6d3 a6b9      	macw %a1l,%a2u,>>,%a3@&,%a3,%acc2
     56c:	a413 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d2,%acc1
     570:	a493 a6b9      	macw %a1l,%a2u,>>,%a3@&,%d2,%acc2
     574:	ae53 a6a9      	macw %a1l,%a2u,>>,%a3@&,%sp,%acc1
     578:	aed3 a6b9      	macw %a1l,%a2u,>>,%a3@&,%sp,%acc2
     57c:	a21a a689      	macw %a1l,%a2u,>>,%a2@\+,%d1,%acc1
     580:	a29a a699      	macw %a1l,%a2u,>>,%a2@\+,%d1,%acc2
     584:	a65a a689      	macw %a1l,%a2u,>>,%a2@\+,%a3,%acc1
     588:	a6da a699      	macw %a1l,%a2u,>>,%a2@\+,%a3,%acc2
     58c:	a41a a689      	macw %a1l,%a2u,>>,%a2@\+,%d2,%acc1
     590:	a49a a699      	macw %a1l,%a2u,>>,%a2@\+,%d2,%acc2
     594:	ae5a a689      	macw %a1l,%a2u,>>,%a2@\+,%sp,%acc1
     598:	aeda a699      	macw %a1l,%a2u,>>,%a2@\+,%sp,%acc2
     59c:	a21a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d1,%acc1
     5a0:	a29a a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%d1,%acc2
     5a4:	a65a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%a3,%acc1
     5a8:	a6da a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%a3,%acc2
     5ac:	a41a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d2,%acc1
     5b0:	a49a a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%d2,%acc2
     5b4:	ae5a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%sp,%acc1
     5b8:	aeda a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%sp,%acc2
     5bc:	a22e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1,%acc1
     5c2:	a2ae a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1,%acc2
     5c8:	a66e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3,%acc1
     5ce:	a6ee a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3,%acc2
     5d4:	a42e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2,%acc1
     5da:	a4ae a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2,%acc2
     5e0:	ae6e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp,%acc1
     5e6:	aeee a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp,%acc2
     5ec:	a22e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1,%acc1
     5f2:	a2ae a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1,%acc2
     5f8:	a66e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3,%acc1
     5fe:	a6ee a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3,%acc2
     604:	a42e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2,%acc1
     60a:	a4ae a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2,%acc2
     610:	ae6e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp,%acc1
     616:	aeee a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp,%acc2
     61c:	a221 a689      	macw %a1l,%a2u,>>,%a1@-,%d1,%acc1
     620:	a2a1 a699      	macw %a1l,%a2u,>>,%a1@-,%d1,%acc2
     624:	a661 a689      	macw %a1l,%a2u,>>,%a1@-,%a3,%acc1
     628:	a6e1 a699      	macw %a1l,%a2u,>>,%a1@-,%a3,%acc2
     62c:	a421 a689      	macw %a1l,%a2u,>>,%a1@-,%d2,%acc1
     630:	a4a1 a699      	macw %a1l,%a2u,>>,%a1@-,%d2,%acc2
     634:	ae61 a689      	macw %a1l,%a2u,>>,%a1@-,%sp,%acc1
     638:	aee1 a699      	macw %a1l,%a2u,>>,%a1@-,%sp,%acc2
     63c:	a221 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d1,%acc1
     640:	a2a1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%d1,%acc2
     644:	a661 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%a3,%acc1
     648:	a6e1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%a3,%acc2
     64c:	a421 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d2,%acc1
     650:	a4a1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%d2,%acc2
     654:	ae61 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%sp,%acc1
     658:	aee1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%sp,%acc2
     65c:	a213 a289      	macw %a1l,%a2u,<<,%a3@,%d1,%acc1
     660:	a293 a299      	macw %a1l,%a2u,<<,%a3@,%d1,%acc2
     664:	a653 a289      	macw %a1l,%a2u,<<,%a3@,%a3,%acc1
     668:	a6d3 a299      	macw %a1l,%a2u,<<,%a3@,%a3,%acc2
     66c:	a413 a289      	macw %a1l,%a2u,<<,%a3@,%d2,%acc1
     670:	a493 a299      	macw %a1l,%a2u,<<,%a3@,%d2,%acc2
     674:	ae53 a289      	macw %a1l,%a2u,<<,%a3@,%sp,%acc1
     678:	aed3 a299      	macw %a1l,%a2u,<<,%a3@,%sp,%acc2
     67c:	a213 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d1,%acc1
     680:	a293 a2b9      	macw %a1l,%a2u,<<,%a3@&,%d1,%acc2
     684:	a653 a2a9      	macw %a1l,%a2u,<<,%a3@&,%a3,%acc1
     688:	a6d3 a2b9      	macw %a1l,%a2u,<<,%a3@&,%a3,%acc2
     68c:	a413 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d2,%acc1
     690:	a493 a2b9      	macw %a1l,%a2u,<<,%a3@&,%d2,%acc2
     694:	ae53 a2a9      	macw %a1l,%a2u,<<,%a3@&,%sp,%acc1
     698:	aed3 a2b9      	macw %a1l,%a2u,<<,%a3@&,%sp,%acc2
     69c:	a21a a289      	macw %a1l,%a2u,<<,%a2@\+,%d1,%acc1
     6a0:	a29a a299      	macw %a1l,%a2u,<<,%a2@\+,%d1,%acc2
     6a4:	a65a a289      	macw %a1l,%a2u,<<,%a2@\+,%a3,%acc1
     6a8:	a6da a299      	macw %a1l,%a2u,<<,%a2@\+,%a3,%acc2
     6ac:	a41a a289      	macw %a1l,%a2u,<<,%a2@\+,%d2,%acc1
     6b0:	a49a a299      	macw %a1l,%a2u,<<,%a2@\+,%d2,%acc2
     6b4:	ae5a a289      	macw %a1l,%a2u,<<,%a2@\+,%sp,%acc1
     6b8:	aeda a299      	macw %a1l,%a2u,<<,%a2@\+,%sp,%acc2
     6bc:	a21a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d1,%acc1
     6c0:	a29a a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%d1,%acc2
     6c4:	a65a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%a3,%acc1
     6c8:	a6da a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%a3,%acc2
     6cc:	a41a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d2,%acc1
     6d0:	a49a a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%d2,%acc2
     6d4:	ae5a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%sp,%acc1
     6d8:	aeda a2b9      	macw %a1l,%a2u,<<,%a2@\+&,%sp,%acc2
     6dc:	a22e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1,%acc1
     6e2:	a2ae a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1,%acc2
     6e8:	a66e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3,%acc1
     6ee:	a6ee a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3,%acc2
     6f4:	a42e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2,%acc1
     6fa:	a4ae a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2,%acc2
     700:	ae6e a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp,%acc1
     706:	aeee a299 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp,%acc2
     70c:	a22e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1,%acc1
     712:	a2ae a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1,%acc2
     718:	a66e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3,%acc1
     71e:	a6ee a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3,%acc2
     724:	a42e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2,%acc1
     72a:	a4ae a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2,%acc2
     730:	ae6e a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp,%acc1
     736:	aeee a2b9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp,%acc2
     73c:	a221 a289      	macw %a1l,%a2u,<<,%a1@-,%d1,%acc1
     740:	a2a1 a299      	macw %a1l,%a2u,<<,%a1@-,%d1,%acc2
     744:	a661 a289      	macw %a1l,%a2u,<<,%a1@-,%a3,%acc1
     748:	a6e1 a299      	macw %a1l,%a2u,<<,%a1@-,%a3,%acc2
     74c:	a421 a289      	macw %a1l,%a2u,<<,%a1@-,%d2,%acc1
     750:	a4a1 a299      	macw %a1l,%a2u,<<,%a1@-,%d2,%acc2
     754:	ae61 a289      	macw %a1l,%a2u,<<,%a1@-,%sp,%acc1
     758:	aee1 a299      	macw %a1l,%a2u,<<,%a1@-,%sp,%acc2
     75c:	a221 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d1,%acc1
     760:	a2a1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%d1,%acc2
     764:	a661 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%a3,%acc1
     768:	a6e1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%a3,%acc2
     76c:	a421 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d2,%acc1
     770:	a4a1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%d2,%acc2
     774:	ae61 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%sp,%acc1
     778:	aee1 a2b9      	macw %a1l,%a2u,<<,%a1@-&,%sp,%acc2
     77c:	a213 a689      	macw %a1l,%a2u,>>,%a3@,%d1,%acc1
     780:	a293 a699      	macw %a1l,%a2u,>>,%a3@,%d1,%acc2
     784:	a653 a689      	macw %a1l,%a2u,>>,%a3@,%a3,%acc1
     788:	a6d3 a699      	macw %a1l,%a2u,>>,%a3@,%a3,%acc2
     78c:	a413 a689      	macw %a1l,%a2u,>>,%a3@,%d2,%acc1
     790:	a493 a699      	macw %a1l,%a2u,>>,%a3@,%d2,%acc2
     794:	ae53 a689      	macw %a1l,%a2u,>>,%a3@,%sp,%acc1
     798:	aed3 a699      	macw %a1l,%a2u,>>,%a3@,%sp,%acc2
     79c:	a213 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d1,%acc1
     7a0:	a293 a6b9      	macw %a1l,%a2u,>>,%a3@&,%d1,%acc2
     7a4:	a653 a6a9      	macw %a1l,%a2u,>>,%a3@&,%a3,%acc1
     7a8:	a6d3 a6b9      	macw %a1l,%a2u,>>,%a3@&,%a3,%acc2
     7ac:	a413 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d2,%acc1
     7b0:	a493 a6b9      	macw %a1l,%a2u,>>,%a3@&,%d2,%acc2
     7b4:	ae53 a6a9      	macw %a1l,%a2u,>>,%a3@&,%sp,%acc1
     7b8:	aed3 a6b9      	macw %a1l,%a2u,>>,%a3@&,%sp,%acc2
     7bc:	a21a a689      	macw %a1l,%a2u,>>,%a2@\+,%d1,%acc1
     7c0:	a29a a699      	macw %a1l,%a2u,>>,%a2@\+,%d1,%acc2
     7c4:	a65a a689      	macw %a1l,%a2u,>>,%a2@\+,%a3,%acc1
     7c8:	a6da a699      	macw %a1l,%a2u,>>,%a2@\+,%a3,%acc2
     7cc:	a41a a689      	macw %a1l,%a2u,>>,%a2@\+,%d2,%acc1
     7d0:	a49a a699      	macw %a1l,%a2u,>>,%a2@\+,%d2,%acc2
     7d4:	ae5a a689      	macw %a1l,%a2u,>>,%a2@\+,%sp,%acc1
     7d8:	aeda a699      	macw %a1l,%a2u,>>,%a2@\+,%sp,%acc2
     7dc:	a21a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d1,%acc1
     7e0:	a29a a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%d1,%acc2
     7e4:	a65a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%a3,%acc1
     7e8:	a6da a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%a3,%acc2
     7ec:	a41a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d2,%acc1
     7f0:	a49a a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%d2,%acc2
     7f4:	ae5a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%sp,%acc1
     7f8:	aeda a6b9      	macw %a1l,%a2u,>>,%a2@\+&,%sp,%acc2
     7fc:	a22e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1,%acc1
     802:	a2ae a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1,%acc2
     808:	a66e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3,%acc1
     80e:	a6ee a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3,%acc2
     814:	a42e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2,%acc1
     81a:	a4ae a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2,%acc2
     820:	ae6e a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp,%acc1
     826:	aeee a699 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp,%acc2
     82c:	a22e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1,%acc1
     832:	a2ae a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1,%acc2
     838:	a66e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3,%acc1
     83e:	a6ee a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3,%acc2
     844:	a42e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2,%acc1
     84a:	a4ae a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2,%acc2
     850:	ae6e a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp,%acc1
     856:	aeee a6b9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp,%acc2
     85c:	a221 a689      	macw %a1l,%a2u,>>,%a1@-,%d1,%acc1
     860:	a2a1 a699      	macw %a1l,%a2u,>>,%a1@-,%d1,%acc2
     864:	a661 a689      	macw %a1l,%a2u,>>,%a1@-,%a3,%acc1
     868:	a6e1 a699      	macw %a1l,%a2u,>>,%a1@-,%a3,%acc2
     86c:	a421 a689      	macw %a1l,%a2u,>>,%a1@-,%d2,%acc1
     870:	a4a1 a699      	macw %a1l,%a2u,>>,%a1@-,%d2,%acc2
     874:	ae61 a689      	macw %a1l,%a2u,>>,%a1@-,%sp,%acc1
     878:	aee1 a699      	macw %a1l,%a2u,>>,%a1@-,%sp,%acc2
     87c:	a221 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d1,%acc1
     880:	a2a1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%d1,%acc2
     884:	a661 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%a3,%acc1
     888:	a6e1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%a3,%acc2
     88c:	a421 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d2,%acc1
     890:	a4a1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%d2,%acc2
     894:	ae61 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%sp,%acc1
     898:	aee1 a6b9      	macw %a1l,%a2u,>>,%a1@-&,%sp,%acc2
     89c:	a213 3009      	macw %a1l,%d3l,%a3@,%d1,%acc1
     8a0:	a293 3019      	macw %a1l,%d3l,%a3@,%d1,%acc2
     8a4:	a653 3009      	macw %a1l,%d3l,%a3@,%a3,%acc1
     8a8:	a6d3 3019      	macw %a1l,%d3l,%a3@,%a3,%acc2
     8ac:	a413 3009      	macw %a1l,%d3l,%a3@,%d2,%acc1
     8b0:	a493 3019      	macw %a1l,%d3l,%a3@,%d2,%acc2
     8b4:	ae53 3009      	macw %a1l,%d3l,%a3@,%sp,%acc1
     8b8:	aed3 3019      	macw %a1l,%d3l,%a3@,%sp,%acc2
     8bc:	a213 3029      	macw %a1l,%d3l,%a3@&,%d1,%acc1
     8c0:	a293 3039      	macw %a1l,%d3l,%a3@&,%d1,%acc2
     8c4:	a653 3029      	macw %a1l,%d3l,%a3@&,%a3,%acc1
     8c8:	a6d3 3039      	macw %a1l,%d3l,%a3@&,%a3,%acc2
     8cc:	a413 3029      	macw %a1l,%d3l,%a3@&,%d2,%acc1
     8d0:	a493 3039      	macw %a1l,%d3l,%a3@&,%d2,%acc2
     8d4:	ae53 3029      	macw %a1l,%d3l,%a3@&,%sp,%acc1
     8d8:	aed3 3039      	macw %a1l,%d3l,%a3@&,%sp,%acc2
     8dc:	a21a 3009      	macw %a1l,%d3l,%a2@\+,%d1,%acc1
     8e0:	a29a 3019      	macw %a1l,%d3l,%a2@\+,%d1,%acc2
     8e4:	a65a 3009      	macw %a1l,%d3l,%a2@\+,%a3,%acc1
     8e8:	a6da 3019      	macw %a1l,%d3l,%a2@\+,%a3,%acc2
     8ec:	a41a 3009      	macw %a1l,%d3l,%a2@\+,%d2,%acc1
     8f0:	a49a 3019      	macw %a1l,%d3l,%a2@\+,%d2,%acc2
     8f4:	ae5a 3009      	macw %a1l,%d3l,%a2@\+,%sp,%acc1
     8f8:	aeda 3019      	macw %a1l,%d3l,%a2@\+,%sp,%acc2
     8fc:	a21a 3029      	macw %a1l,%d3l,%a2@\+&,%d1,%acc1
     900:	a29a 3039      	macw %a1l,%d3l,%a2@\+&,%d1,%acc2
     904:	a65a 3029      	macw %a1l,%d3l,%a2@\+&,%a3,%acc1
     908:	a6da 3039      	macw %a1l,%d3l,%a2@\+&,%a3,%acc2
     90c:	a41a 3029      	macw %a1l,%d3l,%a2@\+&,%d2,%acc1
     910:	a49a 3039      	macw %a1l,%d3l,%a2@\+&,%d2,%acc2
     914:	ae5a 3029      	macw %a1l,%d3l,%a2@\+&,%sp,%acc1
     918:	aeda 3039      	macw %a1l,%d3l,%a2@\+&,%sp,%acc2
     91c:	a22e 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%d1,%acc1
     922:	a2ae 3019 000a 	macw %a1l,%d3l,%fp@\(10\),%d1,%acc2
     928:	a66e 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%a3,%acc1
     92e:	a6ee 3019 000a 	macw %a1l,%d3l,%fp@\(10\),%a3,%acc2
     934:	a42e 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%d2,%acc1
     93a:	a4ae 3019 000a 	macw %a1l,%d3l,%fp@\(10\),%d2,%acc2
     940:	ae6e 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%sp,%acc1
     946:	aeee 3019 000a 	macw %a1l,%d3l,%fp@\(10\),%sp,%acc2
     94c:	a22e 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d1,%acc1
     952:	a2ae 3039 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d1,%acc2
     958:	a66e 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%a3,%acc1
     95e:	a6ee 3039 000a 	macw %a1l,%d3l,%fp@\(10\)&,%a3,%acc2
     964:	a42e 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d2,%acc1
     96a:	a4ae 3039 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d2,%acc2
     970:	ae6e 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%sp,%acc1
     976:	aeee 3039 000a 	macw %a1l,%d3l,%fp@\(10\)&,%sp,%acc2
     97c:	a221 3009      	macw %a1l,%d3l,%a1@-,%d1,%acc1
     980:	a2a1 3019      	macw %a1l,%d3l,%a1@-,%d1,%acc2
     984:	a661 3009      	macw %a1l,%d3l,%a1@-,%a3,%acc1
     988:	a6e1 3019      	macw %a1l,%d3l,%a1@-,%a3,%acc2
     98c:	a421 3009      	macw %a1l,%d3l,%a1@-,%d2,%acc1
     990:	a4a1 3019      	macw %a1l,%d3l,%a1@-,%d2,%acc2
     994:	ae61 3009      	macw %a1l,%d3l,%a1@-,%sp,%acc1
     998:	aee1 3019      	macw %a1l,%d3l,%a1@-,%sp,%acc2
     99c:	a221 3029      	macw %a1l,%d3l,%a1@-&,%d1,%acc1
     9a0:	a2a1 3039      	macw %a1l,%d3l,%a1@-&,%d1,%acc2
     9a4:	a661 3029      	macw %a1l,%d3l,%a1@-&,%a3,%acc1
     9a8:	a6e1 3039      	macw %a1l,%d3l,%a1@-&,%a3,%acc2
     9ac:	a421 3029      	macw %a1l,%d3l,%a1@-&,%d2,%acc1
     9b0:	a4a1 3039      	macw %a1l,%d3l,%a1@-&,%d2,%acc2
     9b4:	ae61 3029      	macw %a1l,%d3l,%a1@-&,%sp,%acc1
     9b8:	aee1 3039      	macw %a1l,%d3l,%a1@-&,%sp,%acc2
     9bc:	a213 3209      	macw %a1l,%d3l,<<,%a3@,%d1,%acc1
     9c0:	a293 3219      	macw %a1l,%d3l,<<,%a3@,%d1,%acc2
     9c4:	a653 3209      	macw %a1l,%d3l,<<,%a3@,%a3,%acc1
     9c8:	a6d3 3219      	macw %a1l,%d3l,<<,%a3@,%a3,%acc2
     9cc:	a413 3209      	macw %a1l,%d3l,<<,%a3@,%d2,%acc1
     9d0:	a493 3219      	macw %a1l,%d3l,<<,%a3@,%d2,%acc2
     9d4:	ae53 3209      	macw %a1l,%d3l,<<,%a3@,%sp,%acc1
     9d8:	aed3 3219      	macw %a1l,%d3l,<<,%a3@,%sp,%acc2
     9dc:	a213 3229      	macw %a1l,%d3l,<<,%a3@&,%d1,%acc1
     9e0:	a293 3239      	macw %a1l,%d3l,<<,%a3@&,%d1,%acc2
     9e4:	a653 3229      	macw %a1l,%d3l,<<,%a3@&,%a3,%acc1
     9e8:	a6d3 3239      	macw %a1l,%d3l,<<,%a3@&,%a3,%acc2
     9ec:	a413 3229      	macw %a1l,%d3l,<<,%a3@&,%d2,%acc1
     9f0:	a493 3239      	macw %a1l,%d3l,<<,%a3@&,%d2,%acc2
     9f4:	ae53 3229      	macw %a1l,%d3l,<<,%a3@&,%sp,%acc1
     9f8:	aed3 3239      	macw %a1l,%d3l,<<,%a3@&,%sp,%acc2
     9fc:	a21a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d1,%acc1
     a00:	a29a 3219      	macw %a1l,%d3l,<<,%a2@\+,%d1,%acc2
     a04:	a65a 3209      	macw %a1l,%d3l,<<,%a2@\+,%a3,%acc1
     a08:	a6da 3219      	macw %a1l,%d3l,<<,%a2@\+,%a3,%acc2
     a0c:	a41a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d2,%acc1
     a10:	a49a 3219      	macw %a1l,%d3l,<<,%a2@\+,%d2,%acc2
     a14:	ae5a 3209      	macw %a1l,%d3l,<<,%a2@\+,%sp,%acc1
     a18:	aeda 3219      	macw %a1l,%d3l,<<,%a2@\+,%sp,%acc2
     a1c:	a21a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d1,%acc1
     a20:	a29a 3239      	macw %a1l,%d3l,<<,%a2@\+&,%d1,%acc2
     a24:	a65a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%a3,%acc1
     a28:	a6da 3239      	macw %a1l,%d3l,<<,%a2@\+&,%a3,%acc2
     a2c:	a41a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d2,%acc1
     a30:	a49a 3239      	macw %a1l,%d3l,<<,%a2@\+&,%d2,%acc2
     a34:	ae5a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%sp,%acc1
     a38:	aeda 3239      	macw %a1l,%d3l,<<,%a2@\+&,%sp,%acc2
     a3c:	a22e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1,%acc1
     a42:	a2ae 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1,%acc2
     a48:	a66e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3,%acc1
     a4e:	a6ee 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3,%acc2
     a54:	a42e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2,%acc1
     a5a:	a4ae 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2,%acc2
     a60:	ae6e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp,%acc1
     a66:	aeee 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp,%acc2
     a6c:	a22e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1,%acc1
     a72:	a2ae 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1,%acc2
     a78:	a66e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3,%acc1
     a7e:	a6ee 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3,%acc2
     a84:	a42e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2,%acc1
     a8a:	a4ae 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2,%acc2
     a90:	ae6e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp,%acc1
     a96:	aeee 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp,%acc2
     a9c:	a221 3209      	macw %a1l,%d3l,<<,%a1@-,%d1,%acc1
     aa0:	a2a1 3219      	macw %a1l,%d3l,<<,%a1@-,%d1,%acc2
     aa4:	a661 3209      	macw %a1l,%d3l,<<,%a1@-,%a3,%acc1
     aa8:	a6e1 3219      	macw %a1l,%d3l,<<,%a1@-,%a3,%acc2
     aac:	a421 3209      	macw %a1l,%d3l,<<,%a1@-,%d2,%acc1
     ab0:	a4a1 3219      	macw %a1l,%d3l,<<,%a1@-,%d2,%acc2
     ab4:	ae61 3209      	macw %a1l,%d3l,<<,%a1@-,%sp,%acc1
     ab8:	aee1 3219      	macw %a1l,%d3l,<<,%a1@-,%sp,%acc2
     abc:	a221 3229      	macw %a1l,%d3l,<<,%a1@-&,%d1,%acc1
     ac0:	a2a1 3239      	macw %a1l,%d3l,<<,%a1@-&,%d1,%acc2
     ac4:	a661 3229      	macw %a1l,%d3l,<<,%a1@-&,%a3,%acc1
     ac8:	a6e1 3239      	macw %a1l,%d3l,<<,%a1@-&,%a3,%acc2
     acc:	a421 3229      	macw %a1l,%d3l,<<,%a1@-&,%d2,%acc1
     ad0:	a4a1 3239      	macw %a1l,%d3l,<<,%a1@-&,%d2,%acc2
     ad4:	ae61 3229      	macw %a1l,%d3l,<<,%a1@-&,%sp,%acc1
     ad8:	aee1 3239      	macw %a1l,%d3l,<<,%a1@-&,%sp,%acc2
     adc:	a213 3609      	macw %a1l,%d3l,>>,%a3@,%d1,%acc1
     ae0:	a293 3619      	macw %a1l,%d3l,>>,%a3@,%d1,%acc2
     ae4:	a653 3609      	macw %a1l,%d3l,>>,%a3@,%a3,%acc1
     ae8:	a6d3 3619      	macw %a1l,%d3l,>>,%a3@,%a3,%acc2
     aec:	a413 3609      	macw %a1l,%d3l,>>,%a3@,%d2,%acc1
     af0:	a493 3619      	macw %a1l,%d3l,>>,%a3@,%d2,%acc2
     af4:	ae53 3609      	macw %a1l,%d3l,>>,%a3@,%sp,%acc1
     af8:	aed3 3619      	macw %a1l,%d3l,>>,%a3@,%sp,%acc2
     afc:	a213 3629      	macw %a1l,%d3l,>>,%a3@&,%d1,%acc1
     b00:	a293 3639      	macw %a1l,%d3l,>>,%a3@&,%d1,%acc2
     b04:	a653 3629      	macw %a1l,%d3l,>>,%a3@&,%a3,%acc1
     b08:	a6d3 3639      	macw %a1l,%d3l,>>,%a3@&,%a3,%acc2
     b0c:	a413 3629      	macw %a1l,%d3l,>>,%a3@&,%d2,%acc1
     b10:	a493 3639      	macw %a1l,%d3l,>>,%a3@&,%d2,%acc2
     b14:	ae53 3629      	macw %a1l,%d3l,>>,%a3@&,%sp,%acc1
     b18:	aed3 3639      	macw %a1l,%d3l,>>,%a3@&,%sp,%acc2
     b1c:	a21a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d1,%acc1
     b20:	a29a 3619      	macw %a1l,%d3l,>>,%a2@\+,%d1,%acc2
     b24:	a65a 3609      	macw %a1l,%d3l,>>,%a2@\+,%a3,%acc1
     b28:	a6da 3619      	macw %a1l,%d3l,>>,%a2@\+,%a3,%acc2
     b2c:	a41a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d2,%acc1
     b30:	a49a 3619      	macw %a1l,%d3l,>>,%a2@\+,%d2,%acc2
     b34:	ae5a 3609      	macw %a1l,%d3l,>>,%a2@\+,%sp,%acc1
     b38:	aeda 3619      	macw %a1l,%d3l,>>,%a2@\+,%sp,%acc2
     b3c:	a21a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d1,%acc1
     b40:	a29a 3639      	macw %a1l,%d3l,>>,%a2@\+&,%d1,%acc2
     b44:	a65a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%a3,%acc1
     b48:	a6da 3639      	macw %a1l,%d3l,>>,%a2@\+&,%a3,%acc2
     b4c:	a41a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d2,%acc1
     b50:	a49a 3639      	macw %a1l,%d3l,>>,%a2@\+&,%d2,%acc2
     b54:	ae5a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%sp,%acc1
     b58:	aeda 3639      	macw %a1l,%d3l,>>,%a2@\+&,%sp,%acc2
     b5c:	a22e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1,%acc1
     b62:	a2ae 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1,%acc2
     b68:	a66e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3,%acc1
     b6e:	a6ee 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3,%acc2
     b74:	a42e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2,%acc1
     b7a:	a4ae 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2,%acc2
     b80:	ae6e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp,%acc1
     b86:	aeee 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp,%acc2
     b8c:	a22e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1,%acc1
     b92:	a2ae 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1,%acc2
     b98:	a66e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3,%acc1
     b9e:	a6ee 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3,%acc2
     ba4:	a42e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2,%acc1
     baa:	a4ae 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2,%acc2
     bb0:	ae6e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp,%acc1
     bb6:	aeee 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp,%acc2
     bbc:	a221 3609      	macw %a1l,%d3l,>>,%a1@-,%d1,%acc1
     bc0:	a2a1 3619      	macw %a1l,%d3l,>>,%a1@-,%d1,%acc2
     bc4:	a661 3609      	macw %a1l,%d3l,>>,%a1@-,%a3,%acc1
     bc8:	a6e1 3619      	macw %a1l,%d3l,>>,%a1@-,%a3,%acc2
     bcc:	a421 3609      	macw %a1l,%d3l,>>,%a1@-,%d2,%acc1
     bd0:	a4a1 3619      	macw %a1l,%d3l,>>,%a1@-,%d2,%acc2
     bd4:	ae61 3609      	macw %a1l,%d3l,>>,%a1@-,%sp,%acc1
     bd8:	aee1 3619      	macw %a1l,%d3l,>>,%a1@-,%sp,%acc2
     bdc:	a221 3629      	macw %a1l,%d3l,>>,%a1@-&,%d1,%acc1
     be0:	a2a1 3639      	macw %a1l,%d3l,>>,%a1@-&,%d1,%acc2
     be4:	a661 3629      	macw %a1l,%d3l,>>,%a1@-&,%a3,%acc1
     be8:	a6e1 3639      	macw %a1l,%d3l,>>,%a1@-&,%a3,%acc2
     bec:	a421 3629      	macw %a1l,%d3l,>>,%a1@-&,%d2,%acc1
     bf0:	a4a1 3639      	macw %a1l,%d3l,>>,%a1@-&,%d2,%acc2
     bf4:	ae61 3629      	macw %a1l,%d3l,>>,%a1@-&,%sp,%acc1
     bf8:	aee1 3639      	macw %a1l,%d3l,>>,%a1@-&,%sp,%acc2
     bfc:	a213 3209      	macw %a1l,%d3l,<<,%a3@,%d1,%acc1
     c00:	a293 3219      	macw %a1l,%d3l,<<,%a3@,%d1,%acc2
     c04:	a653 3209      	macw %a1l,%d3l,<<,%a3@,%a3,%acc1
     c08:	a6d3 3219      	macw %a1l,%d3l,<<,%a3@,%a3,%acc2
     c0c:	a413 3209      	macw %a1l,%d3l,<<,%a3@,%d2,%acc1
     c10:	a493 3219      	macw %a1l,%d3l,<<,%a3@,%d2,%acc2
     c14:	ae53 3209      	macw %a1l,%d3l,<<,%a3@,%sp,%acc1
     c18:	aed3 3219      	macw %a1l,%d3l,<<,%a3@,%sp,%acc2
     c1c:	a213 3229      	macw %a1l,%d3l,<<,%a3@&,%d1,%acc1
     c20:	a293 3239      	macw %a1l,%d3l,<<,%a3@&,%d1,%acc2
     c24:	a653 3229      	macw %a1l,%d3l,<<,%a3@&,%a3,%acc1
     c28:	a6d3 3239      	macw %a1l,%d3l,<<,%a3@&,%a3,%acc2
     c2c:	a413 3229      	macw %a1l,%d3l,<<,%a3@&,%d2,%acc1
     c30:	a493 3239      	macw %a1l,%d3l,<<,%a3@&,%d2,%acc2
     c34:	ae53 3229      	macw %a1l,%d3l,<<,%a3@&,%sp,%acc1
     c38:	aed3 3239      	macw %a1l,%d3l,<<,%a3@&,%sp,%acc2
     c3c:	a21a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d1,%acc1
     c40:	a29a 3219      	macw %a1l,%d3l,<<,%a2@\+,%d1,%acc2
     c44:	a65a 3209      	macw %a1l,%d3l,<<,%a2@\+,%a3,%acc1
     c48:	a6da 3219      	macw %a1l,%d3l,<<,%a2@\+,%a3,%acc2
     c4c:	a41a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d2,%acc1
     c50:	a49a 3219      	macw %a1l,%d3l,<<,%a2@\+,%d2,%acc2
     c54:	ae5a 3209      	macw %a1l,%d3l,<<,%a2@\+,%sp,%acc1
     c58:	aeda 3219      	macw %a1l,%d3l,<<,%a2@\+,%sp,%acc2
     c5c:	a21a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d1,%acc1
     c60:	a29a 3239      	macw %a1l,%d3l,<<,%a2@\+&,%d1,%acc2
     c64:	a65a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%a3,%acc1
     c68:	a6da 3239      	macw %a1l,%d3l,<<,%a2@\+&,%a3,%acc2
     c6c:	a41a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d2,%acc1
     c70:	a49a 3239      	macw %a1l,%d3l,<<,%a2@\+&,%d2,%acc2
     c74:	ae5a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%sp,%acc1
     c78:	aeda 3239      	macw %a1l,%d3l,<<,%a2@\+&,%sp,%acc2
     c7c:	a22e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1,%acc1
     c82:	a2ae 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1,%acc2
     c88:	a66e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3,%acc1
     c8e:	a6ee 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3,%acc2
     c94:	a42e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2,%acc1
     c9a:	a4ae 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2,%acc2
     ca0:	ae6e 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp,%acc1
     ca6:	aeee 3219 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp,%acc2
     cac:	a22e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1,%acc1
     cb2:	a2ae 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1,%acc2
     cb8:	a66e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3,%acc1
     cbe:	a6ee 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3,%acc2
     cc4:	a42e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2,%acc1
     cca:	a4ae 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2,%acc2
     cd0:	ae6e 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp,%acc1
     cd6:	aeee 3239 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp,%acc2
     cdc:	a221 3209      	macw %a1l,%d3l,<<,%a1@-,%d1,%acc1
     ce0:	a2a1 3219      	macw %a1l,%d3l,<<,%a1@-,%d1,%acc2
     ce4:	a661 3209      	macw %a1l,%d3l,<<,%a1@-,%a3,%acc1
     ce8:	a6e1 3219      	macw %a1l,%d3l,<<,%a1@-,%a3,%acc2
     cec:	a421 3209      	macw %a1l,%d3l,<<,%a1@-,%d2,%acc1
     cf0:	a4a1 3219      	macw %a1l,%d3l,<<,%a1@-,%d2,%acc2
     cf4:	ae61 3209      	macw %a1l,%d3l,<<,%a1@-,%sp,%acc1
     cf8:	aee1 3219      	macw %a1l,%d3l,<<,%a1@-,%sp,%acc2
     cfc:	a221 3229      	macw %a1l,%d3l,<<,%a1@-&,%d1,%acc1
     d00:	a2a1 3239      	macw %a1l,%d3l,<<,%a1@-&,%d1,%acc2
     d04:	a661 3229      	macw %a1l,%d3l,<<,%a1@-&,%a3,%acc1
     d08:	a6e1 3239      	macw %a1l,%d3l,<<,%a1@-&,%a3,%acc2
     d0c:	a421 3229      	macw %a1l,%d3l,<<,%a1@-&,%d2,%acc1
     d10:	a4a1 3239      	macw %a1l,%d3l,<<,%a1@-&,%d2,%acc2
     d14:	ae61 3229      	macw %a1l,%d3l,<<,%a1@-&,%sp,%acc1
     d18:	aee1 3239      	macw %a1l,%d3l,<<,%a1@-&,%sp,%acc2
     d1c:	a213 3609      	macw %a1l,%d3l,>>,%a3@,%d1,%acc1
     d20:	a293 3619      	macw %a1l,%d3l,>>,%a3@,%d1,%acc2
     d24:	a653 3609      	macw %a1l,%d3l,>>,%a3@,%a3,%acc1
     d28:	a6d3 3619      	macw %a1l,%d3l,>>,%a3@,%a3,%acc2
     d2c:	a413 3609      	macw %a1l,%d3l,>>,%a3@,%d2,%acc1
     d30:	a493 3619      	macw %a1l,%d3l,>>,%a3@,%d2,%acc2
     d34:	ae53 3609      	macw %a1l,%d3l,>>,%a3@,%sp,%acc1
     d38:	aed3 3619      	macw %a1l,%d3l,>>,%a3@,%sp,%acc2
     d3c:	a213 3629      	macw %a1l,%d3l,>>,%a3@&,%d1,%acc1
     d40:	a293 3639      	macw %a1l,%d3l,>>,%a3@&,%d1,%acc2
     d44:	a653 3629      	macw %a1l,%d3l,>>,%a3@&,%a3,%acc1
     d48:	a6d3 3639      	macw %a1l,%d3l,>>,%a3@&,%a3,%acc2
     d4c:	a413 3629      	macw %a1l,%d3l,>>,%a3@&,%d2,%acc1
     d50:	a493 3639      	macw %a1l,%d3l,>>,%a3@&,%d2,%acc2
     d54:	ae53 3629      	macw %a1l,%d3l,>>,%a3@&,%sp,%acc1
     d58:	aed3 3639      	macw %a1l,%d3l,>>,%a3@&,%sp,%acc2
     d5c:	a21a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d1,%acc1
     d60:	a29a 3619      	macw %a1l,%d3l,>>,%a2@\+,%d1,%acc2
     d64:	a65a 3609      	macw %a1l,%d3l,>>,%a2@\+,%a3,%acc1
     d68:	a6da 3619      	macw %a1l,%d3l,>>,%a2@\+,%a3,%acc2
     d6c:	a41a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d2,%acc1
     d70:	a49a 3619      	macw %a1l,%d3l,>>,%a2@\+,%d2,%acc2
     d74:	ae5a 3609      	macw %a1l,%d3l,>>,%a2@\+,%sp,%acc1
     d78:	aeda 3619      	macw %a1l,%d3l,>>,%a2@\+,%sp,%acc2
     d7c:	a21a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d1,%acc1
     d80:	a29a 3639      	macw %a1l,%d3l,>>,%a2@\+&,%d1,%acc2
     d84:	a65a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%a3,%acc1
     d88:	a6da 3639      	macw %a1l,%d3l,>>,%a2@\+&,%a3,%acc2
     d8c:	a41a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d2,%acc1
     d90:	a49a 3639      	macw %a1l,%d3l,>>,%a2@\+&,%d2,%acc2
     d94:	ae5a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%sp,%acc1
     d98:	aeda 3639      	macw %a1l,%d3l,>>,%a2@\+&,%sp,%acc2
     d9c:	a22e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1,%acc1
     da2:	a2ae 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1,%acc2
     da8:	a66e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3,%acc1
     dae:	a6ee 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3,%acc2
     db4:	a42e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2,%acc1
     dba:	a4ae 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2,%acc2
     dc0:	ae6e 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp,%acc1
     dc6:	aeee 3619 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp,%acc2
     dcc:	a22e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1,%acc1
     dd2:	a2ae 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1,%acc2
     dd8:	a66e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3,%acc1
     dde:	a6ee 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3,%acc2
     de4:	a42e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2,%acc1
     dea:	a4ae 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2,%acc2
     df0:	ae6e 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp,%acc1
     df6:	aeee 3639 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp,%acc2
     dfc:	a221 3609      	macw %a1l,%d3l,>>,%a1@-,%d1,%acc1
     e00:	a2a1 3619      	macw %a1l,%d3l,>>,%a1@-,%d1,%acc2
     e04:	a661 3609      	macw %a1l,%d3l,>>,%a1@-,%a3,%acc1
     e08:	a6e1 3619      	macw %a1l,%d3l,>>,%a1@-,%a3,%acc2
     e0c:	a421 3609      	macw %a1l,%d3l,>>,%a1@-,%d2,%acc1
     e10:	a4a1 3619      	macw %a1l,%d3l,>>,%a1@-,%d2,%acc2
     e14:	ae61 3609      	macw %a1l,%d3l,>>,%a1@-,%sp,%acc1
     e18:	aee1 3619      	macw %a1l,%d3l,>>,%a1@-,%sp,%acc2
     e1c:	a221 3629      	macw %a1l,%d3l,>>,%a1@-&,%d1,%acc1
     e20:	a2a1 3639      	macw %a1l,%d3l,>>,%a1@-&,%d1,%acc2
     e24:	a661 3629      	macw %a1l,%d3l,>>,%a1@-&,%a3,%acc1
     e28:	a6e1 3639      	macw %a1l,%d3l,>>,%a1@-&,%a3,%acc2
     e2c:	a421 3629      	macw %a1l,%d3l,>>,%a1@-&,%d2,%acc1
     e30:	a4a1 3639      	macw %a1l,%d3l,>>,%a1@-&,%d2,%acc2
     e34:	ae61 3629      	macw %a1l,%d3l,>>,%a1@-&,%sp,%acc1
     e38:	aee1 3639      	macw %a1l,%d3l,>>,%a1@-&,%sp,%acc2
     e3c:	a213 f089      	macw %a1l,%a7u,%a3@,%d1,%acc1
     e40:	a293 f099      	macw %a1l,%a7u,%a3@,%d1,%acc2
     e44:	a653 f089      	macw %a1l,%a7u,%a3@,%a3,%acc1
     e48:	a6d3 f099      	macw %a1l,%a7u,%a3@,%a3,%acc2
     e4c:	a413 f089      	macw %a1l,%a7u,%a3@,%d2,%acc1
     e50:	a493 f099      	macw %a1l,%a7u,%a3@,%d2,%acc2
     e54:	ae53 f089      	macw %a1l,%a7u,%a3@,%sp,%acc1
     e58:	aed3 f099      	macw %a1l,%a7u,%a3@,%sp,%acc2
     e5c:	a213 f0a9      	macw %a1l,%a7u,%a3@&,%d1,%acc1
     e60:	a293 f0b9      	macw %a1l,%a7u,%a3@&,%d1,%acc2
     e64:	a653 f0a9      	macw %a1l,%a7u,%a3@&,%a3,%acc1
     e68:	a6d3 f0b9      	macw %a1l,%a7u,%a3@&,%a3,%acc2
     e6c:	a413 f0a9      	macw %a1l,%a7u,%a3@&,%d2,%acc1
     e70:	a493 f0b9      	macw %a1l,%a7u,%a3@&,%d2,%acc2
     e74:	ae53 f0a9      	macw %a1l,%a7u,%a3@&,%sp,%acc1
     e78:	aed3 f0b9      	macw %a1l,%a7u,%a3@&,%sp,%acc2
     e7c:	a21a f089      	macw %a1l,%a7u,%a2@\+,%d1,%acc1
     e80:	a29a f099      	macw %a1l,%a7u,%a2@\+,%d1,%acc2
     e84:	a65a f089      	macw %a1l,%a7u,%a2@\+,%a3,%acc1
     e88:	a6da f099      	macw %a1l,%a7u,%a2@\+,%a3,%acc2
     e8c:	a41a f089      	macw %a1l,%a7u,%a2@\+,%d2,%acc1
     e90:	a49a f099      	macw %a1l,%a7u,%a2@\+,%d2,%acc2
     e94:	ae5a f089      	macw %a1l,%a7u,%a2@\+,%sp,%acc1
     e98:	aeda f099      	macw %a1l,%a7u,%a2@\+,%sp,%acc2
     e9c:	a21a f0a9      	macw %a1l,%a7u,%a2@\+&,%d1,%acc1
     ea0:	a29a f0b9      	macw %a1l,%a7u,%a2@\+&,%d1,%acc2
     ea4:	a65a f0a9      	macw %a1l,%a7u,%a2@\+&,%a3,%acc1
     ea8:	a6da f0b9      	macw %a1l,%a7u,%a2@\+&,%a3,%acc2
     eac:	a41a f0a9      	macw %a1l,%a7u,%a2@\+&,%d2,%acc1
     eb0:	a49a f0b9      	macw %a1l,%a7u,%a2@\+&,%d2,%acc2
     eb4:	ae5a f0a9      	macw %a1l,%a7u,%a2@\+&,%sp,%acc1
     eb8:	aeda f0b9      	macw %a1l,%a7u,%a2@\+&,%sp,%acc2
     ebc:	a22e f089 000a 	macw %a1l,%a7u,%fp@\(10\),%d1,%acc1
     ec2:	a2ae f099 000a 	macw %a1l,%a7u,%fp@\(10\),%d1,%acc2
     ec8:	a66e f089 000a 	macw %a1l,%a7u,%fp@\(10\),%a3,%acc1
     ece:	a6ee f099 000a 	macw %a1l,%a7u,%fp@\(10\),%a3,%acc2
     ed4:	a42e f089 000a 	macw %a1l,%a7u,%fp@\(10\),%d2,%acc1
     eda:	a4ae f099 000a 	macw %a1l,%a7u,%fp@\(10\),%d2,%acc2
     ee0:	ae6e f089 000a 	macw %a1l,%a7u,%fp@\(10\),%sp,%acc1
     ee6:	aeee f099 000a 	macw %a1l,%a7u,%fp@\(10\),%sp,%acc2
     eec:	a22e f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d1,%acc1
     ef2:	a2ae f0b9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d1,%acc2
     ef8:	a66e f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%a3,%acc1
     efe:	a6ee f0b9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%a3,%acc2
     f04:	a42e f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d2,%acc1
     f0a:	a4ae f0b9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d2,%acc2
     f10:	ae6e f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%sp,%acc1
     f16:	aeee f0b9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%sp,%acc2
     f1c:	a221 f089      	macw %a1l,%a7u,%a1@-,%d1,%acc1
     f20:	a2a1 f099      	macw %a1l,%a7u,%a1@-,%d1,%acc2
     f24:	a661 f089      	macw %a1l,%a7u,%a1@-,%a3,%acc1
     f28:	a6e1 f099      	macw %a1l,%a7u,%a1@-,%a3,%acc2
     f2c:	a421 f089      	macw %a1l,%a7u,%a1@-,%d2,%acc1
     f30:	a4a1 f099      	macw %a1l,%a7u,%a1@-,%d2,%acc2
     f34:	ae61 f089      	macw %a1l,%a7u,%a1@-,%sp,%acc1
     f38:	aee1 f099      	macw %a1l,%a7u,%a1@-,%sp,%acc2
     f3c:	a221 f0a9      	macw %a1l,%a7u,%a1@-&,%d1,%acc1
     f40:	a2a1 f0b9      	macw %a1l,%a7u,%a1@-&,%d1,%acc2
     f44:	a661 f0a9      	macw %a1l,%a7u,%a1@-&,%a3,%acc1
     f48:	a6e1 f0b9      	macw %a1l,%a7u,%a1@-&,%a3,%acc2
     f4c:	a421 f0a9      	macw %a1l,%a7u,%a1@-&,%d2,%acc1
     f50:	a4a1 f0b9      	macw %a1l,%a7u,%a1@-&,%d2,%acc2
     f54:	ae61 f0a9      	macw %a1l,%a7u,%a1@-&,%sp,%acc1
     f58:	aee1 f0b9      	macw %a1l,%a7u,%a1@-&,%sp,%acc2
     f5c:	a213 f289      	macw %a1l,%a7u,<<,%a3@,%d1,%acc1
     f60:	a293 f299      	macw %a1l,%a7u,<<,%a3@,%d1,%acc2
     f64:	a653 f289      	macw %a1l,%a7u,<<,%a3@,%a3,%acc1
     f68:	a6d3 f299      	macw %a1l,%a7u,<<,%a3@,%a3,%acc2
     f6c:	a413 f289      	macw %a1l,%a7u,<<,%a3@,%d2,%acc1
     f70:	a493 f299      	macw %a1l,%a7u,<<,%a3@,%d2,%acc2
     f74:	ae53 f289      	macw %a1l,%a7u,<<,%a3@,%sp,%acc1
     f78:	aed3 f299      	macw %a1l,%a7u,<<,%a3@,%sp,%acc2
     f7c:	a213 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d1,%acc1
     f80:	a293 f2b9      	macw %a1l,%a7u,<<,%a3@&,%d1,%acc2
     f84:	a653 f2a9      	macw %a1l,%a7u,<<,%a3@&,%a3,%acc1
     f88:	a6d3 f2b9      	macw %a1l,%a7u,<<,%a3@&,%a3,%acc2
     f8c:	a413 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d2,%acc1
     f90:	a493 f2b9      	macw %a1l,%a7u,<<,%a3@&,%d2,%acc2
     f94:	ae53 f2a9      	macw %a1l,%a7u,<<,%a3@&,%sp,%acc1
     f98:	aed3 f2b9      	macw %a1l,%a7u,<<,%a3@&,%sp,%acc2
     f9c:	a21a f289      	macw %a1l,%a7u,<<,%a2@\+,%d1,%acc1
     fa0:	a29a f299      	macw %a1l,%a7u,<<,%a2@\+,%d1,%acc2
     fa4:	a65a f289      	macw %a1l,%a7u,<<,%a2@\+,%a3,%acc1
     fa8:	a6da f299      	macw %a1l,%a7u,<<,%a2@\+,%a3,%acc2
     fac:	a41a f289      	macw %a1l,%a7u,<<,%a2@\+,%d2,%acc1
     fb0:	a49a f299      	macw %a1l,%a7u,<<,%a2@\+,%d2,%acc2
     fb4:	ae5a f289      	macw %a1l,%a7u,<<,%a2@\+,%sp,%acc1
     fb8:	aeda f299      	macw %a1l,%a7u,<<,%a2@\+,%sp,%acc2
     fbc:	a21a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d1,%acc1
     fc0:	a29a f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%d1,%acc2
     fc4:	a65a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%a3,%acc1
     fc8:	a6da f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%a3,%acc2
     fcc:	a41a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d2,%acc1
     fd0:	a49a f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%d2,%acc2
     fd4:	ae5a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%sp,%acc1
     fd8:	aeda f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%sp,%acc2
     fdc:	a22e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1,%acc1
     fe2:	a2ae f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1,%acc2
     fe8:	a66e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3,%acc1
     fee:	a6ee f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3,%acc2
     ff4:	a42e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2,%acc1
     ffa:	a4ae f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2,%acc2
    1000:	ae6e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp,%acc1
    1006:	aeee f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp,%acc2
    100c:	a22e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    1012:	a2ae f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    1018:	a66e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    101e:	a6ee f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    1024:	a42e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    102a:	a4ae f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    1030:	ae6e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    1036:	aeee f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    103c:	a221 f289      	macw %a1l,%a7u,<<,%a1@-,%d1,%acc1
    1040:	a2a1 f299      	macw %a1l,%a7u,<<,%a1@-,%d1,%acc2
    1044:	a661 f289      	macw %a1l,%a7u,<<,%a1@-,%a3,%acc1
    1048:	a6e1 f299      	macw %a1l,%a7u,<<,%a1@-,%a3,%acc2
    104c:	a421 f289      	macw %a1l,%a7u,<<,%a1@-,%d2,%acc1
    1050:	a4a1 f299      	macw %a1l,%a7u,<<,%a1@-,%d2,%acc2
    1054:	ae61 f289      	macw %a1l,%a7u,<<,%a1@-,%sp,%acc1
    1058:	aee1 f299      	macw %a1l,%a7u,<<,%a1@-,%sp,%acc2
    105c:	a221 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d1,%acc1
    1060:	a2a1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%d1,%acc2
    1064:	a661 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%a3,%acc1
    1068:	a6e1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%a3,%acc2
    106c:	a421 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d2,%acc1
    1070:	a4a1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%d2,%acc2
    1074:	ae61 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%sp,%acc1
    1078:	aee1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%sp,%acc2
    107c:	a213 f689      	macw %a1l,%a7u,>>,%a3@,%d1,%acc1
    1080:	a293 f699      	macw %a1l,%a7u,>>,%a3@,%d1,%acc2
    1084:	a653 f689      	macw %a1l,%a7u,>>,%a3@,%a3,%acc1
    1088:	a6d3 f699      	macw %a1l,%a7u,>>,%a3@,%a3,%acc2
    108c:	a413 f689      	macw %a1l,%a7u,>>,%a3@,%d2,%acc1
    1090:	a493 f699      	macw %a1l,%a7u,>>,%a3@,%d2,%acc2
    1094:	ae53 f689      	macw %a1l,%a7u,>>,%a3@,%sp,%acc1
    1098:	aed3 f699      	macw %a1l,%a7u,>>,%a3@,%sp,%acc2
    109c:	a213 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d1,%acc1
    10a0:	a293 f6b9      	macw %a1l,%a7u,>>,%a3@&,%d1,%acc2
    10a4:	a653 f6a9      	macw %a1l,%a7u,>>,%a3@&,%a3,%acc1
    10a8:	a6d3 f6b9      	macw %a1l,%a7u,>>,%a3@&,%a3,%acc2
    10ac:	a413 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d2,%acc1
    10b0:	a493 f6b9      	macw %a1l,%a7u,>>,%a3@&,%d2,%acc2
    10b4:	ae53 f6a9      	macw %a1l,%a7u,>>,%a3@&,%sp,%acc1
    10b8:	aed3 f6b9      	macw %a1l,%a7u,>>,%a3@&,%sp,%acc2
    10bc:	a21a f689      	macw %a1l,%a7u,>>,%a2@\+,%d1,%acc1
    10c0:	a29a f699      	macw %a1l,%a7u,>>,%a2@\+,%d1,%acc2
    10c4:	a65a f689      	macw %a1l,%a7u,>>,%a2@\+,%a3,%acc1
    10c8:	a6da f699      	macw %a1l,%a7u,>>,%a2@\+,%a3,%acc2
    10cc:	a41a f689      	macw %a1l,%a7u,>>,%a2@\+,%d2,%acc1
    10d0:	a49a f699      	macw %a1l,%a7u,>>,%a2@\+,%d2,%acc2
    10d4:	ae5a f689      	macw %a1l,%a7u,>>,%a2@\+,%sp,%acc1
    10d8:	aeda f699      	macw %a1l,%a7u,>>,%a2@\+,%sp,%acc2
    10dc:	a21a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d1,%acc1
    10e0:	a29a f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%d1,%acc2
    10e4:	a65a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%a3,%acc1
    10e8:	a6da f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%a3,%acc2
    10ec:	a41a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d2,%acc1
    10f0:	a49a f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%d2,%acc2
    10f4:	ae5a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%sp,%acc1
    10f8:	aeda f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%sp,%acc2
    10fc:	a22e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1,%acc1
    1102:	a2ae f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1,%acc2
    1108:	a66e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3,%acc1
    110e:	a6ee f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3,%acc2
    1114:	a42e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2,%acc1
    111a:	a4ae f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2,%acc2
    1120:	ae6e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp,%acc1
    1126:	aeee f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp,%acc2
    112c:	a22e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    1132:	a2ae f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    1138:	a66e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    113e:	a6ee f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    1144:	a42e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    114a:	a4ae f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    1150:	ae6e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    1156:	aeee f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    115c:	a221 f689      	macw %a1l,%a7u,>>,%a1@-,%d1,%acc1
    1160:	a2a1 f699      	macw %a1l,%a7u,>>,%a1@-,%d1,%acc2
    1164:	a661 f689      	macw %a1l,%a7u,>>,%a1@-,%a3,%acc1
    1168:	a6e1 f699      	macw %a1l,%a7u,>>,%a1@-,%a3,%acc2
    116c:	a421 f689      	macw %a1l,%a7u,>>,%a1@-,%d2,%acc1
    1170:	a4a1 f699      	macw %a1l,%a7u,>>,%a1@-,%d2,%acc2
    1174:	ae61 f689      	macw %a1l,%a7u,>>,%a1@-,%sp,%acc1
    1178:	aee1 f699      	macw %a1l,%a7u,>>,%a1@-,%sp,%acc2
    117c:	a221 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d1,%acc1
    1180:	a2a1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%d1,%acc2
    1184:	a661 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%a3,%acc1
    1188:	a6e1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%a3,%acc2
    118c:	a421 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d2,%acc1
    1190:	a4a1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%d2,%acc2
    1194:	ae61 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%sp,%acc1
    1198:	aee1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%sp,%acc2
    119c:	a213 f289      	macw %a1l,%a7u,<<,%a3@,%d1,%acc1
    11a0:	a293 f299      	macw %a1l,%a7u,<<,%a3@,%d1,%acc2
    11a4:	a653 f289      	macw %a1l,%a7u,<<,%a3@,%a3,%acc1
    11a8:	a6d3 f299      	macw %a1l,%a7u,<<,%a3@,%a3,%acc2
    11ac:	a413 f289      	macw %a1l,%a7u,<<,%a3@,%d2,%acc1
    11b0:	a493 f299      	macw %a1l,%a7u,<<,%a3@,%d2,%acc2
    11b4:	ae53 f289      	macw %a1l,%a7u,<<,%a3@,%sp,%acc1
    11b8:	aed3 f299      	macw %a1l,%a7u,<<,%a3@,%sp,%acc2
    11bc:	a213 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d1,%acc1
    11c0:	a293 f2b9      	macw %a1l,%a7u,<<,%a3@&,%d1,%acc2
    11c4:	a653 f2a9      	macw %a1l,%a7u,<<,%a3@&,%a3,%acc1
    11c8:	a6d3 f2b9      	macw %a1l,%a7u,<<,%a3@&,%a3,%acc2
    11cc:	a413 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d2,%acc1
    11d0:	a493 f2b9      	macw %a1l,%a7u,<<,%a3@&,%d2,%acc2
    11d4:	ae53 f2a9      	macw %a1l,%a7u,<<,%a3@&,%sp,%acc1
    11d8:	aed3 f2b9      	macw %a1l,%a7u,<<,%a3@&,%sp,%acc2
    11dc:	a21a f289      	macw %a1l,%a7u,<<,%a2@\+,%d1,%acc1
    11e0:	a29a f299      	macw %a1l,%a7u,<<,%a2@\+,%d1,%acc2
    11e4:	a65a f289      	macw %a1l,%a7u,<<,%a2@\+,%a3,%acc1
    11e8:	a6da f299      	macw %a1l,%a7u,<<,%a2@\+,%a3,%acc2
    11ec:	a41a f289      	macw %a1l,%a7u,<<,%a2@\+,%d2,%acc1
    11f0:	a49a f299      	macw %a1l,%a7u,<<,%a2@\+,%d2,%acc2
    11f4:	ae5a f289      	macw %a1l,%a7u,<<,%a2@\+,%sp,%acc1
    11f8:	aeda f299      	macw %a1l,%a7u,<<,%a2@\+,%sp,%acc2
    11fc:	a21a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d1,%acc1
    1200:	a29a f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%d1,%acc2
    1204:	a65a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%a3,%acc1
    1208:	a6da f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%a3,%acc2
    120c:	a41a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d2,%acc1
    1210:	a49a f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%d2,%acc2
    1214:	ae5a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%sp,%acc1
    1218:	aeda f2b9      	macw %a1l,%a7u,<<,%a2@\+&,%sp,%acc2
    121c:	a22e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1,%acc1
    1222:	a2ae f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1,%acc2
    1228:	a66e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3,%acc1
    122e:	a6ee f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3,%acc2
    1234:	a42e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2,%acc1
    123a:	a4ae f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2,%acc2
    1240:	ae6e f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp,%acc1
    1246:	aeee f299 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp,%acc2
    124c:	a22e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    1252:	a2ae f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    1258:	a66e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    125e:	a6ee f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    1264:	a42e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    126a:	a4ae f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    1270:	ae6e f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    1276:	aeee f2b9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    127c:	a221 f289      	macw %a1l,%a7u,<<,%a1@-,%d1,%acc1
    1280:	a2a1 f299      	macw %a1l,%a7u,<<,%a1@-,%d1,%acc2
    1284:	a661 f289      	macw %a1l,%a7u,<<,%a1@-,%a3,%acc1
    1288:	a6e1 f299      	macw %a1l,%a7u,<<,%a1@-,%a3,%acc2
    128c:	a421 f289      	macw %a1l,%a7u,<<,%a1@-,%d2,%acc1
    1290:	a4a1 f299      	macw %a1l,%a7u,<<,%a1@-,%d2,%acc2
    1294:	ae61 f289      	macw %a1l,%a7u,<<,%a1@-,%sp,%acc1
    1298:	aee1 f299      	macw %a1l,%a7u,<<,%a1@-,%sp,%acc2
    129c:	a221 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d1,%acc1
    12a0:	a2a1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%d1,%acc2
    12a4:	a661 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%a3,%acc1
    12a8:	a6e1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%a3,%acc2
    12ac:	a421 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d2,%acc1
    12b0:	a4a1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%d2,%acc2
    12b4:	ae61 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%sp,%acc1
    12b8:	aee1 f2b9      	macw %a1l,%a7u,<<,%a1@-&,%sp,%acc2
    12bc:	a213 f689      	macw %a1l,%a7u,>>,%a3@,%d1,%acc1
    12c0:	a293 f699      	macw %a1l,%a7u,>>,%a3@,%d1,%acc2
    12c4:	a653 f689      	macw %a1l,%a7u,>>,%a3@,%a3,%acc1
    12c8:	a6d3 f699      	macw %a1l,%a7u,>>,%a3@,%a3,%acc2
    12cc:	a413 f689      	macw %a1l,%a7u,>>,%a3@,%d2,%acc1
    12d0:	a493 f699      	macw %a1l,%a7u,>>,%a3@,%d2,%acc2
    12d4:	ae53 f689      	macw %a1l,%a7u,>>,%a3@,%sp,%acc1
    12d8:	aed3 f699      	macw %a1l,%a7u,>>,%a3@,%sp,%acc2
    12dc:	a213 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d1,%acc1
    12e0:	a293 f6b9      	macw %a1l,%a7u,>>,%a3@&,%d1,%acc2
    12e4:	a653 f6a9      	macw %a1l,%a7u,>>,%a3@&,%a3,%acc1
    12e8:	a6d3 f6b9      	macw %a1l,%a7u,>>,%a3@&,%a3,%acc2
    12ec:	a413 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d2,%acc1
    12f0:	a493 f6b9      	macw %a1l,%a7u,>>,%a3@&,%d2,%acc2
    12f4:	ae53 f6a9      	macw %a1l,%a7u,>>,%a3@&,%sp,%acc1
    12f8:	aed3 f6b9      	macw %a1l,%a7u,>>,%a3@&,%sp,%acc2
    12fc:	a21a f689      	macw %a1l,%a7u,>>,%a2@\+,%d1,%acc1
    1300:	a29a f699      	macw %a1l,%a7u,>>,%a2@\+,%d1,%acc2
    1304:	a65a f689      	macw %a1l,%a7u,>>,%a2@\+,%a3,%acc1
    1308:	a6da f699      	macw %a1l,%a7u,>>,%a2@\+,%a3,%acc2
    130c:	a41a f689      	macw %a1l,%a7u,>>,%a2@\+,%d2,%acc1
    1310:	a49a f699      	macw %a1l,%a7u,>>,%a2@\+,%d2,%acc2
    1314:	ae5a f689      	macw %a1l,%a7u,>>,%a2@\+,%sp,%acc1
    1318:	aeda f699      	macw %a1l,%a7u,>>,%a2@\+,%sp,%acc2
    131c:	a21a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d1,%acc1
    1320:	a29a f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%d1,%acc2
    1324:	a65a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%a3,%acc1
    1328:	a6da f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%a3,%acc2
    132c:	a41a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d2,%acc1
    1330:	a49a f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%d2,%acc2
    1334:	ae5a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%sp,%acc1
    1338:	aeda f6b9      	macw %a1l,%a7u,>>,%a2@\+&,%sp,%acc2
    133c:	a22e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1,%acc1
    1342:	a2ae f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1,%acc2
    1348:	a66e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3,%acc1
    134e:	a6ee f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3,%acc2
    1354:	a42e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2,%acc1
    135a:	a4ae f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2,%acc2
    1360:	ae6e f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp,%acc1
    1366:	aeee f699 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp,%acc2
    136c:	a22e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    1372:	a2ae f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    1378:	a66e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    137e:	a6ee f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    1384:	a42e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    138a:	a4ae f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    1390:	ae6e f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    1396:	aeee f6b9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    139c:	a221 f689      	macw %a1l,%a7u,>>,%a1@-,%d1,%acc1
    13a0:	a2a1 f699      	macw %a1l,%a7u,>>,%a1@-,%d1,%acc2
    13a4:	a661 f689      	macw %a1l,%a7u,>>,%a1@-,%a3,%acc1
    13a8:	a6e1 f699      	macw %a1l,%a7u,>>,%a1@-,%a3,%acc2
    13ac:	a421 f689      	macw %a1l,%a7u,>>,%a1@-,%d2,%acc1
    13b0:	a4a1 f699      	macw %a1l,%a7u,>>,%a1@-,%d2,%acc2
    13b4:	ae61 f689      	macw %a1l,%a7u,>>,%a1@-,%sp,%acc1
    13b8:	aee1 f699      	macw %a1l,%a7u,>>,%a1@-,%sp,%acc2
    13bc:	a221 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d1,%acc1
    13c0:	a2a1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%d1,%acc2
    13c4:	a661 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%a3,%acc1
    13c8:	a6e1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%a3,%acc2
    13cc:	a421 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d2,%acc1
    13d0:	a4a1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%d2,%acc2
    13d4:	ae61 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%sp,%acc1
    13d8:	aee1 f6b9      	macw %a1l,%a7u,>>,%a1@-&,%sp,%acc2
    13dc:	a213 1009      	macw %a1l,%d1l,%a3@,%d1,%acc1
    13e0:	a293 1019      	macw %a1l,%d1l,%a3@,%d1,%acc2
    13e4:	a653 1009      	macw %a1l,%d1l,%a3@,%a3,%acc1
    13e8:	a6d3 1019      	macw %a1l,%d1l,%a3@,%a3,%acc2
    13ec:	a413 1009      	macw %a1l,%d1l,%a3@,%d2,%acc1
    13f0:	a493 1019      	macw %a1l,%d1l,%a3@,%d2,%acc2
    13f4:	ae53 1009      	macw %a1l,%d1l,%a3@,%sp,%acc1
    13f8:	aed3 1019      	macw %a1l,%d1l,%a3@,%sp,%acc2
    13fc:	a213 1029      	macw %a1l,%d1l,%a3@&,%d1,%acc1
    1400:	a293 1039      	macw %a1l,%d1l,%a3@&,%d1,%acc2
    1404:	a653 1029      	macw %a1l,%d1l,%a3@&,%a3,%acc1
    1408:	a6d3 1039      	macw %a1l,%d1l,%a3@&,%a3,%acc2
    140c:	a413 1029      	macw %a1l,%d1l,%a3@&,%d2,%acc1
    1410:	a493 1039      	macw %a1l,%d1l,%a3@&,%d2,%acc2
    1414:	ae53 1029      	macw %a1l,%d1l,%a3@&,%sp,%acc1
    1418:	aed3 1039      	macw %a1l,%d1l,%a3@&,%sp,%acc2
    141c:	a21a 1009      	macw %a1l,%d1l,%a2@\+,%d1,%acc1
    1420:	a29a 1019      	macw %a1l,%d1l,%a2@\+,%d1,%acc2
    1424:	a65a 1009      	macw %a1l,%d1l,%a2@\+,%a3,%acc1
    1428:	a6da 1019      	macw %a1l,%d1l,%a2@\+,%a3,%acc2
    142c:	a41a 1009      	macw %a1l,%d1l,%a2@\+,%d2,%acc1
    1430:	a49a 1019      	macw %a1l,%d1l,%a2@\+,%d2,%acc2
    1434:	ae5a 1009      	macw %a1l,%d1l,%a2@\+,%sp,%acc1
    1438:	aeda 1019      	macw %a1l,%d1l,%a2@\+,%sp,%acc2
    143c:	a21a 1029      	macw %a1l,%d1l,%a2@\+&,%d1,%acc1
    1440:	a29a 1039      	macw %a1l,%d1l,%a2@\+&,%d1,%acc2
    1444:	a65a 1029      	macw %a1l,%d1l,%a2@\+&,%a3,%acc1
    1448:	a6da 1039      	macw %a1l,%d1l,%a2@\+&,%a3,%acc2
    144c:	a41a 1029      	macw %a1l,%d1l,%a2@\+&,%d2,%acc1
    1450:	a49a 1039      	macw %a1l,%d1l,%a2@\+&,%d2,%acc2
    1454:	ae5a 1029      	macw %a1l,%d1l,%a2@\+&,%sp,%acc1
    1458:	aeda 1039      	macw %a1l,%d1l,%a2@\+&,%sp,%acc2
    145c:	a22e 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%d1,%acc1
    1462:	a2ae 1019 000a 	macw %a1l,%d1l,%fp@\(10\),%d1,%acc2
    1468:	a66e 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%a3,%acc1
    146e:	a6ee 1019 000a 	macw %a1l,%d1l,%fp@\(10\),%a3,%acc2
    1474:	a42e 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%d2,%acc1
    147a:	a4ae 1019 000a 	macw %a1l,%d1l,%fp@\(10\),%d2,%acc2
    1480:	ae6e 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%sp,%acc1
    1486:	aeee 1019 000a 	macw %a1l,%d1l,%fp@\(10\),%sp,%acc2
    148c:	a22e 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d1,%acc1
    1492:	a2ae 1039 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d1,%acc2
    1498:	a66e 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%a3,%acc1
    149e:	a6ee 1039 000a 	macw %a1l,%d1l,%fp@\(10\)&,%a3,%acc2
    14a4:	a42e 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d2,%acc1
    14aa:	a4ae 1039 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d2,%acc2
    14b0:	ae6e 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%sp,%acc1
    14b6:	aeee 1039 000a 	macw %a1l,%d1l,%fp@\(10\)&,%sp,%acc2
    14bc:	a221 1009      	macw %a1l,%d1l,%a1@-,%d1,%acc1
    14c0:	a2a1 1019      	macw %a1l,%d1l,%a1@-,%d1,%acc2
    14c4:	a661 1009      	macw %a1l,%d1l,%a1@-,%a3,%acc1
    14c8:	a6e1 1019      	macw %a1l,%d1l,%a1@-,%a3,%acc2
    14cc:	a421 1009      	macw %a1l,%d1l,%a1@-,%d2,%acc1
    14d0:	a4a1 1019      	macw %a1l,%d1l,%a1@-,%d2,%acc2
    14d4:	ae61 1009      	macw %a1l,%d1l,%a1@-,%sp,%acc1
    14d8:	aee1 1019      	macw %a1l,%d1l,%a1@-,%sp,%acc2
    14dc:	a221 1029      	macw %a1l,%d1l,%a1@-&,%d1,%acc1
    14e0:	a2a1 1039      	macw %a1l,%d1l,%a1@-&,%d1,%acc2
    14e4:	a661 1029      	macw %a1l,%d1l,%a1@-&,%a3,%acc1
    14e8:	a6e1 1039      	macw %a1l,%d1l,%a1@-&,%a3,%acc2
    14ec:	a421 1029      	macw %a1l,%d1l,%a1@-&,%d2,%acc1
    14f0:	a4a1 1039      	macw %a1l,%d1l,%a1@-&,%d2,%acc2
    14f4:	ae61 1029      	macw %a1l,%d1l,%a1@-&,%sp,%acc1
    14f8:	aee1 1039      	macw %a1l,%d1l,%a1@-&,%sp,%acc2
    14fc:	a213 1209      	macw %a1l,%d1l,<<,%a3@,%d1,%acc1
    1500:	a293 1219      	macw %a1l,%d1l,<<,%a3@,%d1,%acc2
    1504:	a653 1209      	macw %a1l,%d1l,<<,%a3@,%a3,%acc1
    1508:	a6d3 1219      	macw %a1l,%d1l,<<,%a3@,%a3,%acc2
    150c:	a413 1209      	macw %a1l,%d1l,<<,%a3@,%d2,%acc1
    1510:	a493 1219      	macw %a1l,%d1l,<<,%a3@,%d2,%acc2
    1514:	ae53 1209      	macw %a1l,%d1l,<<,%a3@,%sp,%acc1
    1518:	aed3 1219      	macw %a1l,%d1l,<<,%a3@,%sp,%acc2
    151c:	a213 1229      	macw %a1l,%d1l,<<,%a3@&,%d1,%acc1
    1520:	a293 1239      	macw %a1l,%d1l,<<,%a3@&,%d1,%acc2
    1524:	a653 1229      	macw %a1l,%d1l,<<,%a3@&,%a3,%acc1
    1528:	a6d3 1239      	macw %a1l,%d1l,<<,%a3@&,%a3,%acc2
    152c:	a413 1229      	macw %a1l,%d1l,<<,%a3@&,%d2,%acc1
    1530:	a493 1239      	macw %a1l,%d1l,<<,%a3@&,%d2,%acc2
    1534:	ae53 1229      	macw %a1l,%d1l,<<,%a3@&,%sp,%acc1
    1538:	aed3 1239      	macw %a1l,%d1l,<<,%a3@&,%sp,%acc2
    153c:	a21a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d1,%acc1
    1540:	a29a 1219      	macw %a1l,%d1l,<<,%a2@\+,%d1,%acc2
    1544:	a65a 1209      	macw %a1l,%d1l,<<,%a2@\+,%a3,%acc1
    1548:	a6da 1219      	macw %a1l,%d1l,<<,%a2@\+,%a3,%acc2
    154c:	a41a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d2,%acc1
    1550:	a49a 1219      	macw %a1l,%d1l,<<,%a2@\+,%d2,%acc2
    1554:	ae5a 1209      	macw %a1l,%d1l,<<,%a2@\+,%sp,%acc1
    1558:	aeda 1219      	macw %a1l,%d1l,<<,%a2@\+,%sp,%acc2
    155c:	a21a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d1,%acc1
    1560:	a29a 1239      	macw %a1l,%d1l,<<,%a2@\+&,%d1,%acc2
    1564:	a65a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%a3,%acc1
    1568:	a6da 1239      	macw %a1l,%d1l,<<,%a2@\+&,%a3,%acc2
    156c:	a41a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d2,%acc1
    1570:	a49a 1239      	macw %a1l,%d1l,<<,%a2@\+&,%d2,%acc2
    1574:	ae5a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%sp,%acc1
    1578:	aeda 1239      	macw %a1l,%d1l,<<,%a2@\+&,%sp,%acc2
    157c:	a22e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1,%acc1
    1582:	a2ae 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1,%acc2
    1588:	a66e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3,%acc1
    158e:	a6ee 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3,%acc2
    1594:	a42e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2,%acc1
    159a:	a4ae 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2,%acc2
    15a0:	ae6e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp,%acc1
    15a6:	aeee 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp,%acc2
    15ac:	a22e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    15b2:	a2ae 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    15b8:	a66e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    15be:	a6ee 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    15c4:	a42e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    15ca:	a4ae 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    15d0:	ae6e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    15d6:	aeee 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    15dc:	a221 1209      	macw %a1l,%d1l,<<,%a1@-,%d1,%acc1
    15e0:	a2a1 1219      	macw %a1l,%d1l,<<,%a1@-,%d1,%acc2
    15e4:	a661 1209      	macw %a1l,%d1l,<<,%a1@-,%a3,%acc1
    15e8:	a6e1 1219      	macw %a1l,%d1l,<<,%a1@-,%a3,%acc2
    15ec:	a421 1209      	macw %a1l,%d1l,<<,%a1@-,%d2,%acc1
    15f0:	a4a1 1219      	macw %a1l,%d1l,<<,%a1@-,%d2,%acc2
    15f4:	ae61 1209      	macw %a1l,%d1l,<<,%a1@-,%sp,%acc1
    15f8:	aee1 1219      	macw %a1l,%d1l,<<,%a1@-,%sp,%acc2
    15fc:	a221 1229      	macw %a1l,%d1l,<<,%a1@-&,%d1,%acc1
    1600:	a2a1 1239      	macw %a1l,%d1l,<<,%a1@-&,%d1,%acc2
    1604:	a661 1229      	macw %a1l,%d1l,<<,%a1@-&,%a3,%acc1
    1608:	a6e1 1239      	macw %a1l,%d1l,<<,%a1@-&,%a3,%acc2
    160c:	a421 1229      	macw %a1l,%d1l,<<,%a1@-&,%d2,%acc1
    1610:	a4a1 1239      	macw %a1l,%d1l,<<,%a1@-&,%d2,%acc2
    1614:	ae61 1229      	macw %a1l,%d1l,<<,%a1@-&,%sp,%acc1
    1618:	aee1 1239      	macw %a1l,%d1l,<<,%a1@-&,%sp,%acc2
    161c:	a213 1609      	macw %a1l,%d1l,>>,%a3@,%d1,%acc1
    1620:	a293 1619      	macw %a1l,%d1l,>>,%a3@,%d1,%acc2
    1624:	a653 1609      	macw %a1l,%d1l,>>,%a3@,%a3,%acc1
    1628:	a6d3 1619      	macw %a1l,%d1l,>>,%a3@,%a3,%acc2
    162c:	a413 1609      	macw %a1l,%d1l,>>,%a3@,%d2,%acc1
    1630:	a493 1619      	macw %a1l,%d1l,>>,%a3@,%d2,%acc2
    1634:	ae53 1609      	macw %a1l,%d1l,>>,%a3@,%sp,%acc1
    1638:	aed3 1619      	macw %a1l,%d1l,>>,%a3@,%sp,%acc2
    163c:	a213 1629      	macw %a1l,%d1l,>>,%a3@&,%d1,%acc1
    1640:	a293 1639      	macw %a1l,%d1l,>>,%a3@&,%d1,%acc2
    1644:	a653 1629      	macw %a1l,%d1l,>>,%a3@&,%a3,%acc1
    1648:	a6d3 1639      	macw %a1l,%d1l,>>,%a3@&,%a3,%acc2
    164c:	a413 1629      	macw %a1l,%d1l,>>,%a3@&,%d2,%acc1
    1650:	a493 1639      	macw %a1l,%d1l,>>,%a3@&,%d2,%acc2
    1654:	ae53 1629      	macw %a1l,%d1l,>>,%a3@&,%sp,%acc1
    1658:	aed3 1639      	macw %a1l,%d1l,>>,%a3@&,%sp,%acc2
    165c:	a21a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d1,%acc1
    1660:	a29a 1619      	macw %a1l,%d1l,>>,%a2@\+,%d1,%acc2
    1664:	a65a 1609      	macw %a1l,%d1l,>>,%a2@\+,%a3,%acc1
    1668:	a6da 1619      	macw %a1l,%d1l,>>,%a2@\+,%a3,%acc2
    166c:	a41a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d2,%acc1
    1670:	a49a 1619      	macw %a1l,%d1l,>>,%a2@\+,%d2,%acc2
    1674:	ae5a 1609      	macw %a1l,%d1l,>>,%a2@\+,%sp,%acc1
    1678:	aeda 1619      	macw %a1l,%d1l,>>,%a2@\+,%sp,%acc2
    167c:	a21a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d1,%acc1
    1680:	a29a 1639      	macw %a1l,%d1l,>>,%a2@\+&,%d1,%acc2
    1684:	a65a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%a3,%acc1
    1688:	a6da 1639      	macw %a1l,%d1l,>>,%a2@\+&,%a3,%acc2
    168c:	a41a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d2,%acc1
    1690:	a49a 1639      	macw %a1l,%d1l,>>,%a2@\+&,%d2,%acc2
    1694:	ae5a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%sp,%acc1
    1698:	aeda 1639      	macw %a1l,%d1l,>>,%a2@\+&,%sp,%acc2
    169c:	a22e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1,%acc1
    16a2:	a2ae 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1,%acc2
    16a8:	a66e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3,%acc1
    16ae:	a6ee 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3,%acc2
    16b4:	a42e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2,%acc1
    16ba:	a4ae 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2,%acc2
    16c0:	ae6e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp,%acc1
    16c6:	aeee 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp,%acc2
    16cc:	a22e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    16d2:	a2ae 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    16d8:	a66e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    16de:	a6ee 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    16e4:	a42e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    16ea:	a4ae 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    16f0:	ae6e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    16f6:	aeee 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    16fc:	a221 1609      	macw %a1l,%d1l,>>,%a1@-,%d1,%acc1
    1700:	a2a1 1619      	macw %a1l,%d1l,>>,%a1@-,%d1,%acc2
    1704:	a661 1609      	macw %a1l,%d1l,>>,%a1@-,%a3,%acc1
    1708:	a6e1 1619      	macw %a1l,%d1l,>>,%a1@-,%a3,%acc2
    170c:	a421 1609      	macw %a1l,%d1l,>>,%a1@-,%d2,%acc1
    1710:	a4a1 1619      	macw %a1l,%d1l,>>,%a1@-,%d2,%acc2
    1714:	ae61 1609      	macw %a1l,%d1l,>>,%a1@-,%sp,%acc1
    1718:	aee1 1619      	macw %a1l,%d1l,>>,%a1@-,%sp,%acc2
    171c:	a221 1629      	macw %a1l,%d1l,>>,%a1@-&,%d1,%acc1
    1720:	a2a1 1639      	macw %a1l,%d1l,>>,%a1@-&,%d1,%acc2
    1724:	a661 1629      	macw %a1l,%d1l,>>,%a1@-&,%a3,%acc1
    1728:	a6e1 1639      	macw %a1l,%d1l,>>,%a1@-&,%a3,%acc2
    172c:	a421 1629      	macw %a1l,%d1l,>>,%a1@-&,%d2,%acc1
    1730:	a4a1 1639      	macw %a1l,%d1l,>>,%a1@-&,%d2,%acc2
    1734:	ae61 1629      	macw %a1l,%d1l,>>,%a1@-&,%sp,%acc1
    1738:	aee1 1639      	macw %a1l,%d1l,>>,%a1@-&,%sp,%acc2
    173c:	a213 1209      	macw %a1l,%d1l,<<,%a3@,%d1,%acc1
    1740:	a293 1219      	macw %a1l,%d1l,<<,%a3@,%d1,%acc2
    1744:	a653 1209      	macw %a1l,%d1l,<<,%a3@,%a3,%acc1
    1748:	a6d3 1219      	macw %a1l,%d1l,<<,%a3@,%a3,%acc2
    174c:	a413 1209      	macw %a1l,%d1l,<<,%a3@,%d2,%acc1
    1750:	a493 1219      	macw %a1l,%d1l,<<,%a3@,%d2,%acc2
    1754:	ae53 1209      	macw %a1l,%d1l,<<,%a3@,%sp,%acc1
    1758:	aed3 1219      	macw %a1l,%d1l,<<,%a3@,%sp,%acc2
    175c:	a213 1229      	macw %a1l,%d1l,<<,%a3@&,%d1,%acc1
    1760:	a293 1239      	macw %a1l,%d1l,<<,%a3@&,%d1,%acc2
    1764:	a653 1229      	macw %a1l,%d1l,<<,%a3@&,%a3,%acc1
    1768:	a6d3 1239      	macw %a1l,%d1l,<<,%a3@&,%a3,%acc2
    176c:	a413 1229      	macw %a1l,%d1l,<<,%a3@&,%d2,%acc1
    1770:	a493 1239      	macw %a1l,%d1l,<<,%a3@&,%d2,%acc2
    1774:	ae53 1229      	macw %a1l,%d1l,<<,%a3@&,%sp,%acc1
    1778:	aed3 1239      	macw %a1l,%d1l,<<,%a3@&,%sp,%acc2
    177c:	a21a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d1,%acc1
    1780:	a29a 1219      	macw %a1l,%d1l,<<,%a2@\+,%d1,%acc2
    1784:	a65a 1209      	macw %a1l,%d1l,<<,%a2@\+,%a3,%acc1
    1788:	a6da 1219      	macw %a1l,%d1l,<<,%a2@\+,%a3,%acc2
    178c:	a41a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d2,%acc1
    1790:	a49a 1219      	macw %a1l,%d1l,<<,%a2@\+,%d2,%acc2
    1794:	ae5a 1209      	macw %a1l,%d1l,<<,%a2@\+,%sp,%acc1
    1798:	aeda 1219      	macw %a1l,%d1l,<<,%a2@\+,%sp,%acc2
    179c:	a21a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d1,%acc1
    17a0:	a29a 1239      	macw %a1l,%d1l,<<,%a2@\+&,%d1,%acc2
    17a4:	a65a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%a3,%acc1
    17a8:	a6da 1239      	macw %a1l,%d1l,<<,%a2@\+&,%a3,%acc2
    17ac:	a41a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d2,%acc1
    17b0:	a49a 1239      	macw %a1l,%d1l,<<,%a2@\+&,%d2,%acc2
    17b4:	ae5a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%sp,%acc1
    17b8:	aeda 1239      	macw %a1l,%d1l,<<,%a2@\+&,%sp,%acc2
    17bc:	a22e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1,%acc1
    17c2:	a2ae 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1,%acc2
    17c8:	a66e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3,%acc1
    17ce:	a6ee 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3,%acc2
    17d4:	a42e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2,%acc1
    17da:	a4ae 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2,%acc2
    17e0:	ae6e 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp,%acc1
    17e6:	aeee 1219 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp,%acc2
    17ec:	a22e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    17f2:	a2ae 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    17f8:	a66e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    17fe:	a6ee 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    1804:	a42e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    180a:	a4ae 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    1810:	ae6e 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    1816:	aeee 1239 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    181c:	a221 1209      	macw %a1l,%d1l,<<,%a1@-,%d1,%acc1
    1820:	a2a1 1219      	macw %a1l,%d1l,<<,%a1@-,%d1,%acc2
    1824:	a661 1209      	macw %a1l,%d1l,<<,%a1@-,%a3,%acc1
    1828:	a6e1 1219      	macw %a1l,%d1l,<<,%a1@-,%a3,%acc2
    182c:	a421 1209      	macw %a1l,%d1l,<<,%a1@-,%d2,%acc1
    1830:	a4a1 1219      	macw %a1l,%d1l,<<,%a1@-,%d2,%acc2
    1834:	ae61 1209      	macw %a1l,%d1l,<<,%a1@-,%sp,%acc1
    1838:	aee1 1219      	macw %a1l,%d1l,<<,%a1@-,%sp,%acc2
    183c:	a221 1229      	macw %a1l,%d1l,<<,%a1@-&,%d1,%acc1
    1840:	a2a1 1239      	macw %a1l,%d1l,<<,%a1@-&,%d1,%acc2
    1844:	a661 1229      	macw %a1l,%d1l,<<,%a1@-&,%a3,%acc1
    1848:	a6e1 1239      	macw %a1l,%d1l,<<,%a1@-&,%a3,%acc2
    184c:	a421 1229      	macw %a1l,%d1l,<<,%a1@-&,%d2,%acc1
    1850:	a4a1 1239      	macw %a1l,%d1l,<<,%a1@-&,%d2,%acc2
    1854:	ae61 1229      	macw %a1l,%d1l,<<,%a1@-&,%sp,%acc1
    1858:	aee1 1239      	macw %a1l,%d1l,<<,%a1@-&,%sp,%acc2
    185c:	a213 1609      	macw %a1l,%d1l,>>,%a3@,%d1,%acc1
    1860:	a293 1619      	macw %a1l,%d1l,>>,%a3@,%d1,%acc2
    1864:	a653 1609      	macw %a1l,%d1l,>>,%a3@,%a3,%acc1
    1868:	a6d3 1619      	macw %a1l,%d1l,>>,%a3@,%a3,%acc2
    186c:	a413 1609      	macw %a1l,%d1l,>>,%a3@,%d2,%acc1
    1870:	a493 1619      	macw %a1l,%d1l,>>,%a3@,%d2,%acc2
    1874:	ae53 1609      	macw %a1l,%d1l,>>,%a3@,%sp,%acc1
    1878:	aed3 1619      	macw %a1l,%d1l,>>,%a3@,%sp,%acc2
    187c:	a213 1629      	macw %a1l,%d1l,>>,%a3@&,%d1,%acc1
    1880:	a293 1639      	macw %a1l,%d1l,>>,%a3@&,%d1,%acc2
    1884:	a653 1629      	macw %a1l,%d1l,>>,%a3@&,%a3,%acc1
    1888:	a6d3 1639      	macw %a1l,%d1l,>>,%a3@&,%a3,%acc2
    188c:	a413 1629      	macw %a1l,%d1l,>>,%a3@&,%d2,%acc1
    1890:	a493 1639      	macw %a1l,%d1l,>>,%a3@&,%d2,%acc2
    1894:	ae53 1629      	macw %a1l,%d1l,>>,%a3@&,%sp,%acc1
    1898:	aed3 1639      	macw %a1l,%d1l,>>,%a3@&,%sp,%acc2
    189c:	a21a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d1,%acc1
    18a0:	a29a 1619      	macw %a1l,%d1l,>>,%a2@\+,%d1,%acc2
    18a4:	a65a 1609      	macw %a1l,%d1l,>>,%a2@\+,%a3,%acc1
    18a8:	a6da 1619      	macw %a1l,%d1l,>>,%a2@\+,%a3,%acc2
    18ac:	a41a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d2,%acc1
    18b0:	a49a 1619      	macw %a1l,%d1l,>>,%a2@\+,%d2,%acc2
    18b4:	ae5a 1609      	macw %a1l,%d1l,>>,%a2@\+,%sp,%acc1
    18b8:	aeda 1619      	macw %a1l,%d1l,>>,%a2@\+,%sp,%acc2
    18bc:	a21a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d1,%acc1
    18c0:	a29a 1639      	macw %a1l,%d1l,>>,%a2@\+&,%d1,%acc2
    18c4:	a65a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%a3,%acc1
    18c8:	a6da 1639      	macw %a1l,%d1l,>>,%a2@\+&,%a3,%acc2
    18cc:	a41a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d2,%acc1
    18d0:	a49a 1639      	macw %a1l,%d1l,>>,%a2@\+&,%d2,%acc2
    18d4:	ae5a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%sp,%acc1
    18d8:	aeda 1639      	macw %a1l,%d1l,>>,%a2@\+&,%sp,%acc2
    18dc:	a22e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1,%acc1
    18e2:	a2ae 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1,%acc2
    18e8:	a66e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3,%acc1
    18ee:	a6ee 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3,%acc2
    18f4:	a42e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2,%acc1
    18fa:	a4ae 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2,%acc2
    1900:	ae6e 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp,%acc1
    1906:	aeee 1619 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp,%acc2
    190c:	a22e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    1912:	a2ae 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    1918:	a66e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    191e:	a6ee 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    1924:	a42e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    192a:	a4ae 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    1930:	ae6e 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    1936:	aeee 1639 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    193c:	a221 1609      	macw %a1l,%d1l,>>,%a1@-,%d1,%acc1
    1940:	a2a1 1619      	macw %a1l,%d1l,>>,%a1@-,%d1,%acc2
    1944:	a661 1609      	macw %a1l,%d1l,>>,%a1@-,%a3,%acc1
    1948:	a6e1 1619      	macw %a1l,%d1l,>>,%a1@-,%a3,%acc2
    194c:	a421 1609      	macw %a1l,%d1l,>>,%a1@-,%d2,%acc1
    1950:	a4a1 1619      	macw %a1l,%d1l,>>,%a1@-,%d2,%acc2
    1954:	ae61 1609      	macw %a1l,%d1l,>>,%a1@-,%sp,%acc1
    1958:	aee1 1619      	macw %a1l,%d1l,>>,%a1@-,%sp,%acc2
    195c:	a221 1629      	macw %a1l,%d1l,>>,%a1@-&,%d1,%acc1
    1960:	a2a1 1639      	macw %a1l,%d1l,>>,%a1@-&,%d1,%acc2
    1964:	a661 1629      	macw %a1l,%d1l,>>,%a1@-&,%a3,%acc1
    1968:	a6e1 1639      	macw %a1l,%d1l,>>,%a1@-&,%a3,%acc2
    196c:	a421 1629      	macw %a1l,%d1l,>>,%a1@-&,%d2,%acc1
    1970:	a4a1 1639      	macw %a1l,%d1l,>>,%a1@-&,%d2,%acc2
    1974:	ae61 1629      	macw %a1l,%d1l,>>,%a1@-&,%sp,%acc1
    1978:	aee1 1639      	macw %a1l,%d1l,>>,%a1@-&,%sp,%acc2
    197c:	a213 a0c2      	macw %d2u,%a2u,%a3@,%d1,%acc1
    1980:	a293 a0d2      	macw %d2u,%a2u,%a3@,%d1,%acc2
    1984:	a653 a0c2      	macw %d2u,%a2u,%a3@,%a3,%acc1
    1988:	a6d3 a0d2      	macw %d2u,%a2u,%a3@,%a3,%acc2
    198c:	a413 a0c2      	macw %d2u,%a2u,%a3@,%d2,%acc1
    1990:	a493 a0d2      	macw %d2u,%a2u,%a3@,%d2,%acc2
    1994:	ae53 a0c2      	macw %d2u,%a2u,%a3@,%sp,%acc1
    1998:	aed3 a0d2      	macw %d2u,%a2u,%a3@,%sp,%acc2
    199c:	a213 a0e2      	macw %d2u,%a2u,%a3@&,%d1,%acc1
    19a0:	a293 a0f2      	macw %d2u,%a2u,%a3@&,%d1,%acc2
    19a4:	a653 a0e2      	macw %d2u,%a2u,%a3@&,%a3,%acc1
    19a8:	a6d3 a0f2      	macw %d2u,%a2u,%a3@&,%a3,%acc2
    19ac:	a413 a0e2      	macw %d2u,%a2u,%a3@&,%d2,%acc1
    19b0:	a493 a0f2      	macw %d2u,%a2u,%a3@&,%d2,%acc2
    19b4:	ae53 a0e2      	macw %d2u,%a2u,%a3@&,%sp,%acc1
    19b8:	aed3 a0f2      	macw %d2u,%a2u,%a3@&,%sp,%acc2
    19bc:	a21a a0c2      	macw %d2u,%a2u,%a2@\+,%d1,%acc1
    19c0:	a29a a0d2      	macw %d2u,%a2u,%a2@\+,%d1,%acc2
    19c4:	a65a a0c2      	macw %d2u,%a2u,%a2@\+,%a3,%acc1
    19c8:	a6da a0d2      	macw %d2u,%a2u,%a2@\+,%a3,%acc2
    19cc:	a41a a0c2      	macw %d2u,%a2u,%a2@\+,%d2,%acc1
    19d0:	a49a a0d2      	macw %d2u,%a2u,%a2@\+,%d2,%acc2
    19d4:	ae5a a0c2      	macw %d2u,%a2u,%a2@\+,%sp,%acc1
    19d8:	aeda a0d2      	macw %d2u,%a2u,%a2@\+,%sp,%acc2
    19dc:	a21a a0e2      	macw %d2u,%a2u,%a2@\+&,%d1,%acc1
    19e0:	a29a a0f2      	macw %d2u,%a2u,%a2@\+&,%d1,%acc2
    19e4:	a65a a0e2      	macw %d2u,%a2u,%a2@\+&,%a3,%acc1
    19e8:	a6da a0f2      	macw %d2u,%a2u,%a2@\+&,%a3,%acc2
    19ec:	a41a a0e2      	macw %d2u,%a2u,%a2@\+&,%d2,%acc1
    19f0:	a49a a0f2      	macw %d2u,%a2u,%a2@\+&,%d2,%acc2
    19f4:	ae5a a0e2      	macw %d2u,%a2u,%a2@\+&,%sp,%acc1
    19f8:	aeda a0f2      	macw %d2u,%a2u,%a2@\+&,%sp,%acc2
    19fc:	a22e a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%d1,%acc1
    1a02:	a2ae a0d2 000a 	macw %d2u,%a2u,%fp@\(10\),%d1,%acc2
    1a08:	a66e a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%a3,%acc1
    1a0e:	a6ee a0d2 000a 	macw %d2u,%a2u,%fp@\(10\),%a3,%acc2
    1a14:	a42e a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%d2,%acc1
    1a1a:	a4ae a0d2 000a 	macw %d2u,%a2u,%fp@\(10\),%d2,%acc2
    1a20:	ae6e a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%sp,%acc1
    1a26:	aeee a0d2 000a 	macw %d2u,%a2u,%fp@\(10\),%sp,%acc2
    1a2c:	a22e a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d1,%acc1
    1a32:	a2ae a0f2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d1,%acc2
    1a38:	a66e a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%a3,%acc1
    1a3e:	a6ee a0f2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%a3,%acc2
    1a44:	a42e a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d2,%acc1
    1a4a:	a4ae a0f2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d2,%acc2
    1a50:	ae6e a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%sp,%acc1
    1a56:	aeee a0f2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%sp,%acc2
    1a5c:	a221 a0c2      	macw %d2u,%a2u,%a1@-,%d1,%acc1
    1a60:	a2a1 a0d2      	macw %d2u,%a2u,%a1@-,%d1,%acc2
    1a64:	a661 a0c2      	macw %d2u,%a2u,%a1@-,%a3,%acc1
    1a68:	a6e1 a0d2      	macw %d2u,%a2u,%a1@-,%a3,%acc2
    1a6c:	a421 a0c2      	macw %d2u,%a2u,%a1@-,%d2,%acc1
    1a70:	a4a1 a0d2      	macw %d2u,%a2u,%a1@-,%d2,%acc2
    1a74:	ae61 a0c2      	macw %d2u,%a2u,%a1@-,%sp,%acc1
    1a78:	aee1 a0d2      	macw %d2u,%a2u,%a1@-,%sp,%acc2
    1a7c:	a221 a0e2      	macw %d2u,%a2u,%a1@-&,%d1,%acc1
    1a80:	a2a1 a0f2      	macw %d2u,%a2u,%a1@-&,%d1,%acc2
    1a84:	a661 a0e2      	macw %d2u,%a2u,%a1@-&,%a3,%acc1
    1a88:	a6e1 a0f2      	macw %d2u,%a2u,%a1@-&,%a3,%acc2
    1a8c:	a421 a0e2      	macw %d2u,%a2u,%a1@-&,%d2,%acc1
    1a90:	a4a1 a0f2      	macw %d2u,%a2u,%a1@-&,%d2,%acc2
    1a94:	ae61 a0e2      	macw %d2u,%a2u,%a1@-&,%sp,%acc1
    1a98:	aee1 a0f2      	macw %d2u,%a2u,%a1@-&,%sp,%acc2
    1a9c:	a213 a2c2      	macw %d2u,%a2u,<<,%a3@,%d1,%acc1
    1aa0:	a293 a2d2      	macw %d2u,%a2u,<<,%a3@,%d1,%acc2
    1aa4:	a653 a2c2      	macw %d2u,%a2u,<<,%a3@,%a3,%acc1
    1aa8:	a6d3 a2d2      	macw %d2u,%a2u,<<,%a3@,%a3,%acc2
    1aac:	a413 a2c2      	macw %d2u,%a2u,<<,%a3@,%d2,%acc1
    1ab0:	a493 a2d2      	macw %d2u,%a2u,<<,%a3@,%d2,%acc2
    1ab4:	ae53 a2c2      	macw %d2u,%a2u,<<,%a3@,%sp,%acc1
    1ab8:	aed3 a2d2      	macw %d2u,%a2u,<<,%a3@,%sp,%acc2
    1abc:	a213 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d1,%acc1
    1ac0:	a293 a2f2      	macw %d2u,%a2u,<<,%a3@&,%d1,%acc2
    1ac4:	a653 a2e2      	macw %d2u,%a2u,<<,%a3@&,%a3,%acc1
    1ac8:	a6d3 a2f2      	macw %d2u,%a2u,<<,%a3@&,%a3,%acc2
    1acc:	a413 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d2,%acc1
    1ad0:	a493 a2f2      	macw %d2u,%a2u,<<,%a3@&,%d2,%acc2
    1ad4:	ae53 a2e2      	macw %d2u,%a2u,<<,%a3@&,%sp,%acc1
    1ad8:	aed3 a2f2      	macw %d2u,%a2u,<<,%a3@&,%sp,%acc2
    1adc:	a21a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d1,%acc1
    1ae0:	a29a a2d2      	macw %d2u,%a2u,<<,%a2@\+,%d1,%acc2
    1ae4:	a65a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%a3,%acc1
    1ae8:	a6da a2d2      	macw %d2u,%a2u,<<,%a2@\+,%a3,%acc2
    1aec:	a41a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d2,%acc1
    1af0:	a49a a2d2      	macw %d2u,%a2u,<<,%a2@\+,%d2,%acc2
    1af4:	ae5a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%sp,%acc1
    1af8:	aeda a2d2      	macw %d2u,%a2u,<<,%a2@\+,%sp,%acc2
    1afc:	a21a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d1,%acc1
    1b00:	a29a a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%d1,%acc2
    1b04:	a65a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%a3,%acc1
    1b08:	a6da a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%a3,%acc2
    1b0c:	a41a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d2,%acc1
    1b10:	a49a a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%d2,%acc2
    1b14:	ae5a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%sp,%acc1
    1b18:	aeda a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%sp,%acc2
    1b1c:	a22e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1,%acc1
    1b22:	a2ae a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1,%acc2
    1b28:	a66e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3,%acc1
    1b2e:	a6ee a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3,%acc2
    1b34:	a42e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2,%acc1
    1b3a:	a4ae a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2,%acc2
    1b40:	ae6e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp,%acc1
    1b46:	aeee a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp,%acc2
    1b4c:	a22e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    1b52:	a2ae a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    1b58:	a66e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    1b5e:	a6ee a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    1b64:	a42e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    1b6a:	a4ae a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    1b70:	ae6e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    1b76:	aeee a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    1b7c:	a221 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d1,%acc1
    1b80:	a2a1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%d1,%acc2
    1b84:	a661 a2c2      	macw %d2u,%a2u,<<,%a1@-,%a3,%acc1
    1b88:	a6e1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%a3,%acc2
    1b8c:	a421 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d2,%acc1
    1b90:	a4a1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%d2,%acc2
    1b94:	ae61 a2c2      	macw %d2u,%a2u,<<,%a1@-,%sp,%acc1
    1b98:	aee1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%sp,%acc2
    1b9c:	a221 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d1,%acc1
    1ba0:	a2a1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%d1,%acc2
    1ba4:	a661 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%a3,%acc1
    1ba8:	a6e1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%a3,%acc2
    1bac:	a421 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d2,%acc1
    1bb0:	a4a1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%d2,%acc2
    1bb4:	ae61 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%sp,%acc1
    1bb8:	aee1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%sp,%acc2
    1bbc:	a213 a6c2      	macw %d2u,%a2u,>>,%a3@,%d1,%acc1
    1bc0:	a293 a6d2      	macw %d2u,%a2u,>>,%a3@,%d1,%acc2
    1bc4:	a653 a6c2      	macw %d2u,%a2u,>>,%a3@,%a3,%acc1
    1bc8:	a6d3 a6d2      	macw %d2u,%a2u,>>,%a3@,%a3,%acc2
    1bcc:	a413 a6c2      	macw %d2u,%a2u,>>,%a3@,%d2,%acc1
    1bd0:	a493 a6d2      	macw %d2u,%a2u,>>,%a3@,%d2,%acc2
    1bd4:	ae53 a6c2      	macw %d2u,%a2u,>>,%a3@,%sp,%acc1
    1bd8:	aed3 a6d2      	macw %d2u,%a2u,>>,%a3@,%sp,%acc2
    1bdc:	a213 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d1,%acc1
    1be0:	a293 a6f2      	macw %d2u,%a2u,>>,%a3@&,%d1,%acc2
    1be4:	a653 a6e2      	macw %d2u,%a2u,>>,%a3@&,%a3,%acc1
    1be8:	a6d3 a6f2      	macw %d2u,%a2u,>>,%a3@&,%a3,%acc2
    1bec:	a413 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d2,%acc1
    1bf0:	a493 a6f2      	macw %d2u,%a2u,>>,%a3@&,%d2,%acc2
    1bf4:	ae53 a6e2      	macw %d2u,%a2u,>>,%a3@&,%sp,%acc1
    1bf8:	aed3 a6f2      	macw %d2u,%a2u,>>,%a3@&,%sp,%acc2
    1bfc:	a21a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d1,%acc1
    1c00:	a29a a6d2      	macw %d2u,%a2u,>>,%a2@\+,%d1,%acc2
    1c04:	a65a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%a3,%acc1
    1c08:	a6da a6d2      	macw %d2u,%a2u,>>,%a2@\+,%a3,%acc2
    1c0c:	a41a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d2,%acc1
    1c10:	a49a a6d2      	macw %d2u,%a2u,>>,%a2@\+,%d2,%acc2
    1c14:	ae5a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%sp,%acc1
    1c18:	aeda a6d2      	macw %d2u,%a2u,>>,%a2@\+,%sp,%acc2
    1c1c:	a21a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d1,%acc1
    1c20:	a29a a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%d1,%acc2
    1c24:	a65a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%a3,%acc1
    1c28:	a6da a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%a3,%acc2
    1c2c:	a41a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d2,%acc1
    1c30:	a49a a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%d2,%acc2
    1c34:	ae5a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%sp,%acc1
    1c38:	aeda a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%sp,%acc2
    1c3c:	a22e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1,%acc1
    1c42:	a2ae a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1,%acc2
    1c48:	a66e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3,%acc1
    1c4e:	a6ee a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3,%acc2
    1c54:	a42e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2,%acc1
    1c5a:	a4ae a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2,%acc2
    1c60:	ae6e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp,%acc1
    1c66:	aeee a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp,%acc2
    1c6c:	a22e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    1c72:	a2ae a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    1c78:	a66e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    1c7e:	a6ee a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    1c84:	a42e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    1c8a:	a4ae a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    1c90:	ae6e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    1c96:	aeee a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    1c9c:	a221 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d1,%acc1
    1ca0:	a2a1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%d1,%acc2
    1ca4:	a661 a6c2      	macw %d2u,%a2u,>>,%a1@-,%a3,%acc1
    1ca8:	a6e1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%a3,%acc2
    1cac:	a421 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d2,%acc1
    1cb0:	a4a1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%d2,%acc2
    1cb4:	ae61 a6c2      	macw %d2u,%a2u,>>,%a1@-,%sp,%acc1
    1cb8:	aee1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%sp,%acc2
    1cbc:	a221 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d1,%acc1
    1cc0:	a2a1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%d1,%acc2
    1cc4:	a661 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%a3,%acc1
    1cc8:	a6e1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%a3,%acc2
    1ccc:	a421 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d2,%acc1
    1cd0:	a4a1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%d2,%acc2
    1cd4:	ae61 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%sp,%acc1
    1cd8:	aee1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%sp,%acc2
    1cdc:	a213 a2c2      	macw %d2u,%a2u,<<,%a3@,%d1,%acc1
    1ce0:	a293 a2d2      	macw %d2u,%a2u,<<,%a3@,%d1,%acc2
    1ce4:	a653 a2c2      	macw %d2u,%a2u,<<,%a3@,%a3,%acc1
    1ce8:	a6d3 a2d2      	macw %d2u,%a2u,<<,%a3@,%a3,%acc2
    1cec:	a413 a2c2      	macw %d2u,%a2u,<<,%a3@,%d2,%acc1
    1cf0:	a493 a2d2      	macw %d2u,%a2u,<<,%a3@,%d2,%acc2
    1cf4:	ae53 a2c2      	macw %d2u,%a2u,<<,%a3@,%sp,%acc1
    1cf8:	aed3 a2d2      	macw %d2u,%a2u,<<,%a3@,%sp,%acc2
    1cfc:	a213 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d1,%acc1
    1d00:	a293 a2f2      	macw %d2u,%a2u,<<,%a3@&,%d1,%acc2
    1d04:	a653 a2e2      	macw %d2u,%a2u,<<,%a3@&,%a3,%acc1
    1d08:	a6d3 a2f2      	macw %d2u,%a2u,<<,%a3@&,%a3,%acc2
    1d0c:	a413 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d2,%acc1
    1d10:	a493 a2f2      	macw %d2u,%a2u,<<,%a3@&,%d2,%acc2
    1d14:	ae53 a2e2      	macw %d2u,%a2u,<<,%a3@&,%sp,%acc1
    1d18:	aed3 a2f2      	macw %d2u,%a2u,<<,%a3@&,%sp,%acc2
    1d1c:	a21a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d1,%acc1
    1d20:	a29a a2d2      	macw %d2u,%a2u,<<,%a2@\+,%d1,%acc2
    1d24:	a65a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%a3,%acc1
    1d28:	a6da a2d2      	macw %d2u,%a2u,<<,%a2@\+,%a3,%acc2
    1d2c:	a41a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d2,%acc1
    1d30:	a49a a2d2      	macw %d2u,%a2u,<<,%a2@\+,%d2,%acc2
    1d34:	ae5a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%sp,%acc1
    1d38:	aeda a2d2      	macw %d2u,%a2u,<<,%a2@\+,%sp,%acc2
    1d3c:	a21a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d1,%acc1
    1d40:	a29a a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%d1,%acc2
    1d44:	a65a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%a3,%acc1
    1d48:	a6da a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%a3,%acc2
    1d4c:	a41a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d2,%acc1
    1d50:	a49a a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%d2,%acc2
    1d54:	ae5a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%sp,%acc1
    1d58:	aeda a2f2      	macw %d2u,%a2u,<<,%a2@\+&,%sp,%acc2
    1d5c:	a22e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1,%acc1
    1d62:	a2ae a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1,%acc2
    1d68:	a66e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3,%acc1
    1d6e:	a6ee a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3,%acc2
    1d74:	a42e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2,%acc1
    1d7a:	a4ae a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2,%acc2
    1d80:	ae6e a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp,%acc1
    1d86:	aeee a2d2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp,%acc2
    1d8c:	a22e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    1d92:	a2ae a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    1d98:	a66e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    1d9e:	a6ee a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    1da4:	a42e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    1daa:	a4ae a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    1db0:	ae6e a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    1db6:	aeee a2f2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    1dbc:	a221 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d1,%acc1
    1dc0:	a2a1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%d1,%acc2
    1dc4:	a661 a2c2      	macw %d2u,%a2u,<<,%a1@-,%a3,%acc1
    1dc8:	a6e1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%a3,%acc2
    1dcc:	a421 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d2,%acc1
    1dd0:	a4a1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%d2,%acc2
    1dd4:	ae61 a2c2      	macw %d2u,%a2u,<<,%a1@-,%sp,%acc1
    1dd8:	aee1 a2d2      	macw %d2u,%a2u,<<,%a1@-,%sp,%acc2
    1ddc:	a221 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d1,%acc1
    1de0:	a2a1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%d1,%acc2
    1de4:	a661 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%a3,%acc1
    1de8:	a6e1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%a3,%acc2
    1dec:	a421 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d2,%acc1
    1df0:	a4a1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%d2,%acc2
    1df4:	ae61 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%sp,%acc1
    1df8:	aee1 a2f2      	macw %d2u,%a2u,<<,%a1@-&,%sp,%acc2
    1dfc:	a213 a6c2      	macw %d2u,%a2u,>>,%a3@,%d1,%acc1
    1e00:	a293 a6d2      	macw %d2u,%a2u,>>,%a3@,%d1,%acc2
    1e04:	a653 a6c2      	macw %d2u,%a2u,>>,%a3@,%a3,%acc1
    1e08:	a6d3 a6d2      	macw %d2u,%a2u,>>,%a3@,%a3,%acc2
    1e0c:	a413 a6c2      	macw %d2u,%a2u,>>,%a3@,%d2,%acc1
    1e10:	a493 a6d2      	macw %d2u,%a2u,>>,%a3@,%d2,%acc2
    1e14:	ae53 a6c2      	macw %d2u,%a2u,>>,%a3@,%sp,%acc1
    1e18:	aed3 a6d2      	macw %d2u,%a2u,>>,%a3@,%sp,%acc2
    1e1c:	a213 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d1,%acc1
    1e20:	a293 a6f2      	macw %d2u,%a2u,>>,%a3@&,%d1,%acc2
    1e24:	a653 a6e2      	macw %d2u,%a2u,>>,%a3@&,%a3,%acc1
    1e28:	a6d3 a6f2      	macw %d2u,%a2u,>>,%a3@&,%a3,%acc2
    1e2c:	a413 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d2,%acc1
    1e30:	a493 a6f2      	macw %d2u,%a2u,>>,%a3@&,%d2,%acc2
    1e34:	ae53 a6e2      	macw %d2u,%a2u,>>,%a3@&,%sp,%acc1
    1e38:	aed3 a6f2      	macw %d2u,%a2u,>>,%a3@&,%sp,%acc2
    1e3c:	a21a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d1,%acc1
    1e40:	a29a a6d2      	macw %d2u,%a2u,>>,%a2@\+,%d1,%acc2
    1e44:	a65a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%a3,%acc1
    1e48:	a6da a6d2      	macw %d2u,%a2u,>>,%a2@\+,%a3,%acc2
    1e4c:	a41a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d2,%acc1
    1e50:	a49a a6d2      	macw %d2u,%a2u,>>,%a2@\+,%d2,%acc2
    1e54:	ae5a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%sp,%acc1
    1e58:	aeda a6d2      	macw %d2u,%a2u,>>,%a2@\+,%sp,%acc2
    1e5c:	a21a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d1,%acc1
    1e60:	a29a a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%d1,%acc2
    1e64:	a65a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%a3,%acc1
    1e68:	a6da a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%a3,%acc2
    1e6c:	a41a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d2,%acc1
    1e70:	a49a a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%d2,%acc2
    1e74:	ae5a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%sp,%acc1
    1e78:	aeda a6f2      	macw %d2u,%a2u,>>,%a2@\+&,%sp,%acc2
    1e7c:	a22e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1,%acc1
    1e82:	a2ae a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1,%acc2
    1e88:	a66e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3,%acc1
    1e8e:	a6ee a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3,%acc2
    1e94:	a42e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2,%acc1
    1e9a:	a4ae a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2,%acc2
    1ea0:	ae6e a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp,%acc1
    1ea6:	aeee a6d2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp,%acc2
    1eac:	a22e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    1eb2:	a2ae a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    1eb8:	a66e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    1ebe:	a6ee a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    1ec4:	a42e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    1eca:	a4ae a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    1ed0:	ae6e a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    1ed6:	aeee a6f2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    1edc:	a221 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d1,%acc1
    1ee0:	a2a1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%d1,%acc2
    1ee4:	a661 a6c2      	macw %d2u,%a2u,>>,%a1@-,%a3,%acc1
    1ee8:	a6e1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%a3,%acc2
    1eec:	a421 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d2,%acc1
    1ef0:	a4a1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%d2,%acc2
    1ef4:	ae61 a6c2      	macw %d2u,%a2u,>>,%a1@-,%sp,%acc1
    1ef8:	aee1 a6d2      	macw %d2u,%a2u,>>,%a1@-,%sp,%acc2
    1efc:	a221 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d1,%acc1
    1f00:	a2a1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%d1,%acc2
    1f04:	a661 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%a3,%acc1
    1f08:	a6e1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%a3,%acc2
    1f0c:	a421 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d2,%acc1
    1f10:	a4a1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%d2,%acc2
    1f14:	ae61 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%sp,%acc1
    1f18:	aee1 a6f2      	macw %d2u,%a2u,>>,%a1@-&,%sp,%acc2
    1f1c:	a213 3042      	macw %d2u,%d3l,%a3@,%d1,%acc1
    1f20:	a293 3052      	macw %d2u,%d3l,%a3@,%d1,%acc2
    1f24:	a653 3042      	macw %d2u,%d3l,%a3@,%a3,%acc1
    1f28:	a6d3 3052      	macw %d2u,%d3l,%a3@,%a3,%acc2
    1f2c:	a413 3042      	macw %d2u,%d3l,%a3@,%d2,%acc1
    1f30:	a493 3052      	macw %d2u,%d3l,%a3@,%d2,%acc2
    1f34:	ae53 3042      	macw %d2u,%d3l,%a3@,%sp,%acc1
    1f38:	aed3 3052      	macw %d2u,%d3l,%a3@,%sp,%acc2
    1f3c:	a213 3062      	macw %d2u,%d3l,%a3@&,%d1,%acc1
    1f40:	a293 3072      	macw %d2u,%d3l,%a3@&,%d1,%acc2
    1f44:	a653 3062      	macw %d2u,%d3l,%a3@&,%a3,%acc1
    1f48:	a6d3 3072      	macw %d2u,%d3l,%a3@&,%a3,%acc2
    1f4c:	a413 3062      	macw %d2u,%d3l,%a3@&,%d2,%acc1
    1f50:	a493 3072      	macw %d2u,%d3l,%a3@&,%d2,%acc2
    1f54:	ae53 3062      	macw %d2u,%d3l,%a3@&,%sp,%acc1
    1f58:	aed3 3072      	macw %d2u,%d3l,%a3@&,%sp,%acc2
    1f5c:	a21a 3042      	macw %d2u,%d3l,%a2@\+,%d1,%acc1
    1f60:	a29a 3052      	macw %d2u,%d3l,%a2@\+,%d1,%acc2
    1f64:	a65a 3042      	macw %d2u,%d3l,%a2@\+,%a3,%acc1
    1f68:	a6da 3052      	macw %d2u,%d3l,%a2@\+,%a3,%acc2
    1f6c:	a41a 3042      	macw %d2u,%d3l,%a2@\+,%d2,%acc1
    1f70:	a49a 3052      	macw %d2u,%d3l,%a2@\+,%d2,%acc2
    1f74:	ae5a 3042      	macw %d2u,%d3l,%a2@\+,%sp,%acc1
    1f78:	aeda 3052      	macw %d2u,%d3l,%a2@\+,%sp,%acc2
    1f7c:	a21a 3062      	macw %d2u,%d3l,%a2@\+&,%d1,%acc1
    1f80:	a29a 3072      	macw %d2u,%d3l,%a2@\+&,%d1,%acc2
    1f84:	a65a 3062      	macw %d2u,%d3l,%a2@\+&,%a3,%acc1
    1f88:	a6da 3072      	macw %d2u,%d3l,%a2@\+&,%a3,%acc2
    1f8c:	a41a 3062      	macw %d2u,%d3l,%a2@\+&,%d2,%acc1
    1f90:	a49a 3072      	macw %d2u,%d3l,%a2@\+&,%d2,%acc2
    1f94:	ae5a 3062      	macw %d2u,%d3l,%a2@\+&,%sp,%acc1
    1f98:	aeda 3072      	macw %d2u,%d3l,%a2@\+&,%sp,%acc2
    1f9c:	a22e 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%d1,%acc1
    1fa2:	a2ae 3052 000a 	macw %d2u,%d3l,%fp@\(10\),%d1,%acc2
    1fa8:	a66e 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%a3,%acc1
    1fae:	a6ee 3052 000a 	macw %d2u,%d3l,%fp@\(10\),%a3,%acc2
    1fb4:	a42e 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%d2,%acc1
    1fba:	a4ae 3052 000a 	macw %d2u,%d3l,%fp@\(10\),%d2,%acc2
    1fc0:	ae6e 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%sp,%acc1
    1fc6:	aeee 3052 000a 	macw %d2u,%d3l,%fp@\(10\),%sp,%acc2
    1fcc:	a22e 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d1,%acc1
    1fd2:	a2ae 3072 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d1,%acc2
    1fd8:	a66e 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%a3,%acc1
    1fde:	a6ee 3072 000a 	macw %d2u,%d3l,%fp@\(10\)&,%a3,%acc2
    1fe4:	a42e 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d2,%acc1
    1fea:	a4ae 3072 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d2,%acc2
    1ff0:	ae6e 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%sp,%acc1
    1ff6:	aeee 3072 000a 	macw %d2u,%d3l,%fp@\(10\)&,%sp,%acc2
    1ffc:	a221 3042      	macw %d2u,%d3l,%a1@-,%d1,%acc1
    2000:	a2a1 3052      	macw %d2u,%d3l,%a1@-,%d1,%acc2
    2004:	a661 3042      	macw %d2u,%d3l,%a1@-,%a3,%acc1
    2008:	a6e1 3052      	macw %d2u,%d3l,%a1@-,%a3,%acc2
    200c:	a421 3042      	macw %d2u,%d3l,%a1@-,%d2,%acc1
    2010:	a4a1 3052      	macw %d2u,%d3l,%a1@-,%d2,%acc2
    2014:	ae61 3042      	macw %d2u,%d3l,%a1@-,%sp,%acc1
    2018:	aee1 3052      	macw %d2u,%d3l,%a1@-,%sp,%acc2
    201c:	a221 3062      	macw %d2u,%d3l,%a1@-&,%d1,%acc1
    2020:	a2a1 3072      	macw %d2u,%d3l,%a1@-&,%d1,%acc2
    2024:	a661 3062      	macw %d2u,%d3l,%a1@-&,%a3,%acc1
    2028:	a6e1 3072      	macw %d2u,%d3l,%a1@-&,%a3,%acc2
    202c:	a421 3062      	macw %d2u,%d3l,%a1@-&,%d2,%acc1
    2030:	a4a1 3072      	macw %d2u,%d3l,%a1@-&,%d2,%acc2
    2034:	ae61 3062      	macw %d2u,%d3l,%a1@-&,%sp,%acc1
    2038:	aee1 3072      	macw %d2u,%d3l,%a1@-&,%sp,%acc2
    203c:	a213 3242      	macw %d2u,%d3l,<<,%a3@,%d1,%acc1
    2040:	a293 3252      	macw %d2u,%d3l,<<,%a3@,%d1,%acc2
    2044:	a653 3242      	macw %d2u,%d3l,<<,%a3@,%a3,%acc1
    2048:	a6d3 3252      	macw %d2u,%d3l,<<,%a3@,%a3,%acc2
    204c:	a413 3242      	macw %d2u,%d3l,<<,%a3@,%d2,%acc1
    2050:	a493 3252      	macw %d2u,%d3l,<<,%a3@,%d2,%acc2
    2054:	ae53 3242      	macw %d2u,%d3l,<<,%a3@,%sp,%acc1
    2058:	aed3 3252      	macw %d2u,%d3l,<<,%a3@,%sp,%acc2
    205c:	a213 3262      	macw %d2u,%d3l,<<,%a3@&,%d1,%acc1
    2060:	a293 3272      	macw %d2u,%d3l,<<,%a3@&,%d1,%acc2
    2064:	a653 3262      	macw %d2u,%d3l,<<,%a3@&,%a3,%acc1
    2068:	a6d3 3272      	macw %d2u,%d3l,<<,%a3@&,%a3,%acc2
    206c:	a413 3262      	macw %d2u,%d3l,<<,%a3@&,%d2,%acc1
    2070:	a493 3272      	macw %d2u,%d3l,<<,%a3@&,%d2,%acc2
    2074:	ae53 3262      	macw %d2u,%d3l,<<,%a3@&,%sp,%acc1
    2078:	aed3 3272      	macw %d2u,%d3l,<<,%a3@&,%sp,%acc2
    207c:	a21a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d1,%acc1
    2080:	a29a 3252      	macw %d2u,%d3l,<<,%a2@\+,%d1,%acc2
    2084:	a65a 3242      	macw %d2u,%d3l,<<,%a2@\+,%a3,%acc1
    2088:	a6da 3252      	macw %d2u,%d3l,<<,%a2@\+,%a3,%acc2
    208c:	a41a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d2,%acc1
    2090:	a49a 3252      	macw %d2u,%d3l,<<,%a2@\+,%d2,%acc2
    2094:	ae5a 3242      	macw %d2u,%d3l,<<,%a2@\+,%sp,%acc1
    2098:	aeda 3252      	macw %d2u,%d3l,<<,%a2@\+,%sp,%acc2
    209c:	a21a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d1,%acc1
    20a0:	a29a 3272      	macw %d2u,%d3l,<<,%a2@\+&,%d1,%acc2
    20a4:	a65a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%a3,%acc1
    20a8:	a6da 3272      	macw %d2u,%d3l,<<,%a2@\+&,%a3,%acc2
    20ac:	a41a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d2,%acc1
    20b0:	a49a 3272      	macw %d2u,%d3l,<<,%a2@\+&,%d2,%acc2
    20b4:	ae5a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%sp,%acc1
    20b8:	aeda 3272      	macw %d2u,%d3l,<<,%a2@\+&,%sp,%acc2
    20bc:	a22e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1,%acc1
    20c2:	a2ae 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1,%acc2
    20c8:	a66e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3,%acc1
    20ce:	a6ee 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3,%acc2
    20d4:	a42e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2,%acc1
    20da:	a4ae 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2,%acc2
    20e0:	ae6e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp,%acc1
    20e6:	aeee 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp,%acc2
    20ec:	a22e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    20f2:	a2ae 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    20f8:	a66e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    20fe:	a6ee 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    2104:	a42e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    210a:	a4ae 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    2110:	ae6e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    2116:	aeee 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    211c:	a221 3242      	macw %d2u,%d3l,<<,%a1@-,%d1,%acc1
    2120:	a2a1 3252      	macw %d2u,%d3l,<<,%a1@-,%d1,%acc2
    2124:	a661 3242      	macw %d2u,%d3l,<<,%a1@-,%a3,%acc1
    2128:	a6e1 3252      	macw %d2u,%d3l,<<,%a1@-,%a3,%acc2
    212c:	a421 3242      	macw %d2u,%d3l,<<,%a1@-,%d2,%acc1
    2130:	a4a1 3252      	macw %d2u,%d3l,<<,%a1@-,%d2,%acc2
    2134:	ae61 3242      	macw %d2u,%d3l,<<,%a1@-,%sp,%acc1
    2138:	aee1 3252      	macw %d2u,%d3l,<<,%a1@-,%sp,%acc2
    213c:	a221 3262      	macw %d2u,%d3l,<<,%a1@-&,%d1,%acc1
    2140:	a2a1 3272      	macw %d2u,%d3l,<<,%a1@-&,%d1,%acc2
    2144:	a661 3262      	macw %d2u,%d3l,<<,%a1@-&,%a3,%acc1
    2148:	a6e1 3272      	macw %d2u,%d3l,<<,%a1@-&,%a3,%acc2
    214c:	a421 3262      	macw %d2u,%d3l,<<,%a1@-&,%d2,%acc1
    2150:	a4a1 3272      	macw %d2u,%d3l,<<,%a1@-&,%d2,%acc2
    2154:	ae61 3262      	macw %d2u,%d3l,<<,%a1@-&,%sp,%acc1
    2158:	aee1 3272      	macw %d2u,%d3l,<<,%a1@-&,%sp,%acc2
    215c:	a213 3642      	macw %d2u,%d3l,>>,%a3@,%d1,%acc1
    2160:	a293 3652      	macw %d2u,%d3l,>>,%a3@,%d1,%acc2
    2164:	a653 3642      	macw %d2u,%d3l,>>,%a3@,%a3,%acc1
    2168:	a6d3 3652      	macw %d2u,%d3l,>>,%a3@,%a3,%acc2
    216c:	a413 3642      	macw %d2u,%d3l,>>,%a3@,%d2,%acc1
    2170:	a493 3652      	macw %d2u,%d3l,>>,%a3@,%d2,%acc2
    2174:	ae53 3642      	macw %d2u,%d3l,>>,%a3@,%sp,%acc1
    2178:	aed3 3652      	macw %d2u,%d3l,>>,%a3@,%sp,%acc2
    217c:	a213 3662      	macw %d2u,%d3l,>>,%a3@&,%d1,%acc1
    2180:	a293 3672      	macw %d2u,%d3l,>>,%a3@&,%d1,%acc2
    2184:	a653 3662      	macw %d2u,%d3l,>>,%a3@&,%a3,%acc1
    2188:	a6d3 3672      	macw %d2u,%d3l,>>,%a3@&,%a3,%acc2
    218c:	a413 3662      	macw %d2u,%d3l,>>,%a3@&,%d2,%acc1
    2190:	a493 3672      	macw %d2u,%d3l,>>,%a3@&,%d2,%acc2
    2194:	ae53 3662      	macw %d2u,%d3l,>>,%a3@&,%sp,%acc1
    2198:	aed3 3672      	macw %d2u,%d3l,>>,%a3@&,%sp,%acc2
    219c:	a21a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d1,%acc1
    21a0:	a29a 3652      	macw %d2u,%d3l,>>,%a2@\+,%d1,%acc2
    21a4:	a65a 3642      	macw %d2u,%d3l,>>,%a2@\+,%a3,%acc1
    21a8:	a6da 3652      	macw %d2u,%d3l,>>,%a2@\+,%a3,%acc2
    21ac:	a41a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d2,%acc1
    21b0:	a49a 3652      	macw %d2u,%d3l,>>,%a2@\+,%d2,%acc2
    21b4:	ae5a 3642      	macw %d2u,%d3l,>>,%a2@\+,%sp,%acc1
    21b8:	aeda 3652      	macw %d2u,%d3l,>>,%a2@\+,%sp,%acc2
    21bc:	a21a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d1,%acc1
    21c0:	a29a 3672      	macw %d2u,%d3l,>>,%a2@\+&,%d1,%acc2
    21c4:	a65a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%a3,%acc1
    21c8:	a6da 3672      	macw %d2u,%d3l,>>,%a2@\+&,%a3,%acc2
    21cc:	a41a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d2,%acc1
    21d0:	a49a 3672      	macw %d2u,%d3l,>>,%a2@\+&,%d2,%acc2
    21d4:	ae5a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%sp,%acc1
    21d8:	aeda 3672      	macw %d2u,%d3l,>>,%a2@\+&,%sp,%acc2
    21dc:	a22e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1,%acc1
    21e2:	a2ae 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1,%acc2
    21e8:	a66e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3,%acc1
    21ee:	a6ee 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3,%acc2
    21f4:	a42e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2,%acc1
    21fa:	a4ae 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2,%acc2
    2200:	ae6e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp,%acc1
    2206:	aeee 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp,%acc2
    220c:	a22e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    2212:	a2ae 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    2218:	a66e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    221e:	a6ee 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    2224:	a42e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    222a:	a4ae 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    2230:	ae6e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    2236:	aeee 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    223c:	a221 3642      	macw %d2u,%d3l,>>,%a1@-,%d1,%acc1
    2240:	a2a1 3652      	macw %d2u,%d3l,>>,%a1@-,%d1,%acc2
    2244:	a661 3642      	macw %d2u,%d3l,>>,%a1@-,%a3,%acc1
    2248:	a6e1 3652      	macw %d2u,%d3l,>>,%a1@-,%a3,%acc2
    224c:	a421 3642      	macw %d2u,%d3l,>>,%a1@-,%d2,%acc1
    2250:	a4a1 3652      	macw %d2u,%d3l,>>,%a1@-,%d2,%acc2
    2254:	ae61 3642      	macw %d2u,%d3l,>>,%a1@-,%sp,%acc1
    2258:	aee1 3652      	macw %d2u,%d3l,>>,%a1@-,%sp,%acc2
    225c:	a221 3662      	macw %d2u,%d3l,>>,%a1@-&,%d1,%acc1
    2260:	a2a1 3672      	macw %d2u,%d3l,>>,%a1@-&,%d1,%acc2
    2264:	a661 3662      	macw %d2u,%d3l,>>,%a1@-&,%a3,%acc1
    2268:	a6e1 3672      	macw %d2u,%d3l,>>,%a1@-&,%a3,%acc2
    226c:	a421 3662      	macw %d2u,%d3l,>>,%a1@-&,%d2,%acc1
    2270:	a4a1 3672      	macw %d2u,%d3l,>>,%a1@-&,%d2,%acc2
    2274:	ae61 3662      	macw %d2u,%d3l,>>,%a1@-&,%sp,%acc1
    2278:	aee1 3672      	macw %d2u,%d3l,>>,%a1@-&,%sp,%acc2
    227c:	a213 3242      	macw %d2u,%d3l,<<,%a3@,%d1,%acc1
    2280:	a293 3252      	macw %d2u,%d3l,<<,%a3@,%d1,%acc2
    2284:	a653 3242      	macw %d2u,%d3l,<<,%a3@,%a3,%acc1
    2288:	a6d3 3252      	macw %d2u,%d3l,<<,%a3@,%a3,%acc2
    228c:	a413 3242      	macw %d2u,%d3l,<<,%a3@,%d2,%acc1
    2290:	a493 3252      	macw %d2u,%d3l,<<,%a3@,%d2,%acc2
    2294:	ae53 3242      	macw %d2u,%d3l,<<,%a3@,%sp,%acc1
    2298:	aed3 3252      	macw %d2u,%d3l,<<,%a3@,%sp,%acc2
    229c:	a213 3262      	macw %d2u,%d3l,<<,%a3@&,%d1,%acc1
    22a0:	a293 3272      	macw %d2u,%d3l,<<,%a3@&,%d1,%acc2
    22a4:	a653 3262      	macw %d2u,%d3l,<<,%a3@&,%a3,%acc1
    22a8:	a6d3 3272      	macw %d2u,%d3l,<<,%a3@&,%a3,%acc2
    22ac:	a413 3262      	macw %d2u,%d3l,<<,%a3@&,%d2,%acc1
    22b0:	a493 3272      	macw %d2u,%d3l,<<,%a3@&,%d2,%acc2
    22b4:	ae53 3262      	macw %d2u,%d3l,<<,%a3@&,%sp,%acc1
    22b8:	aed3 3272      	macw %d2u,%d3l,<<,%a3@&,%sp,%acc2
    22bc:	a21a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d1,%acc1
    22c0:	a29a 3252      	macw %d2u,%d3l,<<,%a2@\+,%d1,%acc2
    22c4:	a65a 3242      	macw %d2u,%d3l,<<,%a2@\+,%a3,%acc1
    22c8:	a6da 3252      	macw %d2u,%d3l,<<,%a2@\+,%a3,%acc2
    22cc:	a41a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d2,%acc1
    22d0:	a49a 3252      	macw %d2u,%d3l,<<,%a2@\+,%d2,%acc2
    22d4:	ae5a 3242      	macw %d2u,%d3l,<<,%a2@\+,%sp,%acc1
    22d8:	aeda 3252      	macw %d2u,%d3l,<<,%a2@\+,%sp,%acc2
    22dc:	a21a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d1,%acc1
    22e0:	a29a 3272      	macw %d2u,%d3l,<<,%a2@\+&,%d1,%acc2
    22e4:	a65a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%a3,%acc1
    22e8:	a6da 3272      	macw %d2u,%d3l,<<,%a2@\+&,%a3,%acc2
    22ec:	a41a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d2,%acc1
    22f0:	a49a 3272      	macw %d2u,%d3l,<<,%a2@\+&,%d2,%acc2
    22f4:	ae5a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%sp,%acc1
    22f8:	aeda 3272      	macw %d2u,%d3l,<<,%a2@\+&,%sp,%acc2
    22fc:	a22e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1,%acc1
    2302:	a2ae 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1,%acc2
    2308:	a66e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3,%acc1
    230e:	a6ee 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3,%acc2
    2314:	a42e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2,%acc1
    231a:	a4ae 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2,%acc2
    2320:	ae6e 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp,%acc1
    2326:	aeee 3252 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp,%acc2
    232c:	a22e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    2332:	a2ae 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    2338:	a66e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    233e:	a6ee 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    2344:	a42e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    234a:	a4ae 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    2350:	ae6e 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    2356:	aeee 3272 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    235c:	a221 3242      	macw %d2u,%d3l,<<,%a1@-,%d1,%acc1
    2360:	a2a1 3252      	macw %d2u,%d3l,<<,%a1@-,%d1,%acc2
    2364:	a661 3242      	macw %d2u,%d3l,<<,%a1@-,%a3,%acc1
    2368:	a6e1 3252      	macw %d2u,%d3l,<<,%a1@-,%a3,%acc2
    236c:	a421 3242      	macw %d2u,%d3l,<<,%a1@-,%d2,%acc1
    2370:	a4a1 3252      	macw %d2u,%d3l,<<,%a1@-,%d2,%acc2
    2374:	ae61 3242      	macw %d2u,%d3l,<<,%a1@-,%sp,%acc1
    2378:	aee1 3252      	macw %d2u,%d3l,<<,%a1@-,%sp,%acc2
    237c:	a221 3262      	macw %d2u,%d3l,<<,%a1@-&,%d1,%acc1
    2380:	a2a1 3272      	macw %d2u,%d3l,<<,%a1@-&,%d1,%acc2
    2384:	a661 3262      	macw %d2u,%d3l,<<,%a1@-&,%a3,%acc1
    2388:	a6e1 3272      	macw %d2u,%d3l,<<,%a1@-&,%a3,%acc2
    238c:	a421 3262      	macw %d2u,%d3l,<<,%a1@-&,%d2,%acc1
    2390:	a4a1 3272      	macw %d2u,%d3l,<<,%a1@-&,%d2,%acc2
    2394:	ae61 3262      	macw %d2u,%d3l,<<,%a1@-&,%sp,%acc1
    2398:	aee1 3272      	macw %d2u,%d3l,<<,%a1@-&,%sp,%acc2
    239c:	a213 3642      	macw %d2u,%d3l,>>,%a3@,%d1,%acc1
    23a0:	a293 3652      	macw %d2u,%d3l,>>,%a3@,%d1,%acc2
    23a4:	a653 3642      	macw %d2u,%d3l,>>,%a3@,%a3,%acc1
    23a8:	a6d3 3652      	macw %d2u,%d3l,>>,%a3@,%a3,%acc2
    23ac:	a413 3642      	macw %d2u,%d3l,>>,%a3@,%d2,%acc1
    23b0:	a493 3652      	macw %d2u,%d3l,>>,%a3@,%d2,%acc2
    23b4:	ae53 3642      	macw %d2u,%d3l,>>,%a3@,%sp,%acc1
    23b8:	aed3 3652      	macw %d2u,%d3l,>>,%a3@,%sp,%acc2
    23bc:	a213 3662      	macw %d2u,%d3l,>>,%a3@&,%d1,%acc1
    23c0:	a293 3672      	macw %d2u,%d3l,>>,%a3@&,%d1,%acc2
    23c4:	a653 3662      	macw %d2u,%d3l,>>,%a3@&,%a3,%acc1
    23c8:	a6d3 3672      	macw %d2u,%d3l,>>,%a3@&,%a3,%acc2
    23cc:	a413 3662      	macw %d2u,%d3l,>>,%a3@&,%d2,%acc1
    23d0:	a493 3672      	macw %d2u,%d3l,>>,%a3@&,%d2,%acc2
    23d4:	ae53 3662      	macw %d2u,%d3l,>>,%a3@&,%sp,%acc1
    23d8:	aed3 3672      	macw %d2u,%d3l,>>,%a3@&,%sp,%acc2
    23dc:	a21a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d1,%acc1
    23e0:	a29a 3652      	macw %d2u,%d3l,>>,%a2@\+,%d1,%acc2
    23e4:	a65a 3642      	macw %d2u,%d3l,>>,%a2@\+,%a3,%acc1
    23e8:	a6da 3652      	macw %d2u,%d3l,>>,%a2@\+,%a3,%acc2
    23ec:	a41a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d2,%acc1
    23f0:	a49a 3652      	macw %d2u,%d3l,>>,%a2@\+,%d2,%acc2
    23f4:	ae5a 3642      	macw %d2u,%d3l,>>,%a2@\+,%sp,%acc1
    23f8:	aeda 3652      	macw %d2u,%d3l,>>,%a2@\+,%sp,%acc2
    23fc:	a21a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d1,%acc1
    2400:	a29a 3672      	macw %d2u,%d3l,>>,%a2@\+&,%d1,%acc2
    2404:	a65a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%a3,%acc1
    2408:	a6da 3672      	macw %d2u,%d3l,>>,%a2@\+&,%a3,%acc2
    240c:	a41a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d2,%acc1
    2410:	a49a 3672      	macw %d2u,%d3l,>>,%a2@\+&,%d2,%acc2
    2414:	ae5a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%sp,%acc1
    2418:	aeda 3672      	macw %d2u,%d3l,>>,%a2@\+&,%sp,%acc2
    241c:	a22e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1,%acc1
    2422:	a2ae 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1,%acc2
    2428:	a66e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3,%acc1
    242e:	a6ee 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3,%acc2
    2434:	a42e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2,%acc1
    243a:	a4ae 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2,%acc2
    2440:	ae6e 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp,%acc1
    2446:	aeee 3652 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp,%acc2
    244c:	a22e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    2452:	a2ae 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    2458:	a66e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    245e:	a6ee 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    2464:	a42e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    246a:	a4ae 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    2470:	ae6e 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    2476:	aeee 3672 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    247c:	a221 3642      	macw %d2u,%d3l,>>,%a1@-,%d1,%acc1
    2480:	a2a1 3652      	macw %d2u,%d3l,>>,%a1@-,%d1,%acc2
    2484:	a661 3642      	macw %d2u,%d3l,>>,%a1@-,%a3,%acc1
    2488:	a6e1 3652      	macw %d2u,%d3l,>>,%a1@-,%a3,%acc2
    248c:	a421 3642      	macw %d2u,%d3l,>>,%a1@-,%d2,%acc1
    2490:	a4a1 3652      	macw %d2u,%d3l,>>,%a1@-,%d2,%acc2
    2494:	ae61 3642      	macw %d2u,%d3l,>>,%a1@-,%sp,%acc1
    2498:	aee1 3652      	macw %d2u,%d3l,>>,%a1@-,%sp,%acc2
    249c:	a221 3662      	macw %d2u,%d3l,>>,%a1@-&,%d1,%acc1
    24a0:	a2a1 3672      	macw %d2u,%d3l,>>,%a1@-&,%d1,%acc2
    24a4:	a661 3662      	macw %d2u,%d3l,>>,%a1@-&,%a3,%acc1
    24a8:	a6e1 3672      	macw %d2u,%d3l,>>,%a1@-&,%a3,%acc2
    24ac:	a421 3662      	macw %d2u,%d3l,>>,%a1@-&,%d2,%acc1
    24b0:	a4a1 3672      	macw %d2u,%d3l,>>,%a1@-&,%d2,%acc2
    24b4:	ae61 3662      	macw %d2u,%d3l,>>,%a1@-&,%sp,%acc1
    24b8:	aee1 3672      	macw %d2u,%d3l,>>,%a1@-&,%sp,%acc2
    24bc:	a213 f0c2      	macw %d2u,%a7u,%a3@,%d1,%acc1
    24c0:	a293 f0d2      	macw %d2u,%a7u,%a3@,%d1,%acc2
    24c4:	a653 f0c2      	macw %d2u,%a7u,%a3@,%a3,%acc1
    24c8:	a6d3 f0d2      	macw %d2u,%a7u,%a3@,%a3,%acc2
    24cc:	a413 f0c2      	macw %d2u,%a7u,%a3@,%d2,%acc1
    24d0:	a493 f0d2      	macw %d2u,%a7u,%a3@,%d2,%acc2
    24d4:	ae53 f0c2      	macw %d2u,%a7u,%a3@,%sp,%acc1
    24d8:	aed3 f0d2      	macw %d2u,%a7u,%a3@,%sp,%acc2
    24dc:	a213 f0e2      	macw %d2u,%a7u,%a3@&,%d1,%acc1
    24e0:	a293 f0f2      	macw %d2u,%a7u,%a3@&,%d1,%acc2
    24e4:	a653 f0e2      	macw %d2u,%a7u,%a3@&,%a3,%acc1
    24e8:	a6d3 f0f2      	macw %d2u,%a7u,%a3@&,%a3,%acc2
    24ec:	a413 f0e2      	macw %d2u,%a7u,%a3@&,%d2,%acc1
    24f0:	a493 f0f2      	macw %d2u,%a7u,%a3@&,%d2,%acc2
    24f4:	ae53 f0e2      	macw %d2u,%a7u,%a3@&,%sp,%acc1
    24f8:	aed3 f0f2      	macw %d2u,%a7u,%a3@&,%sp,%acc2
    24fc:	a21a f0c2      	macw %d2u,%a7u,%a2@\+,%d1,%acc1
    2500:	a29a f0d2      	macw %d2u,%a7u,%a2@\+,%d1,%acc2
    2504:	a65a f0c2      	macw %d2u,%a7u,%a2@\+,%a3,%acc1
    2508:	a6da f0d2      	macw %d2u,%a7u,%a2@\+,%a3,%acc2
    250c:	a41a f0c2      	macw %d2u,%a7u,%a2@\+,%d2,%acc1
    2510:	a49a f0d2      	macw %d2u,%a7u,%a2@\+,%d2,%acc2
    2514:	ae5a f0c2      	macw %d2u,%a7u,%a2@\+,%sp,%acc1
    2518:	aeda f0d2      	macw %d2u,%a7u,%a2@\+,%sp,%acc2
    251c:	a21a f0e2      	macw %d2u,%a7u,%a2@\+&,%d1,%acc1
    2520:	a29a f0f2      	macw %d2u,%a7u,%a2@\+&,%d1,%acc2
    2524:	a65a f0e2      	macw %d2u,%a7u,%a2@\+&,%a3,%acc1
    2528:	a6da f0f2      	macw %d2u,%a7u,%a2@\+&,%a3,%acc2
    252c:	a41a f0e2      	macw %d2u,%a7u,%a2@\+&,%d2,%acc1
    2530:	a49a f0f2      	macw %d2u,%a7u,%a2@\+&,%d2,%acc2
    2534:	ae5a f0e2      	macw %d2u,%a7u,%a2@\+&,%sp,%acc1
    2538:	aeda f0f2      	macw %d2u,%a7u,%a2@\+&,%sp,%acc2
    253c:	a22e f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%d1,%acc1
    2542:	a2ae f0d2 000a 	macw %d2u,%a7u,%fp@\(10\),%d1,%acc2
    2548:	a66e f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%a3,%acc1
    254e:	a6ee f0d2 000a 	macw %d2u,%a7u,%fp@\(10\),%a3,%acc2
    2554:	a42e f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%d2,%acc1
    255a:	a4ae f0d2 000a 	macw %d2u,%a7u,%fp@\(10\),%d2,%acc2
    2560:	ae6e f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%sp,%acc1
    2566:	aeee f0d2 000a 	macw %d2u,%a7u,%fp@\(10\),%sp,%acc2
    256c:	a22e f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d1,%acc1
    2572:	a2ae f0f2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d1,%acc2
    2578:	a66e f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%a3,%acc1
    257e:	a6ee f0f2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%a3,%acc2
    2584:	a42e f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d2,%acc1
    258a:	a4ae f0f2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d2,%acc2
    2590:	ae6e f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%sp,%acc1
    2596:	aeee f0f2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%sp,%acc2
    259c:	a221 f0c2      	macw %d2u,%a7u,%a1@-,%d1,%acc1
    25a0:	a2a1 f0d2      	macw %d2u,%a7u,%a1@-,%d1,%acc2
    25a4:	a661 f0c2      	macw %d2u,%a7u,%a1@-,%a3,%acc1
    25a8:	a6e1 f0d2      	macw %d2u,%a7u,%a1@-,%a3,%acc2
    25ac:	a421 f0c2      	macw %d2u,%a7u,%a1@-,%d2,%acc1
    25b0:	a4a1 f0d2      	macw %d2u,%a7u,%a1@-,%d2,%acc2
    25b4:	ae61 f0c2      	macw %d2u,%a7u,%a1@-,%sp,%acc1
    25b8:	aee1 f0d2      	macw %d2u,%a7u,%a1@-,%sp,%acc2
    25bc:	a221 f0e2      	macw %d2u,%a7u,%a1@-&,%d1,%acc1
    25c0:	a2a1 f0f2      	macw %d2u,%a7u,%a1@-&,%d1,%acc2
    25c4:	a661 f0e2      	macw %d2u,%a7u,%a1@-&,%a3,%acc1
    25c8:	a6e1 f0f2      	macw %d2u,%a7u,%a1@-&,%a3,%acc2
    25cc:	a421 f0e2      	macw %d2u,%a7u,%a1@-&,%d2,%acc1
    25d0:	a4a1 f0f2      	macw %d2u,%a7u,%a1@-&,%d2,%acc2
    25d4:	ae61 f0e2      	macw %d2u,%a7u,%a1@-&,%sp,%acc1
    25d8:	aee1 f0f2      	macw %d2u,%a7u,%a1@-&,%sp,%acc2
    25dc:	a213 f2c2      	macw %d2u,%a7u,<<,%a3@,%d1,%acc1
    25e0:	a293 f2d2      	macw %d2u,%a7u,<<,%a3@,%d1,%acc2
    25e4:	a653 f2c2      	macw %d2u,%a7u,<<,%a3@,%a3,%acc1
    25e8:	a6d3 f2d2      	macw %d2u,%a7u,<<,%a3@,%a3,%acc2
    25ec:	a413 f2c2      	macw %d2u,%a7u,<<,%a3@,%d2,%acc1
    25f0:	a493 f2d2      	macw %d2u,%a7u,<<,%a3@,%d2,%acc2
    25f4:	ae53 f2c2      	macw %d2u,%a7u,<<,%a3@,%sp,%acc1
    25f8:	aed3 f2d2      	macw %d2u,%a7u,<<,%a3@,%sp,%acc2
    25fc:	a213 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d1,%acc1
    2600:	a293 f2f2      	macw %d2u,%a7u,<<,%a3@&,%d1,%acc2
    2604:	a653 f2e2      	macw %d2u,%a7u,<<,%a3@&,%a3,%acc1
    2608:	a6d3 f2f2      	macw %d2u,%a7u,<<,%a3@&,%a3,%acc2
    260c:	a413 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d2,%acc1
    2610:	a493 f2f2      	macw %d2u,%a7u,<<,%a3@&,%d2,%acc2
    2614:	ae53 f2e2      	macw %d2u,%a7u,<<,%a3@&,%sp,%acc1
    2618:	aed3 f2f2      	macw %d2u,%a7u,<<,%a3@&,%sp,%acc2
    261c:	a21a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d1,%acc1
    2620:	a29a f2d2      	macw %d2u,%a7u,<<,%a2@\+,%d1,%acc2
    2624:	a65a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%a3,%acc1
    2628:	a6da f2d2      	macw %d2u,%a7u,<<,%a2@\+,%a3,%acc2
    262c:	a41a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d2,%acc1
    2630:	a49a f2d2      	macw %d2u,%a7u,<<,%a2@\+,%d2,%acc2
    2634:	ae5a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%sp,%acc1
    2638:	aeda f2d2      	macw %d2u,%a7u,<<,%a2@\+,%sp,%acc2
    263c:	a21a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d1,%acc1
    2640:	a29a f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%d1,%acc2
    2644:	a65a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%a3,%acc1
    2648:	a6da f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%a3,%acc2
    264c:	a41a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d2,%acc1
    2650:	a49a f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%d2,%acc2
    2654:	ae5a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%sp,%acc1
    2658:	aeda f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%sp,%acc2
    265c:	a22e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1,%acc1
    2662:	a2ae f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1,%acc2
    2668:	a66e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3,%acc1
    266e:	a6ee f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3,%acc2
    2674:	a42e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2,%acc1
    267a:	a4ae f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2,%acc2
    2680:	ae6e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp,%acc1
    2686:	aeee f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp,%acc2
    268c:	a22e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    2692:	a2ae f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    2698:	a66e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    269e:	a6ee f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    26a4:	a42e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    26aa:	a4ae f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    26b0:	ae6e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    26b6:	aeee f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    26bc:	a221 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d1,%acc1
    26c0:	a2a1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%d1,%acc2
    26c4:	a661 f2c2      	macw %d2u,%a7u,<<,%a1@-,%a3,%acc1
    26c8:	a6e1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%a3,%acc2
    26cc:	a421 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d2,%acc1
    26d0:	a4a1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%d2,%acc2
    26d4:	ae61 f2c2      	macw %d2u,%a7u,<<,%a1@-,%sp,%acc1
    26d8:	aee1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%sp,%acc2
    26dc:	a221 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d1,%acc1
    26e0:	a2a1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%d1,%acc2
    26e4:	a661 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%a3,%acc1
    26e8:	a6e1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%a3,%acc2
    26ec:	a421 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d2,%acc1
    26f0:	a4a1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%d2,%acc2
    26f4:	ae61 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%sp,%acc1
    26f8:	aee1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%sp,%acc2
    26fc:	a213 f6c2      	macw %d2u,%a7u,>>,%a3@,%d1,%acc1
    2700:	a293 f6d2      	macw %d2u,%a7u,>>,%a3@,%d1,%acc2
    2704:	a653 f6c2      	macw %d2u,%a7u,>>,%a3@,%a3,%acc1
    2708:	a6d3 f6d2      	macw %d2u,%a7u,>>,%a3@,%a3,%acc2
    270c:	a413 f6c2      	macw %d2u,%a7u,>>,%a3@,%d2,%acc1
    2710:	a493 f6d2      	macw %d2u,%a7u,>>,%a3@,%d2,%acc2
    2714:	ae53 f6c2      	macw %d2u,%a7u,>>,%a3@,%sp,%acc1
    2718:	aed3 f6d2      	macw %d2u,%a7u,>>,%a3@,%sp,%acc2
    271c:	a213 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d1,%acc1
    2720:	a293 f6f2      	macw %d2u,%a7u,>>,%a3@&,%d1,%acc2
    2724:	a653 f6e2      	macw %d2u,%a7u,>>,%a3@&,%a3,%acc1
    2728:	a6d3 f6f2      	macw %d2u,%a7u,>>,%a3@&,%a3,%acc2
    272c:	a413 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d2,%acc1
    2730:	a493 f6f2      	macw %d2u,%a7u,>>,%a3@&,%d2,%acc2
    2734:	ae53 f6e2      	macw %d2u,%a7u,>>,%a3@&,%sp,%acc1
    2738:	aed3 f6f2      	macw %d2u,%a7u,>>,%a3@&,%sp,%acc2
    273c:	a21a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d1,%acc1
    2740:	a29a f6d2      	macw %d2u,%a7u,>>,%a2@\+,%d1,%acc2
    2744:	a65a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%a3,%acc1
    2748:	a6da f6d2      	macw %d2u,%a7u,>>,%a2@\+,%a3,%acc2
    274c:	a41a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d2,%acc1
    2750:	a49a f6d2      	macw %d2u,%a7u,>>,%a2@\+,%d2,%acc2
    2754:	ae5a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%sp,%acc1
    2758:	aeda f6d2      	macw %d2u,%a7u,>>,%a2@\+,%sp,%acc2
    275c:	a21a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d1,%acc1
    2760:	a29a f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%d1,%acc2
    2764:	a65a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%a3,%acc1
    2768:	a6da f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%a3,%acc2
    276c:	a41a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d2,%acc1
    2770:	a49a f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%d2,%acc2
    2774:	ae5a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%sp,%acc1
    2778:	aeda f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%sp,%acc2
    277c:	a22e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1,%acc1
    2782:	a2ae f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1,%acc2
    2788:	a66e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3,%acc1
    278e:	a6ee f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3,%acc2
    2794:	a42e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2,%acc1
    279a:	a4ae f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2,%acc2
    27a0:	ae6e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp,%acc1
    27a6:	aeee f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp,%acc2
    27ac:	a22e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    27b2:	a2ae f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    27b8:	a66e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    27be:	a6ee f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    27c4:	a42e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    27ca:	a4ae f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    27d0:	ae6e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    27d6:	aeee f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    27dc:	a221 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d1,%acc1
    27e0:	a2a1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%d1,%acc2
    27e4:	a661 f6c2      	macw %d2u,%a7u,>>,%a1@-,%a3,%acc1
    27e8:	a6e1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%a3,%acc2
    27ec:	a421 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d2,%acc1
    27f0:	a4a1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%d2,%acc2
    27f4:	ae61 f6c2      	macw %d2u,%a7u,>>,%a1@-,%sp,%acc1
    27f8:	aee1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%sp,%acc2
    27fc:	a221 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d1,%acc1
    2800:	a2a1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%d1,%acc2
    2804:	a661 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%a3,%acc1
    2808:	a6e1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%a3,%acc2
    280c:	a421 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d2,%acc1
    2810:	a4a1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%d2,%acc2
    2814:	ae61 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%sp,%acc1
    2818:	aee1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%sp,%acc2
    281c:	a213 f2c2      	macw %d2u,%a7u,<<,%a3@,%d1,%acc1
    2820:	a293 f2d2      	macw %d2u,%a7u,<<,%a3@,%d1,%acc2
    2824:	a653 f2c2      	macw %d2u,%a7u,<<,%a3@,%a3,%acc1
    2828:	a6d3 f2d2      	macw %d2u,%a7u,<<,%a3@,%a3,%acc2
    282c:	a413 f2c2      	macw %d2u,%a7u,<<,%a3@,%d2,%acc1
    2830:	a493 f2d2      	macw %d2u,%a7u,<<,%a3@,%d2,%acc2
    2834:	ae53 f2c2      	macw %d2u,%a7u,<<,%a3@,%sp,%acc1
    2838:	aed3 f2d2      	macw %d2u,%a7u,<<,%a3@,%sp,%acc2
    283c:	a213 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d1,%acc1
    2840:	a293 f2f2      	macw %d2u,%a7u,<<,%a3@&,%d1,%acc2
    2844:	a653 f2e2      	macw %d2u,%a7u,<<,%a3@&,%a3,%acc1
    2848:	a6d3 f2f2      	macw %d2u,%a7u,<<,%a3@&,%a3,%acc2
    284c:	a413 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d2,%acc1
    2850:	a493 f2f2      	macw %d2u,%a7u,<<,%a3@&,%d2,%acc2
    2854:	ae53 f2e2      	macw %d2u,%a7u,<<,%a3@&,%sp,%acc1
    2858:	aed3 f2f2      	macw %d2u,%a7u,<<,%a3@&,%sp,%acc2
    285c:	a21a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d1,%acc1
    2860:	a29a f2d2      	macw %d2u,%a7u,<<,%a2@\+,%d1,%acc2
    2864:	a65a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%a3,%acc1
    2868:	a6da f2d2      	macw %d2u,%a7u,<<,%a2@\+,%a3,%acc2
    286c:	a41a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d2,%acc1
    2870:	a49a f2d2      	macw %d2u,%a7u,<<,%a2@\+,%d2,%acc2
    2874:	ae5a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%sp,%acc1
    2878:	aeda f2d2      	macw %d2u,%a7u,<<,%a2@\+,%sp,%acc2
    287c:	a21a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d1,%acc1
    2880:	a29a f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%d1,%acc2
    2884:	a65a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%a3,%acc1
    2888:	a6da f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%a3,%acc2
    288c:	a41a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d2,%acc1
    2890:	a49a f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%d2,%acc2
    2894:	ae5a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%sp,%acc1
    2898:	aeda f2f2      	macw %d2u,%a7u,<<,%a2@\+&,%sp,%acc2
    289c:	a22e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1,%acc1
    28a2:	a2ae f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1,%acc2
    28a8:	a66e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3,%acc1
    28ae:	a6ee f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3,%acc2
    28b4:	a42e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2,%acc1
    28ba:	a4ae f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2,%acc2
    28c0:	ae6e f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp,%acc1
    28c6:	aeee f2d2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp,%acc2
    28cc:	a22e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    28d2:	a2ae f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    28d8:	a66e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    28de:	a6ee f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    28e4:	a42e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    28ea:	a4ae f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    28f0:	ae6e f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    28f6:	aeee f2f2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    28fc:	a221 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d1,%acc1
    2900:	a2a1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%d1,%acc2
    2904:	a661 f2c2      	macw %d2u,%a7u,<<,%a1@-,%a3,%acc1
    2908:	a6e1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%a3,%acc2
    290c:	a421 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d2,%acc1
    2910:	a4a1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%d2,%acc2
    2914:	ae61 f2c2      	macw %d2u,%a7u,<<,%a1@-,%sp,%acc1
    2918:	aee1 f2d2      	macw %d2u,%a7u,<<,%a1@-,%sp,%acc2
    291c:	a221 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d1,%acc1
    2920:	a2a1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%d1,%acc2
    2924:	a661 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%a3,%acc1
    2928:	a6e1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%a3,%acc2
    292c:	a421 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d2,%acc1
    2930:	a4a1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%d2,%acc2
    2934:	ae61 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%sp,%acc1
    2938:	aee1 f2f2      	macw %d2u,%a7u,<<,%a1@-&,%sp,%acc2
    293c:	a213 f6c2      	macw %d2u,%a7u,>>,%a3@,%d1,%acc1
    2940:	a293 f6d2      	macw %d2u,%a7u,>>,%a3@,%d1,%acc2
    2944:	a653 f6c2      	macw %d2u,%a7u,>>,%a3@,%a3,%acc1
    2948:	a6d3 f6d2      	macw %d2u,%a7u,>>,%a3@,%a3,%acc2
    294c:	a413 f6c2      	macw %d2u,%a7u,>>,%a3@,%d2,%acc1
    2950:	a493 f6d2      	macw %d2u,%a7u,>>,%a3@,%d2,%acc2
    2954:	ae53 f6c2      	macw %d2u,%a7u,>>,%a3@,%sp,%acc1
    2958:	aed3 f6d2      	macw %d2u,%a7u,>>,%a3@,%sp,%acc2
    295c:	a213 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d1,%acc1
    2960:	a293 f6f2      	macw %d2u,%a7u,>>,%a3@&,%d1,%acc2
    2964:	a653 f6e2      	macw %d2u,%a7u,>>,%a3@&,%a3,%acc1
    2968:	a6d3 f6f2      	macw %d2u,%a7u,>>,%a3@&,%a3,%acc2
    296c:	a413 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d2,%acc1
    2970:	a493 f6f2      	macw %d2u,%a7u,>>,%a3@&,%d2,%acc2
    2974:	ae53 f6e2      	macw %d2u,%a7u,>>,%a3@&,%sp,%acc1
    2978:	aed3 f6f2      	macw %d2u,%a7u,>>,%a3@&,%sp,%acc2
    297c:	a21a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d1,%acc1
    2980:	a29a f6d2      	macw %d2u,%a7u,>>,%a2@\+,%d1,%acc2
    2984:	a65a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%a3,%acc1
    2988:	a6da f6d2      	macw %d2u,%a7u,>>,%a2@\+,%a3,%acc2
    298c:	a41a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d2,%acc1
    2990:	a49a f6d2      	macw %d2u,%a7u,>>,%a2@\+,%d2,%acc2
    2994:	ae5a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%sp,%acc1
    2998:	aeda f6d2      	macw %d2u,%a7u,>>,%a2@\+,%sp,%acc2
    299c:	a21a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d1,%acc1
    29a0:	a29a f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%d1,%acc2
    29a4:	a65a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%a3,%acc1
    29a8:	a6da f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%a3,%acc2
    29ac:	a41a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d2,%acc1
    29b0:	a49a f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%d2,%acc2
    29b4:	ae5a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%sp,%acc1
    29b8:	aeda f6f2      	macw %d2u,%a7u,>>,%a2@\+&,%sp,%acc2
    29bc:	a22e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1,%acc1
    29c2:	a2ae f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1,%acc2
    29c8:	a66e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3,%acc1
    29ce:	a6ee f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3,%acc2
    29d4:	a42e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2,%acc1
    29da:	a4ae f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2,%acc2
    29e0:	ae6e f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp,%acc1
    29e6:	aeee f6d2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp,%acc2
    29ec:	a22e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    29f2:	a2ae f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    29f8:	a66e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    29fe:	a6ee f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    2a04:	a42e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    2a0a:	a4ae f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    2a10:	ae6e f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    2a16:	aeee f6f2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    2a1c:	a221 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d1,%acc1
    2a20:	a2a1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%d1,%acc2
    2a24:	a661 f6c2      	macw %d2u,%a7u,>>,%a1@-,%a3,%acc1
    2a28:	a6e1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%a3,%acc2
    2a2c:	a421 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d2,%acc1
    2a30:	a4a1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%d2,%acc2
    2a34:	ae61 f6c2      	macw %d2u,%a7u,>>,%a1@-,%sp,%acc1
    2a38:	aee1 f6d2      	macw %d2u,%a7u,>>,%a1@-,%sp,%acc2
    2a3c:	a221 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d1,%acc1
    2a40:	a2a1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%d1,%acc2
    2a44:	a661 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%a3,%acc1
    2a48:	a6e1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%a3,%acc2
    2a4c:	a421 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d2,%acc1
    2a50:	a4a1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%d2,%acc2
    2a54:	ae61 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%sp,%acc1
    2a58:	aee1 f6f2      	macw %d2u,%a7u,>>,%a1@-&,%sp,%acc2
    2a5c:	a213 1042      	macw %d2u,%d1l,%a3@,%d1,%acc1
    2a60:	a293 1052      	macw %d2u,%d1l,%a3@,%d1,%acc2
    2a64:	a653 1042      	macw %d2u,%d1l,%a3@,%a3,%acc1
    2a68:	a6d3 1052      	macw %d2u,%d1l,%a3@,%a3,%acc2
    2a6c:	a413 1042      	macw %d2u,%d1l,%a3@,%d2,%acc1
    2a70:	a493 1052      	macw %d2u,%d1l,%a3@,%d2,%acc2
    2a74:	ae53 1042      	macw %d2u,%d1l,%a3@,%sp,%acc1
    2a78:	aed3 1052      	macw %d2u,%d1l,%a3@,%sp,%acc2
    2a7c:	a213 1062      	macw %d2u,%d1l,%a3@&,%d1,%acc1
    2a80:	a293 1072      	macw %d2u,%d1l,%a3@&,%d1,%acc2
    2a84:	a653 1062      	macw %d2u,%d1l,%a3@&,%a3,%acc1
    2a88:	a6d3 1072      	macw %d2u,%d1l,%a3@&,%a3,%acc2
    2a8c:	a413 1062      	macw %d2u,%d1l,%a3@&,%d2,%acc1
    2a90:	a493 1072      	macw %d2u,%d1l,%a3@&,%d2,%acc2
    2a94:	ae53 1062      	macw %d2u,%d1l,%a3@&,%sp,%acc1
    2a98:	aed3 1072      	macw %d2u,%d1l,%a3@&,%sp,%acc2
    2a9c:	a21a 1042      	macw %d2u,%d1l,%a2@\+,%d1,%acc1
    2aa0:	a29a 1052      	macw %d2u,%d1l,%a2@\+,%d1,%acc2
    2aa4:	a65a 1042      	macw %d2u,%d1l,%a2@\+,%a3,%acc1
    2aa8:	a6da 1052      	macw %d2u,%d1l,%a2@\+,%a3,%acc2
    2aac:	a41a 1042      	macw %d2u,%d1l,%a2@\+,%d2,%acc1
    2ab0:	a49a 1052      	macw %d2u,%d1l,%a2@\+,%d2,%acc2
    2ab4:	ae5a 1042      	macw %d2u,%d1l,%a2@\+,%sp,%acc1
    2ab8:	aeda 1052      	macw %d2u,%d1l,%a2@\+,%sp,%acc2
    2abc:	a21a 1062      	macw %d2u,%d1l,%a2@\+&,%d1,%acc1
    2ac0:	a29a 1072      	macw %d2u,%d1l,%a2@\+&,%d1,%acc2
    2ac4:	a65a 1062      	macw %d2u,%d1l,%a2@\+&,%a3,%acc1
    2ac8:	a6da 1072      	macw %d2u,%d1l,%a2@\+&,%a3,%acc2
    2acc:	a41a 1062      	macw %d2u,%d1l,%a2@\+&,%d2,%acc1
    2ad0:	a49a 1072      	macw %d2u,%d1l,%a2@\+&,%d2,%acc2
    2ad4:	ae5a 1062      	macw %d2u,%d1l,%a2@\+&,%sp,%acc1
    2ad8:	aeda 1072      	macw %d2u,%d1l,%a2@\+&,%sp,%acc2
    2adc:	a22e 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%d1,%acc1
    2ae2:	a2ae 1052 000a 	macw %d2u,%d1l,%fp@\(10\),%d1,%acc2
    2ae8:	a66e 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%a3,%acc1
    2aee:	a6ee 1052 000a 	macw %d2u,%d1l,%fp@\(10\),%a3,%acc2
    2af4:	a42e 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%d2,%acc1
    2afa:	a4ae 1052 000a 	macw %d2u,%d1l,%fp@\(10\),%d2,%acc2
    2b00:	ae6e 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%sp,%acc1
    2b06:	aeee 1052 000a 	macw %d2u,%d1l,%fp@\(10\),%sp,%acc2
    2b0c:	a22e 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d1,%acc1
    2b12:	a2ae 1072 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d1,%acc2
    2b18:	a66e 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%a3,%acc1
    2b1e:	a6ee 1072 000a 	macw %d2u,%d1l,%fp@\(10\)&,%a3,%acc2
    2b24:	a42e 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d2,%acc1
    2b2a:	a4ae 1072 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d2,%acc2
    2b30:	ae6e 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%sp,%acc1
    2b36:	aeee 1072 000a 	macw %d2u,%d1l,%fp@\(10\)&,%sp,%acc2
    2b3c:	a221 1042      	macw %d2u,%d1l,%a1@-,%d1,%acc1
    2b40:	a2a1 1052      	macw %d2u,%d1l,%a1@-,%d1,%acc2
    2b44:	a661 1042      	macw %d2u,%d1l,%a1@-,%a3,%acc1
    2b48:	a6e1 1052      	macw %d2u,%d1l,%a1@-,%a3,%acc2
    2b4c:	a421 1042      	macw %d2u,%d1l,%a1@-,%d2,%acc1
    2b50:	a4a1 1052      	macw %d2u,%d1l,%a1@-,%d2,%acc2
    2b54:	ae61 1042      	macw %d2u,%d1l,%a1@-,%sp,%acc1
    2b58:	aee1 1052      	macw %d2u,%d1l,%a1@-,%sp,%acc2
    2b5c:	a221 1062      	macw %d2u,%d1l,%a1@-&,%d1,%acc1
    2b60:	a2a1 1072      	macw %d2u,%d1l,%a1@-&,%d1,%acc2
    2b64:	a661 1062      	macw %d2u,%d1l,%a1@-&,%a3,%acc1
    2b68:	a6e1 1072      	macw %d2u,%d1l,%a1@-&,%a3,%acc2
    2b6c:	a421 1062      	macw %d2u,%d1l,%a1@-&,%d2,%acc1
    2b70:	a4a1 1072      	macw %d2u,%d1l,%a1@-&,%d2,%acc2
    2b74:	ae61 1062      	macw %d2u,%d1l,%a1@-&,%sp,%acc1
    2b78:	aee1 1072      	macw %d2u,%d1l,%a1@-&,%sp,%acc2
    2b7c:	a213 1242      	macw %d2u,%d1l,<<,%a3@,%d1,%acc1
    2b80:	a293 1252      	macw %d2u,%d1l,<<,%a3@,%d1,%acc2
    2b84:	a653 1242      	macw %d2u,%d1l,<<,%a3@,%a3,%acc1
    2b88:	a6d3 1252      	macw %d2u,%d1l,<<,%a3@,%a3,%acc2
    2b8c:	a413 1242      	macw %d2u,%d1l,<<,%a3@,%d2,%acc1
    2b90:	a493 1252      	macw %d2u,%d1l,<<,%a3@,%d2,%acc2
    2b94:	ae53 1242      	macw %d2u,%d1l,<<,%a3@,%sp,%acc1
    2b98:	aed3 1252      	macw %d2u,%d1l,<<,%a3@,%sp,%acc2
    2b9c:	a213 1262      	macw %d2u,%d1l,<<,%a3@&,%d1,%acc1
    2ba0:	a293 1272      	macw %d2u,%d1l,<<,%a3@&,%d1,%acc2
    2ba4:	a653 1262      	macw %d2u,%d1l,<<,%a3@&,%a3,%acc1
    2ba8:	a6d3 1272      	macw %d2u,%d1l,<<,%a3@&,%a3,%acc2
    2bac:	a413 1262      	macw %d2u,%d1l,<<,%a3@&,%d2,%acc1
    2bb0:	a493 1272      	macw %d2u,%d1l,<<,%a3@&,%d2,%acc2
    2bb4:	ae53 1262      	macw %d2u,%d1l,<<,%a3@&,%sp,%acc1
    2bb8:	aed3 1272      	macw %d2u,%d1l,<<,%a3@&,%sp,%acc2
    2bbc:	a21a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d1,%acc1
    2bc0:	a29a 1252      	macw %d2u,%d1l,<<,%a2@\+,%d1,%acc2
    2bc4:	a65a 1242      	macw %d2u,%d1l,<<,%a2@\+,%a3,%acc1
    2bc8:	a6da 1252      	macw %d2u,%d1l,<<,%a2@\+,%a3,%acc2
    2bcc:	a41a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d2,%acc1
    2bd0:	a49a 1252      	macw %d2u,%d1l,<<,%a2@\+,%d2,%acc2
    2bd4:	ae5a 1242      	macw %d2u,%d1l,<<,%a2@\+,%sp,%acc1
    2bd8:	aeda 1252      	macw %d2u,%d1l,<<,%a2@\+,%sp,%acc2
    2bdc:	a21a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d1,%acc1
    2be0:	a29a 1272      	macw %d2u,%d1l,<<,%a2@\+&,%d1,%acc2
    2be4:	a65a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%a3,%acc1
    2be8:	a6da 1272      	macw %d2u,%d1l,<<,%a2@\+&,%a3,%acc2
    2bec:	a41a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d2,%acc1
    2bf0:	a49a 1272      	macw %d2u,%d1l,<<,%a2@\+&,%d2,%acc2
    2bf4:	ae5a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%sp,%acc1
    2bf8:	aeda 1272      	macw %d2u,%d1l,<<,%a2@\+&,%sp,%acc2
    2bfc:	a22e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1,%acc1
    2c02:	a2ae 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1,%acc2
    2c08:	a66e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3,%acc1
    2c0e:	a6ee 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3,%acc2
    2c14:	a42e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2,%acc1
    2c1a:	a4ae 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2,%acc2
    2c20:	ae6e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp,%acc1
    2c26:	aeee 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp,%acc2
    2c2c:	a22e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    2c32:	a2ae 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    2c38:	a66e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    2c3e:	a6ee 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    2c44:	a42e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    2c4a:	a4ae 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    2c50:	ae6e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    2c56:	aeee 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    2c5c:	a221 1242      	macw %d2u,%d1l,<<,%a1@-,%d1,%acc1
    2c60:	a2a1 1252      	macw %d2u,%d1l,<<,%a1@-,%d1,%acc2
    2c64:	a661 1242      	macw %d2u,%d1l,<<,%a1@-,%a3,%acc1
    2c68:	a6e1 1252      	macw %d2u,%d1l,<<,%a1@-,%a3,%acc2
    2c6c:	a421 1242      	macw %d2u,%d1l,<<,%a1@-,%d2,%acc1
    2c70:	a4a1 1252      	macw %d2u,%d1l,<<,%a1@-,%d2,%acc2
    2c74:	ae61 1242      	macw %d2u,%d1l,<<,%a1@-,%sp,%acc1
    2c78:	aee1 1252      	macw %d2u,%d1l,<<,%a1@-,%sp,%acc2
    2c7c:	a221 1262      	macw %d2u,%d1l,<<,%a1@-&,%d1,%acc1
    2c80:	a2a1 1272      	macw %d2u,%d1l,<<,%a1@-&,%d1,%acc2
    2c84:	a661 1262      	macw %d2u,%d1l,<<,%a1@-&,%a3,%acc1
    2c88:	a6e1 1272      	macw %d2u,%d1l,<<,%a1@-&,%a3,%acc2
    2c8c:	a421 1262      	macw %d2u,%d1l,<<,%a1@-&,%d2,%acc1
    2c90:	a4a1 1272      	macw %d2u,%d1l,<<,%a1@-&,%d2,%acc2
    2c94:	ae61 1262      	macw %d2u,%d1l,<<,%a1@-&,%sp,%acc1
    2c98:	aee1 1272      	macw %d2u,%d1l,<<,%a1@-&,%sp,%acc2
    2c9c:	a213 1642      	macw %d2u,%d1l,>>,%a3@,%d1,%acc1
    2ca0:	a293 1652      	macw %d2u,%d1l,>>,%a3@,%d1,%acc2
    2ca4:	a653 1642      	macw %d2u,%d1l,>>,%a3@,%a3,%acc1
    2ca8:	a6d3 1652      	macw %d2u,%d1l,>>,%a3@,%a3,%acc2
    2cac:	a413 1642      	macw %d2u,%d1l,>>,%a3@,%d2,%acc1
    2cb0:	a493 1652      	macw %d2u,%d1l,>>,%a3@,%d2,%acc2
    2cb4:	ae53 1642      	macw %d2u,%d1l,>>,%a3@,%sp,%acc1
    2cb8:	aed3 1652      	macw %d2u,%d1l,>>,%a3@,%sp,%acc2
    2cbc:	a213 1662      	macw %d2u,%d1l,>>,%a3@&,%d1,%acc1
    2cc0:	a293 1672      	macw %d2u,%d1l,>>,%a3@&,%d1,%acc2
    2cc4:	a653 1662      	macw %d2u,%d1l,>>,%a3@&,%a3,%acc1
    2cc8:	a6d3 1672      	macw %d2u,%d1l,>>,%a3@&,%a3,%acc2
    2ccc:	a413 1662      	macw %d2u,%d1l,>>,%a3@&,%d2,%acc1
    2cd0:	a493 1672      	macw %d2u,%d1l,>>,%a3@&,%d2,%acc2
    2cd4:	ae53 1662      	macw %d2u,%d1l,>>,%a3@&,%sp,%acc1
    2cd8:	aed3 1672      	macw %d2u,%d1l,>>,%a3@&,%sp,%acc2
    2cdc:	a21a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d1,%acc1
    2ce0:	a29a 1652      	macw %d2u,%d1l,>>,%a2@\+,%d1,%acc2
    2ce4:	a65a 1642      	macw %d2u,%d1l,>>,%a2@\+,%a3,%acc1
    2ce8:	a6da 1652      	macw %d2u,%d1l,>>,%a2@\+,%a3,%acc2
    2cec:	a41a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d2,%acc1
    2cf0:	a49a 1652      	macw %d2u,%d1l,>>,%a2@\+,%d2,%acc2
    2cf4:	ae5a 1642      	macw %d2u,%d1l,>>,%a2@\+,%sp,%acc1
    2cf8:	aeda 1652      	macw %d2u,%d1l,>>,%a2@\+,%sp,%acc2
    2cfc:	a21a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d1,%acc1
    2d00:	a29a 1672      	macw %d2u,%d1l,>>,%a2@\+&,%d1,%acc2
    2d04:	a65a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%a3,%acc1
    2d08:	a6da 1672      	macw %d2u,%d1l,>>,%a2@\+&,%a3,%acc2
    2d0c:	a41a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d2,%acc1
    2d10:	a49a 1672      	macw %d2u,%d1l,>>,%a2@\+&,%d2,%acc2
    2d14:	ae5a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%sp,%acc1
    2d18:	aeda 1672      	macw %d2u,%d1l,>>,%a2@\+&,%sp,%acc2
    2d1c:	a22e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1,%acc1
    2d22:	a2ae 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1,%acc2
    2d28:	a66e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3,%acc1
    2d2e:	a6ee 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3,%acc2
    2d34:	a42e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2,%acc1
    2d3a:	a4ae 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2,%acc2
    2d40:	ae6e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp,%acc1
    2d46:	aeee 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp,%acc2
    2d4c:	a22e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    2d52:	a2ae 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    2d58:	a66e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    2d5e:	a6ee 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    2d64:	a42e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    2d6a:	a4ae 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    2d70:	ae6e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    2d76:	aeee 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    2d7c:	a221 1642      	macw %d2u,%d1l,>>,%a1@-,%d1,%acc1
    2d80:	a2a1 1652      	macw %d2u,%d1l,>>,%a1@-,%d1,%acc2
    2d84:	a661 1642      	macw %d2u,%d1l,>>,%a1@-,%a3,%acc1
    2d88:	a6e1 1652      	macw %d2u,%d1l,>>,%a1@-,%a3,%acc2
    2d8c:	a421 1642      	macw %d2u,%d1l,>>,%a1@-,%d2,%acc1
    2d90:	a4a1 1652      	macw %d2u,%d1l,>>,%a1@-,%d2,%acc2
    2d94:	ae61 1642      	macw %d2u,%d1l,>>,%a1@-,%sp,%acc1
    2d98:	aee1 1652      	macw %d2u,%d1l,>>,%a1@-,%sp,%acc2
    2d9c:	a221 1662      	macw %d2u,%d1l,>>,%a1@-&,%d1,%acc1
    2da0:	a2a1 1672      	macw %d2u,%d1l,>>,%a1@-&,%d1,%acc2
    2da4:	a661 1662      	macw %d2u,%d1l,>>,%a1@-&,%a3,%acc1
    2da8:	a6e1 1672      	macw %d2u,%d1l,>>,%a1@-&,%a3,%acc2
    2dac:	a421 1662      	macw %d2u,%d1l,>>,%a1@-&,%d2,%acc1
    2db0:	a4a1 1672      	macw %d2u,%d1l,>>,%a1@-&,%d2,%acc2
    2db4:	ae61 1662      	macw %d2u,%d1l,>>,%a1@-&,%sp,%acc1
    2db8:	aee1 1672      	macw %d2u,%d1l,>>,%a1@-&,%sp,%acc2
    2dbc:	a213 1242      	macw %d2u,%d1l,<<,%a3@,%d1,%acc1
    2dc0:	a293 1252      	macw %d2u,%d1l,<<,%a3@,%d1,%acc2
    2dc4:	a653 1242      	macw %d2u,%d1l,<<,%a3@,%a3,%acc1
    2dc8:	a6d3 1252      	macw %d2u,%d1l,<<,%a3@,%a3,%acc2
    2dcc:	a413 1242      	macw %d2u,%d1l,<<,%a3@,%d2,%acc1
    2dd0:	a493 1252      	macw %d2u,%d1l,<<,%a3@,%d2,%acc2
    2dd4:	ae53 1242      	macw %d2u,%d1l,<<,%a3@,%sp,%acc1
    2dd8:	aed3 1252      	macw %d2u,%d1l,<<,%a3@,%sp,%acc2
    2ddc:	a213 1262      	macw %d2u,%d1l,<<,%a3@&,%d1,%acc1
    2de0:	a293 1272      	macw %d2u,%d1l,<<,%a3@&,%d1,%acc2
    2de4:	a653 1262      	macw %d2u,%d1l,<<,%a3@&,%a3,%acc1
    2de8:	a6d3 1272      	macw %d2u,%d1l,<<,%a3@&,%a3,%acc2
    2dec:	a413 1262      	macw %d2u,%d1l,<<,%a3@&,%d2,%acc1
    2df0:	a493 1272      	macw %d2u,%d1l,<<,%a3@&,%d2,%acc2
    2df4:	ae53 1262      	macw %d2u,%d1l,<<,%a3@&,%sp,%acc1
    2df8:	aed3 1272      	macw %d2u,%d1l,<<,%a3@&,%sp,%acc2
    2dfc:	a21a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d1,%acc1
    2e00:	a29a 1252      	macw %d2u,%d1l,<<,%a2@\+,%d1,%acc2
    2e04:	a65a 1242      	macw %d2u,%d1l,<<,%a2@\+,%a3,%acc1
    2e08:	a6da 1252      	macw %d2u,%d1l,<<,%a2@\+,%a3,%acc2
    2e0c:	a41a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d2,%acc1
    2e10:	a49a 1252      	macw %d2u,%d1l,<<,%a2@\+,%d2,%acc2
    2e14:	ae5a 1242      	macw %d2u,%d1l,<<,%a2@\+,%sp,%acc1
    2e18:	aeda 1252      	macw %d2u,%d1l,<<,%a2@\+,%sp,%acc2
    2e1c:	a21a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d1,%acc1
    2e20:	a29a 1272      	macw %d2u,%d1l,<<,%a2@\+&,%d1,%acc2
    2e24:	a65a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%a3,%acc1
    2e28:	a6da 1272      	macw %d2u,%d1l,<<,%a2@\+&,%a3,%acc2
    2e2c:	a41a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d2,%acc1
    2e30:	a49a 1272      	macw %d2u,%d1l,<<,%a2@\+&,%d2,%acc2
    2e34:	ae5a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%sp,%acc1
    2e38:	aeda 1272      	macw %d2u,%d1l,<<,%a2@\+&,%sp,%acc2
    2e3c:	a22e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1,%acc1
    2e42:	a2ae 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1,%acc2
    2e48:	a66e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3,%acc1
    2e4e:	a6ee 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3,%acc2
    2e54:	a42e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2,%acc1
    2e5a:	a4ae 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2,%acc2
    2e60:	ae6e 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp,%acc1
    2e66:	aeee 1252 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp,%acc2
    2e6c:	a22e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    2e72:	a2ae 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    2e78:	a66e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    2e7e:	a6ee 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    2e84:	a42e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    2e8a:	a4ae 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    2e90:	ae6e 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    2e96:	aeee 1272 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    2e9c:	a221 1242      	macw %d2u,%d1l,<<,%a1@-,%d1,%acc1
    2ea0:	a2a1 1252      	macw %d2u,%d1l,<<,%a1@-,%d1,%acc2
    2ea4:	a661 1242      	macw %d2u,%d1l,<<,%a1@-,%a3,%acc1
    2ea8:	a6e1 1252      	macw %d2u,%d1l,<<,%a1@-,%a3,%acc2
    2eac:	a421 1242      	macw %d2u,%d1l,<<,%a1@-,%d2,%acc1
    2eb0:	a4a1 1252      	macw %d2u,%d1l,<<,%a1@-,%d2,%acc2
    2eb4:	ae61 1242      	macw %d2u,%d1l,<<,%a1@-,%sp,%acc1
    2eb8:	aee1 1252      	macw %d2u,%d1l,<<,%a1@-,%sp,%acc2
    2ebc:	a221 1262      	macw %d2u,%d1l,<<,%a1@-&,%d1,%acc1
    2ec0:	a2a1 1272      	macw %d2u,%d1l,<<,%a1@-&,%d1,%acc2
    2ec4:	a661 1262      	macw %d2u,%d1l,<<,%a1@-&,%a3,%acc1
    2ec8:	a6e1 1272      	macw %d2u,%d1l,<<,%a1@-&,%a3,%acc2
    2ecc:	a421 1262      	macw %d2u,%d1l,<<,%a1@-&,%d2,%acc1
    2ed0:	a4a1 1272      	macw %d2u,%d1l,<<,%a1@-&,%d2,%acc2
    2ed4:	ae61 1262      	macw %d2u,%d1l,<<,%a1@-&,%sp,%acc1
    2ed8:	aee1 1272      	macw %d2u,%d1l,<<,%a1@-&,%sp,%acc2
    2edc:	a213 1642      	macw %d2u,%d1l,>>,%a3@,%d1,%acc1
    2ee0:	a293 1652      	macw %d2u,%d1l,>>,%a3@,%d1,%acc2
    2ee4:	a653 1642      	macw %d2u,%d1l,>>,%a3@,%a3,%acc1
    2ee8:	a6d3 1652      	macw %d2u,%d1l,>>,%a3@,%a3,%acc2
    2eec:	a413 1642      	macw %d2u,%d1l,>>,%a3@,%d2,%acc1
    2ef0:	a493 1652      	macw %d2u,%d1l,>>,%a3@,%d2,%acc2
    2ef4:	ae53 1642      	macw %d2u,%d1l,>>,%a3@,%sp,%acc1
    2ef8:	aed3 1652      	macw %d2u,%d1l,>>,%a3@,%sp,%acc2
    2efc:	a213 1662      	macw %d2u,%d1l,>>,%a3@&,%d1,%acc1
    2f00:	a293 1672      	macw %d2u,%d1l,>>,%a3@&,%d1,%acc2
    2f04:	a653 1662      	macw %d2u,%d1l,>>,%a3@&,%a3,%acc1
    2f08:	a6d3 1672      	macw %d2u,%d1l,>>,%a3@&,%a3,%acc2
    2f0c:	a413 1662      	macw %d2u,%d1l,>>,%a3@&,%d2,%acc1
    2f10:	a493 1672      	macw %d2u,%d1l,>>,%a3@&,%d2,%acc2
    2f14:	ae53 1662      	macw %d2u,%d1l,>>,%a3@&,%sp,%acc1
    2f18:	aed3 1672      	macw %d2u,%d1l,>>,%a3@&,%sp,%acc2
    2f1c:	a21a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d1,%acc1
    2f20:	a29a 1652      	macw %d2u,%d1l,>>,%a2@\+,%d1,%acc2
    2f24:	a65a 1642      	macw %d2u,%d1l,>>,%a2@\+,%a3,%acc1
    2f28:	a6da 1652      	macw %d2u,%d1l,>>,%a2@\+,%a3,%acc2
    2f2c:	a41a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d2,%acc1
    2f30:	a49a 1652      	macw %d2u,%d1l,>>,%a2@\+,%d2,%acc2
    2f34:	ae5a 1642      	macw %d2u,%d1l,>>,%a2@\+,%sp,%acc1
    2f38:	aeda 1652      	macw %d2u,%d1l,>>,%a2@\+,%sp,%acc2
    2f3c:	a21a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d1,%acc1
    2f40:	a29a 1672      	macw %d2u,%d1l,>>,%a2@\+&,%d1,%acc2
    2f44:	a65a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%a3,%acc1
    2f48:	a6da 1672      	macw %d2u,%d1l,>>,%a2@\+&,%a3,%acc2
    2f4c:	a41a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d2,%acc1
    2f50:	a49a 1672      	macw %d2u,%d1l,>>,%a2@\+&,%d2,%acc2
    2f54:	ae5a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%sp,%acc1
    2f58:	aeda 1672      	macw %d2u,%d1l,>>,%a2@\+&,%sp,%acc2
    2f5c:	a22e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1,%acc1
    2f62:	a2ae 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1,%acc2
    2f68:	a66e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3,%acc1
    2f6e:	a6ee 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3,%acc2
    2f74:	a42e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2,%acc1
    2f7a:	a4ae 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2,%acc2
    2f80:	ae6e 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp,%acc1
    2f86:	aeee 1652 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp,%acc2
    2f8c:	a22e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    2f92:	a2ae 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    2f98:	a66e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    2f9e:	a6ee 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    2fa4:	a42e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    2faa:	a4ae 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    2fb0:	ae6e 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    2fb6:	aeee 1672 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    2fbc:	a221 1642      	macw %d2u,%d1l,>>,%a1@-,%d1,%acc1
    2fc0:	a2a1 1652      	macw %d2u,%d1l,>>,%a1@-,%d1,%acc2
    2fc4:	a661 1642      	macw %d2u,%d1l,>>,%a1@-,%a3,%acc1
    2fc8:	a6e1 1652      	macw %d2u,%d1l,>>,%a1@-,%a3,%acc2
    2fcc:	a421 1642      	macw %d2u,%d1l,>>,%a1@-,%d2,%acc1
    2fd0:	a4a1 1652      	macw %d2u,%d1l,>>,%a1@-,%d2,%acc2
    2fd4:	ae61 1642      	macw %d2u,%d1l,>>,%a1@-,%sp,%acc1
    2fd8:	aee1 1652      	macw %d2u,%d1l,>>,%a1@-,%sp,%acc2
    2fdc:	a221 1662      	macw %d2u,%d1l,>>,%a1@-&,%d1,%acc1
    2fe0:	a2a1 1672      	macw %d2u,%d1l,>>,%a1@-&,%d1,%acc2
    2fe4:	a661 1662      	macw %d2u,%d1l,>>,%a1@-&,%a3,%acc1
    2fe8:	a6e1 1672      	macw %d2u,%d1l,>>,%a1@-&,%a3,%acc2
    2fec:	a421 1662      	macw %d2u,%d1l,>>,%a1@-&,%d2,%acc1
    2ff0:	a4a1 1672      	macw %d2u,%d1l,>>,%a1@-&,%d2,%acc2
    2ff4:	ae61 1662      	macw %d2u,%d1l,>>,%a1@-&,%sp,%acc1
    2ff8:	aee1 1672      	macw %d2u,%d1l,>>,%a1@-&,%sp,%acc2
    2ffc:	a213 a08d      	macw %a5l,%a2u,%a3@,%d1,%acc1
    3000:	a293 a09d      	macw %a5l,%a2u,%a3@,%d1,%acc2
    3004:	a653 a08d      	macw %a5l,%a2u,%a3@,%a3,%acc1
    3008:	a6d3 a09d      	macw %a5l,%a2u,%a3@,%a3,%acc2
    300c:	a413 a08d      	macw %a5l,%a2u,%a3@,%d2,%acc1
    3010:	a493 a09d      	macw %a5l,%a2u,%a3@,%d2,%acc2
    3014:	ae53 a08d      	macw %a5l,%a2u,%a3@,%sp,%acc1
    3018:	aed3 a09d      	macw %a5l,%a2u,%a3@,%sp,%acc2
    301c:	a213 a0ad      	macw %a5l,%a2u,%a3@&,%d1,%acc1
    3020:	a293 a0bd      	macw %a5l,%a2u,%a3@&,%d1,%acc2
    3024:	a653 a0ad      	macw %a5l,%a2u,%a3@&,%a3,%acc1
    3028:	a6d3 a0bd      	macw %a5l,%a2u,%a3@&,%a3,%acc2
    302c:	a413 a0ad      	macw %a5l,%a2u,%a3@&,%d2,%acc1
    3030:	a493 a0bd      	macw %a5l,%a2u,%a3@&,%d2,%acc2
    3034:	ae53 a0ad      	macw %a5l,%a2u,%a3@&,%sp,%acc1
    3038:	aed3 a0bd      	macw %a5l,%a2u,%a3@&,%sp,%acc2
    303c:	a21a a08d      	macw %a5l,%a2u,%a2@\+,%d1,%acc1
    3040:	a29a a09d      	macw %a5l,%a2u,%a2@\+,%d1,%acc2
    3044:	a65a a08d      	macw %a5l,%a2u,%a2@\+,%a3,%acc1
    3048:	a6da a09d      	macw %a5l,%a2u,%a2@\+,%a3,%acc2
    304c:	a41a a08d      	macw %a5l,%a2u,%a2@\+,%d2,%acc1
    3050:	a49a a09d      	macw %a5l,%a2u,%a2@\+,%d2,%acc2
    3054:	ae5a a08d      	macw %a5l,%a2u,%a2@\+,%sp,%acc1
    3058:	aeda a09d      	macw %a5l,%a2u,%a2@\+,%sp,%acc2
    305c:	a21a a0ad      	macw %a5l,%a2u,%a2@\+&,%d1,%acc1
    3060:	a29a a0bd      	macw %a5l,%a2u,%a2@\+&,%d1,%acc2
    3064:	a65a a0ad      	macw %a5l,%a2u,%a2@\+&,%a3,%acc1
    3068:	a6da a0bd      	macw %a5l,%a2u,%a2@\+&,%a3,%acc2
    306c:	a41a a0ad      	macw %a5l,%a2u,%a2@\+&,%d2,%acc1
    3070:	a49a a0bd      	macw %a5l,%a2u,%a2@\+&,%d2,%acc2
    3074:	ae5a a0ad      	macw %a5l,%a2u,%a2@\+&,%sp,%acc1
    3078:	aeda a0bd      	macw %a5l,%a2u,%a2@\+&,%sp,%acc2
    307c:	a22e a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%d1,%acc1
    3082:	a2ae a09d 000a 	macw %a5l,%a2u,%fp@\(10\),%d1,%acc2
    3088:	a66e a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%a3,%acc1
    308e:	a6ee a09d 000a 	macw %a5l,%a2u,%fp@\(10\),%a3,%acc2
    3094:	a42e a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%d2,%acc1
    309a:	a4ae a09d 000a 	macw %a5l,%a2u,%fp@\(10\),%d2,%acc2
    30a0:	ae6e a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%sp,%acc1
    30a6:	aeee a09d 000a 	macw %a5l,%a2u,%fp@\(10\),%sp,%acc2
    30ac:	a22e a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d1,%acc1
    30b2:	a2ae a0bd 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d1,%acc2
    30b8:	a66e a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%a3,%acc1
    30be:	a6ee a0bd 000a 	macw %a5l,%a2u,%fp@\(10\)&,%a3,%acc2
    30c4:	a42e a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d2,%acc1
    30ca:	a4ae a0bd 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d2,%acc2
    30d0:	ae6e a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%sp,%acc1
    30d6:	aeee a0bd 000a 	macw %a5l,%a2u,%fp@\(10\)&,%sp,%acc2
    30dc:	a221 a08d      	macw %a5l,%a2u,%a1@-,%d1,%acc1
    30e0:	a2a1 a09d      	macw %a5l,%a2u,%a1@-,%d1,%acc2
    30e4:	a661 a08d      	macw %a5l,%a2u,%a1@-,%a3,%acc1
    30e8:	a6e1 a09d      	macw %a5l,%a2u,%a1@-,%a3,%acc2
    30ec:	a421 a08d      	macw %a5l,%a2u,%a1@-,%d2,%acc1
    30f0:	a4a1 a09d      	macw %a5l,%a2u,%a1@-,%d2,%acc2
    30f4:	ae61 a08d      	macw %a5l,%a2u,%a1@-,%sp,%acc1
    30f8:	aee1 a09d      	macw %a5l,%a2u,%a1@-,%sp,%acc2
    30fc:	a221 a0ad      	macw %a5l,%a2u,%a1@-&,%d1,%acc1
    3100:	a2a1 a0bd      	macw %a5l,%a2u,%a1@-&,%d1,%acc2
    3104:	a661 a0ad      	macw %a5l,%a2u,%a1@-&,%a3,%acc1
    3108:	a6e1 a0bd      	macw %a5l,%a2u,%a1@-&,%a3,%acc2
    310c:	a421 a0ad      	macw %a5l,%a2u,%a1@-&,%d2,%acc1
    3110:	a4a1 a0bd      	macw %a5l,%a2u,%a1@-&,%d2,%acc2
    3114:	ae61 a0ad      	macw %a5l,%a2u,%a1@-&,%sp,%acc1
    3118:	aee1 a0bd      	macw %a5l,%a2u,%a1@-&,%sp,%acc2
    311c:	a213 a28d      	macw %a5l,%a2u,<<,%a3@,%d1,%acc1
    3120:	a293 a29d      	macw %a5l,%a2u,<<,%a3@,%d1,%acc2
    3124:	a653 a28d      	macw %a5l,%a2u,<<,%a3@,%a3,%acc1
    3128:	a6d3 a29d      	macw %a5l,%a2u,<<,%a3@,%a3,%acc2
    312c:	a413 a28d      	macw %a5l,%a2u,<<,%a3@,%d2,%acc1
    3130:	a493 a29d      	macw %a5l,%a2u,<<,%a3@,%d2,%acc2
    3134:	ae53 a28d      	macw %a5l,%a2u,<<,%a3@,%sp,%acc1
    3138:	aed3 a29d      	macw %a5l,%a2u,<<,%a3@,%sp,%acc2
    313c:	a213 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d1,%acc1
    3140:	a293 a2bd      	macw %a5l,%a2u,<<,%a3@&,%d1,%acc2
    3144:	a653 a2ad      	macw %a5l,%a2u,<<,%a3@&,%a3,%acc1
    3148:	a6d3 a2bd      	macw %a5l,%a2u,<<,%a3@&,%a3,%acc2
    314c:	a413 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d2,%acc1
    3150:	a493 a2bd      	macw %a5l,%a2u,<<,%a3@&,%d2,%acc2
    3154:	ae53 a2ad      	macw %a5l,%a2u,<<,%a3@&,%sp,%acc1
    3158:	aed3 a2bd      	macw %a5l,%a2u,<<,%a3@&,%sp,%acc2
    315c:	a21a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d1,%acc1
    3160:	a29a a29d      	macw %a5l,%a2u,<<,%a2@\+,%d1,%acc2
    3164:	a65a a28d      	macw %a5l,%a2u,<<,%a2@\+,%a3,%acc1
    3168:	a6da a29d      	macw %a5l,%a2u,<<,%a2@\+,%a3,%acc2
    316c:	a41a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d2,%acc1
    3170:	a49a a29d      	macw %a5l,%a2u,<<,%a2@\+,%d2,%acc2
    3174:	ae5a a28d      	macw %a5l,%a2u,<<,%a2@\+,%sp,%acc1
    3178:	aeda a29d      	macw %a5l,%a2u,<<,%a2@\+,%sp,%acc2
    317c:	a21a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d1,%acc1
    3180:	a29a a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%d1,%acc2
    3184:	a65a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%a3,%acc1
    3188:	a6da a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%a3,%acc2
    318c:	a41a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d2,%acc1
    3190:	a49a a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%d2,%acc2
    3194:	ae5a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%sp,%acc1
    3198:	aeda a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%sp,%acc2
    319c:	a22e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1,%acc1
    31a2:	a2ae a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1,%acc2
    31a8:	a66e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3,%acc1
    31ae:	a6ee a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3,%acc2
    31b4:	a42e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2,%acc1
    31ba:	a4ae a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2,%acc2
    31c0:	ae6e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp,%acc1
    31c6:	aeee a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp,%acc2
    31cc:	a22e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    31d2:	a2ae a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    31d8:	a66e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    31de:	a6ee a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    31e4:	a42e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    31ea:	a4ae a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    31f0:	ae6e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    31f6:	aeee a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    31fc:	a221 a28d      	macw %a5l,%a2u,<<,%a1@-,%d1,%acc1
    3200:	a2a1 a29d      	macw %a5l,%a2u,<<,%a1@-,%d1,%acc2
    3204:	a661 a28d      	macw %a5l,%a2u,<<,%a1@-,%a3,%acc1
    3208:	a6e1 a29d      	macw %a5l,%a2u,<<,%a1@-,%a3,%acc2
    320c:	a421 a28d      	macw %a5l,%a2u,<<,%a1@-,%d2,%acc1
    3210:	a4a1 a29d      	macw %a5l,%a2u,<<,%a1@-,%d2,%acc2
    3214:	ae61 a28d      	macw %a5l,%a2u,<<,%a1@-,%sp,%acc1
    3218:	aee1 a29d      	macw %a5l,%a2u,<<,%a1@-,%sp,%acc2
    321c:	a221 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d1,%acc1
    3220:	a2a1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%d1,%acc2
    3224:	a661 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%a3,%acc1
    3228:	a6e1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%a3,%acc2
    322c:	a421 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d2,%acc1
    3230:	a4a1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%d2,%acc2
    3234:	ae61 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%sp,%acc1
    3238:	aee1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%sp,%acc2
    323c:	a213 a68d      	macw %a5l,%a2u,>>,%a3@,%d1,%acc1
    3240:	a293 a69d      	macw %a5l,%a2u,>>,%a3@,%d1,%acc2
    3244:	a653 a68d      	macw %a5l,%a2u,>>,%a3@,%a3,%acc1
    3248:	a6d3 a69d      	macw %a5l,%a2u,>>,%a3@,%a3,%acc2
    324c:	a413 a68d      	macw %a5l,%a2u,>>,%a3@,%d2,%acc1
    3250:	a493 a69d      	macw %a5l,%a2u,>>,%a3@,%d2,%acc2
    3254:	ae53 a68d      	macw %a5l,%a2u,>>,%a3@,%sp,%acc1
    3258:	aed3 a69d      	macw %a5l,%a2u,>>,%a3@,%sp,%acc2
    325c:	a213 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d1,%acc1
    3260:	a293 a6bd      	macw %a5l,%a2u,>>,%a3@&,%d1,%acc2
    3264:	a653 a6ad      	macw %a5l,%a2u,>>,%a3@&,%a3,%acc1
    3268:	a6d3 a6bd      	macw %a5l,%a2u,>>,%a3@&,%a3,%acc2
    326c:	a413 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d2,%acc1
    3270:	a493 a6bd      	macw %a5l,%a2u,>>,%a3@&,%d2,%acc2
    3274:	ae53 a6ad      	macw %a5l,%a2u,>>,%a3@&,%sp,%acc1
    3278:	aed3 a6bd      	macw %a5l,%a2u,>>,%a3@&,%sp,%acc2
    327c:	a21a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d1,%acc1
    3280:	a29a a69d      	macw %a5l,%a2u,>>,%a2@\+,%d1,%acc2
    3284:	a65a a68d      	macw %a5l,%a2u,>>,%a2@\+,%a3,%acc1
    3288:	a6da a69d      	macw %a5l,%a2u,>>,%a2@\+,%a3,%acc2
    328c:	a41a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d2,%acc1
    3290:	a49a a69d      	macw %a5l,%a2u,>>,%a2@\+,%d2,%acc2
    3294:	ae5a a68d      	macw %a5l,%a2u,>>,%a2@\+,%sp,%acc1
    3298:	aeda a69d      	macw %a5l,%a2u,>>,%a2@\+,%sp,%acc2
    329c:	a21a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d1,%acc1
    32a0:	a29a a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%d1,%acc2
    32a4:	a65a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%a3,%acc1
    32a8:	a6da a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%a3,%acc2
    32ac:	a41a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d2,%acc1
    32b0:	a49a a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%d2,%acc2
    32b4:	ae5a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%sp,%acc1
    32b8:	aeda a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%sp,%acc2
    32bc:	a22e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1,%acc1
    32c2:	a2ae a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1,%acc2
    32c8:	a66e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3,%acc1
    32ce:	a6ee a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3,%acc2
    32d4:	a42e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2,%acc1
    32da:	a4ae a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2,%acc2
    32e0:	ae6e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp,%acc1
    32e6:	aeee a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp,%acc2
    32ec:	a22e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    32f2:	a2ae a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    32f8:	a66e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    32fe:	a6ee a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    3304:	a42e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    330a:	a4ae a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    3310:	ae6e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    3316:	aeee a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    331c:	a221 a68d      	macw %a5l,%a2u,>>,%a1@-,%d1,%acc1
    3320:	a2a1 a69d      	macw %a5l,%a2u,>>,%a1@-,%d1,%acc2
    3324:	a661 a68d      	macw %a5l,%a2u,>>,%a1@-,%a3,%acc1
    3328:	a6e1 a69d      	macw %a5l,%a2u,>>,%a1@-,%a3,%acc2
    332c:	a421 a68d      	macw %a5l,%a2u,>>,%a1@-,%d2,%acc1
    3330:	a4a1 a69d      	macw %a5l,%a2u,>>,%a1@-,%d2,%acc2
    3334:	ae61 a68d      	macw %a5l,%a2u,>>,%a1@-,%sp,%acc1
    3338:	aee1 a69d      	macw %a5l,%a2u,>>,%a1@-,%sp,%acc2
    333c:	a221 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d1,%acc1
    3340:	a2a1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%d1,%acc2
    3344:	a661 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%a3,%acc1
    3348:	a6e1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%a3,%acc2
    334c:	a421 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d2,%acc1
    3350:	a4a1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%d2,%acc2
    3354:	ae61 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%sp,%acc1
    3358:	aee1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%sp,%acc2
    335c:	a213 a28d      	macw %a5l,%a2u,<<,%a3@,%d1,%acc1
    3360:	a293 a29d      	macw %a5l,%a2u,<<,%a3@,%d1,%acc2
    3364:	a653 a28d      	macw %a5l,%a2u,<<,%a3@,%a3,%acc1
    3368:	a6d3 a29d      	macw %a5l,%a2u,<<,%a3@,%a3,%acc2
    336c:	a413 a28d      	macw %a5l,%a2u,<<,%a3@,%d2,%acc1
    3370:	a493 a29d      	macw %a5l,%a2u,<<,%a3@,%d2,%acc2
    3374:	ae53 a28d      	macw %a5l,%a2u,<<,%a3@,%sp,%acc1
    3378:	aed3 a29d      	macw %a5l,%a2u,<<,%a3@,%sp,%acc2
    337c:	a213 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d1,%acc1
    3380:	a293 a2bd      	macw %a5l,%a2u,<<,%a3@&,%d1,%acc2
    3384:	a653 a2ad      	macw %a5l,%a2u,<<,%a3@&,%a3,%acc1
    3388:	a6d3 a2bd      	macw %a5l,%a2u,<<,%a3@&,%a3,%acc2
    338c:	a413 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d2,%acc1
    3390:	a493 a2bd      	macw %a5l,%a2u,<<,%a3@&,%d2,%acc2
    3394:	ae53 a2ad      	macw %a5l,%a2u,<<,%a3@&,%sp,%acc1
    3398:	aed3 a2bd      	macw %a5l,%a2u,<<,%a3@&,%sp,%acc2
    339c:	a21a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d1,%acc1
    33a0:	a29a a29d      	macw %a5l,%a2u,<<,%a2@\+,%d1,%acc2
    33a4:	a65a a28d      	macw %a5l,%a2u,<<,%a2@\+,%a3,%acc1
    33a8:	a6da a29d      	macw %a5l,%a2u,<<,%a2@\+,%a3,%acc2
    33ac:	a41a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d2,%acc1
    33b0:	a49a a29d      	macw %a5l,%a2u,<<,%a2@\+,%d2,%acc2
    33b4:	ae5a a28d      	macw %a5l,%a2u,<<,%a2@\+,%sp,%acc1
    33b8:	aeda a29d      	macw %a5l,%a2u,<<,%a2@\+,%sp,%acc2
    33bc:	a21a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d1,%acc1
    33c0:	a29a a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%d1,%acc2
    33c4:	a65a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%a3,%acc1
    33c8:	a6da a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%a3,%acc2
    33cc:	a41a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d2,%acc1
    33d0:	a49a a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%d2,%acc2
    33d4:	ae5a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%sp,%acc1
    33d8:	aeda a2bd      	macw %a5l,%a2u,<<,%a2@\+&,%sp,%acc2
    33dc:	a22e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1,%acc1
    33e2:	a2ae a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1,%acc2
    33e8:	a66e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3,%acc1
    33ee:	a6ee a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3,%acc2
    33f4:	a42e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2,%acc1
    33fa:	a4ae a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2,%acc2
    3400:	ae6e a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp,%acc1
    3406:	aeee a29d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp,%acc2
    340c:	a22e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    3412:	a2ae a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    3418:	a66e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    341e:	a6ee a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    3424:	a42e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    342a:	a4ae a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    3430:	ae6e a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    3436:	aeee a2bd 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    343c:	a221 a28d      	macw %a5l,%a2u,<<,%a1@-,%d1,%acc1
    3440:	a2a1 a29d      	macw %a5l,%a2u,<<,%a1@-,%d1,%acc2
    3444:	a661 a28d      	macw %a5l,%a2u,<<,%a1@-,%a3,%acc1
    3448:	a6e1 a29d      	macw %a5l,%a2u,<<,%a1@-,%a3,%acc2
    344c:	a421 a28d      	macw %a5l,%a2u,<<,%a1@-,%d2,%acc1
    3450:	a4a1 a29d      	macw %a5l,%a2u,<<,%a1@-,%d2,%acc2
    3454:	ae61 a28d      	macw %a5l,%a2u,<<,%a1@-,%sp,%acc1
    3458:	aee1 a29d      	macw %a5l,%a2u,<<,%a1@-,%sp,%acc2
    345c:	a221 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d1,%acc1
    3460:	a2a1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%d1,%acc2
    3464:	a661 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%a3,%acc1
    3468:	a6e1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%a3,%acc2
    346c:	a421 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d2,%acc1
    3470:	a4a1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%d2,%acc2
    3474:	ae61 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%sp,%acc1
    3478:	aee1 a2bd      	macw %a5l,%a2u,<<,%a1@-&,%sp,%acc2
    347c:	a213 a68d      	macw %a5l,%a2u,>>,%a3@,%d1,%acc1
    3480:	a293 a69d      	macw %a5l,%a2u,>>,%a3@,%d1,%acc2
    3484:	a653 a68d      	macw %a5l,%a2u,>>,%a3@,%a3,%acc1
    3488:	a6d3 a69d      	macw %a5l,%a2u,>>,%a3@,%a3,%acc2
    348c:	a413 a68d      	macw %a5l,%a2u,>>,%a3@,%d2,%acc1
    3490:	a493 a69d      	macw %a5l,%a2u,>>,%a3@,%d2,%acc2
    3494:	ae53 a68d      	macw %a5l,%a2u,>>,%a3@,%sp,%acc1
    3498:	aed3 a69d      	macw %a5l,%a2u,>>,%a3@,%sp,%acc2
    349c:	a213 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d1,%acc1
    34a0:	a293 a6bd      	macw %a5l,%a2u,>>,%a3@&,%d1,%acc2
    34a4:	a653 a6ad      	macw %a5l,%a2u,>>,%a3@&,%a3,%acc1
    34a8:	a6d3 a6bd      	macw %a5l,%a2u,>>,%a3@&,%a3,%acc2
    34ac:	a413 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d2,%acc1
    34b0:	a493 a6bd      	macw %a5l,%a2u,>>,%a3@&,%d2,%acc2
    34b4:	ae53 a6ad      	macw %a5l,%a2u,>>,%a3@&,%sp,%acc1
    34b8:	aed3 a6bd      	macw %a5l,%a2u,>>,%a3@&,%sp,%acc2
    34bc:	a21a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d1,%acc1
    34c0:	a29a a69d      	macw %a5l,%a2u,>>,%a2@\+,%d1,%acc2
    34c4:	a65a a68d      	macw %a5l,%a2u,>>,%a2@\+,%a3,%acc1
    34c8:	a6da a69d      	macw %a5l,%a2u,>>,%a2@\+,%a3,%acc2
    34cc:	a41a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d2,%acc1
    34d0:	a49a a69d      	macw %a5l,%a2u,>>,%a2@\+,%d2,%acc2
    34d4:	ae5a a68d      	macw %a5l,%a2u,>>,%a2@\+,%sp,%acc1
    34d8:	aeda a69d      	macw %a5l,%a2u,>>,%a2@\+,%sp,%acc2
    34dc:	a21a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d1,%acc1
    34e0:	a29a a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%d1,%acc2
    34e4:	a65a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%a3,%acc1
    34e8:	a6da a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%a3,%acc2
    34ec:	a41a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d2,%acc1
    34f0:	a49a a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%d2,%acc2
    34f4:	ae5a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%sp,%acc1
    34f8:	aeda a6bd      	macw %a5l,%a2u,>>,%a2@\+&,%sp,%acc2
    34fc:	a22e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1,%acc1
    3502:	a2ae a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1,%acc2
    3508:	a66e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3,%acc1
    350e:	a6ee a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3,%acc2
    3514:	a42e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2,%acc1
    351a:	a4ae a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2,%acc2
    3520:	ae6e a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp,%acc1
    3526:	aeee a69d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp,%acc2
    352c:	a22e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    3532:	a2ae a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    3538:	a66e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    353e:	a6ee a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    3544:	a42e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    354a:	a4ae a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    3550:	ae6e a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    3556:	aeee a6bd 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    355c:	a221 a68d      	macw %a5l,%a2u,>>,%a1@-,%d1,%acc1
    3560:	a2a1 a69d      	macw %a5l,%a2u,>>,%a1@-,%d1,%acc2
    3564:	a661 a68d      	macw %a5l,%a2u,>>,%a1@-,%a3,%acc1
    3568:	a6e1 a69d      	macw %a5l,%a2u,>>,%a1@-,%a3,%acc2
    356c:	a421 a68d      	macw %a5l,%a2u,>>,%a1@-,%d2,%acc1
    3570:	a4a1 a69d      	macw %a5l,%a2u,>>,%a1@-,%d2,%acc2
    3574:	ae61 a68d      	macw %a5l,%a2u,>>,%a1@-,%sp,%acc1
    3578:	aee1 a69d      	macw %a5l,%a2u,>>,%a1@-,%sp,%acc2
    357c:	a221 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d1,%acc1
    3580:	a2a1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%d1,%acc2
    3584:	a661 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%a3,%acc1
    3588:	a6e1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%a3,%acc2
    358c:	a421 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d2,%acc1
    3590:	a4a1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%d2,%acc2
    3594:	ae61 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%sp,%acc1
    3598:	aee1 a6bd      	macw %a5l,%a2u,>>,%a1@-&,%sp,%acc2
    359c:	a213 300d      	macw %a5l,%d3l,%a3@,%d1,%acc1
    35a0:	a293 301d      	macw %a5l,%d3l,%a3@,%d1,%acc2
    35a4:	a653 300d      	macw %a5l,%d3l,%a3@,%a3,%acc1
    35a8:	a6d3 301d      	macw %a5l,%d3l,%a3@,%a3,%acc2
    35ac:	a413 300d      	macw %a5l,%d3l,%a3@,%d2,%acc1
    35b0:	a493 301d      	macw %a5l,%d3l,%a3@,%d2,%acc2
    35b4:	ae53 300d      	macw %a5l,%d3l,%a3@,%sp,%acc1
    35b8:	aed3 301d      	macw %a5l,%d3l,%a3@,%sp,%acc2
    35bc:	a213 302d      	macw %a5l,%d3l,%a3@&,%d1,%acc1
    35c0:	a293 303d      	macw %a5l,%d3l,%a3@&,%d1,%acc2
    35c4:	a653 302d      	macw %a5l,%d3l,%a3@&,%a3,%acc1
    35c8:	a6d3 303d      	macw %a5l,%d3l,%a3@&,%a3,%acc2
    35cc:	a413 302d      	macw %a5l,%d3l,%a3@&,%d2,%acc1
    35d0:	a493 303d      	macw %a5l,%d3l,%a3@&,%d2,%acc2
    35d4:	ae53 302d      	macw %a5l,%d3l,%a3@&,%sp,%acc1
    35d8:	aed3 303d      	macw %a5l,%d3l,%a3@&,%sp,%acc2
    35dc:	a21a 300d      	macw %a5l,%d3l,%a2@\+,%d1,%acc1
    35e0:	a29a 301d      	macw %a5l,%d3l,%a2@\+,%d1,%acc2
    35e4:	a65a 300d      	macw %a5l,%d3l,%a2@\+,%a3,%acc1
    35e8:	a6da 301d      	macw %a5l,%d3l,%a2@\+,%a3,%acc2
    35ec:	a41a 300d      	macw %a5l,%d3l,%a2@\+,%d2,%acc1
    35f0:	a49a 301d      	macw %a5l,%d3l,%a2@\+,%d2,%acc2
    35f4:	ae5a 300d      	macw %a5l,%d3l,%a2@\+,%sp,%acc1
    35f8:	aeda 301d      	macw %a5l,%d3l,%a2@\+,%sp,%acc2
    35fc:	a21a 302d      	macw %a5l,%d3l,%a2@\+&,%d1,%acc1
    3600:	a29a 303d      	macw %a5l,%d3l,%a2@\+&,%d1,%acc2
    3604:	a65a 302d      	macw %a5l,%d3l,%a2@\+&,%a3,%acc1
    3608:	a6da 303d      	macw %a5l,%d3l,%a2@\+&,%a3,%acc2
    360c:	a41a 302d      	macw %a5l,%d3l,%a2@\+&,%d2,%acc1
    3610:	a49a 303d      	macw %a5l,%d3l,%a2@\+&,%d2,%acc2
    3614:	ae5a 302d      	macw %a5l,%d3l,%a2@\+&,%sp,%acc1
    3618:	aeda 303d      	macw %a5l,%d3l,%a2@\+&,%sp,%acc2
    361c:	a22e 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%d1,%acc1
    3622:	a2ae 301d 000a 	macw %a5l,%d3l,%fp@\(10\),%d1,%acc2
    3628:	a66e 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%a3,%acc1
    362e:	a6ee 301d 000a 	macw %a5l,%d3l,%fp@\(10\),%a3,%acc2
    3634:	a42e 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%d2,%acc1
    363a:	a4ae 301d 000a 	macw %a5l,%d3l,%fp@\(10\),%d2,%acc2
    3640:	ae6e 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%sp,%acc1
    3646:	aeee 301d 000a 	macw %a5l,%d3l,%fp@\(10\),%sp,%acc2
    364c:	a22e 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d1,%acc1
    3652:	a2ae 303d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d1,%acc2
    3658:	a66e 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%a3,%acc1
    365e:	a6ee 303d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%a3,%acc2
    3664:	a42e 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d2,%acc1
    366a:	a4ae 303d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d2,%acc2
    3670:	ae6e 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%sp,%acc1
    3676:	aeee 303d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%sp,%acc2
    367c:	a221 300d      	macw %a5l,%d3l,%a1@-,%d1,%acc1
    3680:	a2a1 301d      	macw %a5l,%d3l,%a1@-,%d1,%acc2
    3684:	a661 300d      	macw %a5l,%d3l,%a1@-,%a3,%acc1
    3688:	a6e1 301d      	macw %a5l,%d3l,%a1@-,%a3,%acc2
    368c:	a421 300d      	macw %a5l,%d3l,%a1@-,%d2,%acc1
    3690:	a4a1 301d      	macw %a5l,%d3l,%a1@-,%d2,%acc2
    3694:	ae61 300d      	macw %a5l,%d3l,%a1@-,%sp,%acc1
    3698:	aee1 301d      	macw %a5l,%d3l,%a1@-,%sp,%acc2
    369c:	a221 302d      	macw %a5l,%d3l,%a1@-&,%d1,%acc1
    36a0:	a2a1 303d      	macw %a5l,%d3l,%a1@-&,%d1,%acc2
    36a4:	a661 302d      	macw %a5l,%d3l,%a1@-&,%a3,%acc1
    36a8:	a6e1 303d      	macw %a5l,%d3l,%a1@-&,%a3,%acc2
    36ac:	a421 302d      	macw %a5l,%d3l,%a1@-&,%d2,%acc1
    36b0:	a4a1 303d      	macw %a5l,%d3l,%a1@-&,%d2,%acc2
    36b4:	ae61 302d      	macw %a5l,%d3l,%a1@-&,%sp,%acc1
    36b8:	aee1 303d      	macw %a5l,%d3l,%a1@-&,%sp,%acc2
    36bc:	a213 320d      	macw %a5l,%d3l,<<,%a3@,%d1,%acc1
    36c0:	a293 321d      	macw %a5l,%d3l,<<,%a3@,%d1,%acc2
    36c4:	a653 320d      	macw %a5l,%d3l,<<,%a3@,%a3,%acc1
    36c8:	a6d3 321d      	macw %a5l,%d3l,<<,%a3@,%a3,%acc2
    36cc:	a413 320d      	macw %a5l,%d3l,<<,%a3@,%d2,%acc1
    36d0:	a493 321d      	macw %a5l,%d3l,<<,%a3@,%d2,%acc2
    36d4:	ae53 320d      	macw %a5l,%d3l,<<,%a3@,%sp,%acc1
    36d8:	aed3 321d      	macw %a5l,%d3l,<<,%a3@,%sp,%acc2
    36dc:	a213 322d      	macw %a5l,%d3l,<<,%a3@&,%d1,%acc1
    36e0:	a293 323d      	macw %a5l,%d3l,<<,%a3@&,%d1,%acc2
    36e4:	a653 322d      	macw %a5l,%d3l,<<,%a3@&,%a3,%acc1
    36e8:	a6d3 323d      	macw %a5l,%d3l,<<,%a3@&,%a3,%acc2
    36ec:	a413 322d      	macw %a5l,%d3l,<<,%a3@&,%d2,%acc1
    36f0:	a493 323d      	macw %a5l,%d3l,<<,%a3@&,%d2,%acc2
    36f4:	ae53 322d      	macw %a5l,%d3l,<<,%a3@&,%sp,%acc1
    36f8:	aed3 323d      	macw %a5l,%d3l,<<,%a3@&,%sp,%acc2
    36fc:	a21a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d1,%acc1
    3700:	a29a 321d      	macw %a5l,%d3l,<<,%a2@\+,%d1,%acc2
    3704:	a65a 320d      	macw %a5l,%d3l,<<,%a2@\+,%a3,%acc1
    3708:	a6da 321d      	macw %a5l,%d3l,<<,%a2@\+,%a3,%acc2
    370c:	a41a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d2,%acc1
    3710:	a49a 321d      	macw %a5l,%d3l,<<,%a2@\+,%d2,%acc2
    3714:	ae5a 320d      	macw %a5l,%d3l,<<,%a2@\+,%sp,%acc1
    3718:	aeda 321d      	macw %a5l,%d3l,<<,%a2@\+,%sp,%acc2
    371c:	a21a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d1,%acc1
    3720:	a29a 323d      	macw %a5l,%d3l,<<,%a2@\+&,%d1,%acc2
    3724:	a65a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%a3,%acc1
    3728:	a6da 323d      	macw %a5l,%d3l,<<,%a2@\+&,%a3,%acc2
    372c:	a41a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d2,%acc1
    3730:	a49a 323d      	macw %a5l,%d3l,<<,%a2@\+&,%d2,%acc2
    3734:	ae5a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%sp,%acc1
    3738:	aeda 323d      	macw %a5l,%d3l,<<,%a2@\+&,%sp,%acc2
    373c:	a22e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1,%acc1
    3742:	a2ae 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1,%acc2
    3748:	a66e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3,%acc1
    374e:	a6ee 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3,%acc2
    3754:	a42e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2,%acc1
    375a:	a4ae 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2,%acc2
    3760:	ae6e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp,%acc1
    3766:	aeee 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp,%acc2
    376c:	a22e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    3772:	a2ae 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    3778:	a66e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    377e:	a6ee 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    3784:	a42e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    378a:	a4ae 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    3790:	ae6e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    3796:	aeee 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    379c:	a221 320d      	macw %a5l,%d3l,<<,%a1@-,%d1,%acc1
    37a0:	a2a1 321d      	macw %a5l,%d3l,<<,%a1@-,%d1,%acc2
    37a4:	a661 320d      	macw %a5l,%d3l,<<,%a1@-,%a3,%acc1
    37a8:	a6e1 321d      	macw %a5l,%d3l,<<,%a1@-,%a3,%acc2
    37ac:	a421 320d      	macw %a5l,%d3l,<<,%a1@-,%d2,%acc1
    37b0:	a4a1 321d      	macw %a5l,%d3l,<<,%a1@-,%d2,%acc2
    37b4:	ae61 320d      	macw %a5l,%d3l,<<,%a1@-,%sp,%acc1
    37b8:	aee1 321d      	macw %a5l,%d3l,<<,%a1@-,%sp,%acc2
    37bc:	a221 322d      	macw %a5l,%d3l,<<,%a1@-&,%d1,%acc1
    37c0:	a2a1 323d      	macw %a5l,%d3l,<<,%a1@-&,%d1,%acc2
    37c4:	a661 322d      	macw %a5l,%d3l,<<,%a1@-&,%a3,%acc1
    37c8:	a6e1 323d      	macw %a5l,%d3l,<<,%a1@-&,%a3,%acc2
    37cc:	a421 322d      	macw %a5l,%d3l,<<,%a1@-&,%d2,%acc1
    37d0:	a4a1 323d      	macw %a5l,%d3l,<<,%a1@-&,%d2,%acc2
    37d4:	ae61 322d      	macw %a5l,%d3l,<<,%a1@-&,%sp,%acc1
    37d8:	aee1 323d      	macw %a5l,%d3l,<<,%a1@-&,%sp,%acc2
    37dc:	a213 360d      	macw %a5l,%d3l,>>,%a3@,%d1,%acc1
    37e0:	a293 361d      	macw %a5l,%d3l,>>,%a3@,%d1,%acc2
    37e4:	a653 360d      	macw %a5l,%d3l,>>,%a3@,%a3,%acc1
    37e8:	a6d3 361d      	macw %a5l,%d3l,>>,%a3@,%a3,%acc2
    37ec:	a413 360d      	macw %a5l,%d3l,>>,%a3@,%d2,%acc1
    37f0:	a493 361d      	macw %a5l,%d3l,>>,%a3@,%d2,%acc2
    37f4:	ae53 360d      	macw %a5l,%d3l,>>,%a3@,%sp,%acc1
    37f8:	aed3 361d      	macw %a5l,%d3l,>>,%a3@,%sp,%acc2
    37fc:	a213 362d      	macw %a5l,%d3l,>>,%a3@&,%d1,%acc1
    3800:	a293 363d      	macw %a5l,%d3l,>>,%a3@&,%d1,%acc2
    3804:	a653 362d      	macw %a5l,%d3l,>>,%a3@&,%a3,%acc1
    3808:	a6d3 363d      	macw %a5l,%d3l,>>,%a3@&,%a3,%acc2
    380c:	a413 362d      	macw %a5l,%d3l,>>,%a3@&,%d2,%acc1
    3810:	a493 363d      	macw %a5l,%d3l,>>,%a3@&,%d2,%acc2
    3814:	ae53 362d      	macw %a5l,%d3l,>>,%a3@&,%sp,%acc1
    3818:	aed3 363d      	macw %a5l,%d3l,>>,%a3@&,%sp,%acc2
    381c:	a21a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d1,%acc1
    3820:	a29a 361d      	macw %a5l,%d3l,>>,%a2@\+,%d1,%acc2
    3824:	a65a 360d      	macw %a5l,%d3l,>>,%a2@\+,%a3,%acc1
    3828:	a6da 361d      	macw %a5l,%d3l,>>,%a2@\+,%a3,%acc2
    382c:	a41a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d2,%acc1
    3830:	a49a 361d      	macw %a5l,%d3l,>>,%a2@\+,%d2,%acc2
    3834:	ae5a 360d      	macw %a5l,%d3l,>>,%a2@\+,%sp,%acc1
    3838:	aeda 361d      	macw %a5l,%d3l,>>,%a2@\+,%sp,%acc2
    383c:	a21a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d1,%acc1
    3840:	a29a 363d      	macw %a5l,%d3l,>>,%a2@\+&,%d1,%acc2
    3844:	a65a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%a3,%acc1
    3848:	a6da 363d      	macw %a5l,%d3l,>>,%a2@\+&,%a3,%acc2
    384c:	a41a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d2,%acc1
    3850:	a49a 363d      	macw %a5l,%d3l,>>,%a2@\+&,%d2,%acc2
    3854:	ae5a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%sp,%acc1
    3858:	aeda 363d      	macw %a5l,%d3l,>>,%a2@\+&,%sp,%acc2
    385c:	a22e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1,%acc1
    3862:	a2ae 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1,%acc2
    3868:	a66e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3,%acc1
    386e:	a6ee 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3,%acc2
    3874:	a42e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2,%acc1
    387a:	a4ae 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2,%acc2
    3880:	ae6e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp,%acc1
    3886:	aeee 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp,%acc2
    388c:	a22e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    3892:	a2ae 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    3898:	a66e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    389e:	a6ee 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    38a4:	a42e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    38aa:	a4ae 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    38b0:	ae6e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    38b6:	aeee 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    38bc:	a221 360d      	macw %a5l,%d3l,>>,%a1@-,%d1,%acc1
    38c0:	a2a1 361d      	macw %a5l,%d3l,>>,%a1@-,%d1,%acc2
    38c4:	a661 360d      	macw %a5l,%d3l,>>,%a1@-,%a3,%acc1
    38c8:	a6e1 361d      	macw %a5l,%d3l,>>,%a1@-,%a3,%acc2
    38cc:	a421 360d      	macw %a5l,%d3l,>>,%a1@-,%d2,%acc1
    38d0:	a4a1 361d      	macw %a5l,%d3l,>>,%a1@-,%d2,%acc2
    38d4:	ae61 360d      	macw %a5l,%d3l,>>,%a1@-,%sp,%acc1
    38d8:	aee1 361d      	macw %a5l,%d3l,>>,%a1@-,%sp,%acc2
    38dc:	a221 362d      	macw %a5l,%d3l,>>,%a1@-&,%d1,%acc1
    38e0:	a2a1 363d      	macw %a5l,%d3l,>>,%a1@-&,%d1,%acc2
    38e4:	a661 362d      	macw %a5l,%d3l,>>,%a1@-&,%a3,%acc1
    38e8:	a6e1 363d      	macw %a5l,%d3l,>>,%a1@-&,%a3,%acc2
    38ec:	a421 362d      	macw %a5l,%d3l,>>,%a1@-&,%d2,%acc1
    38f0:	a4a1 363d      	macw %a5l,%d3l,>>,%a1@-&,%d2,%acc2
    38f4:	ae61 362d      	macw %a5l,%d3l,>>,%a1@-&,%sp,%acc1
    38f8:	aee1 363d      	macw %a5l,%d3l,>>,%a1@-&,%sp,%acc2
    38fc:	a213 320d      	macw %a5l,%d3l,<<,%a3@,%d1,%acc1
    3900:	a293 321d      	macw %a5l,%d3l,<<,%a3@,%d1,%acc2
    3904:	a653 320d      	macw %a5l,%d3l,<<,%a3@,%a3,%acc1
    3908:	a6d3 321d      	macw %a5l,%d3l,<<,%a3@,%a3,%acc2
    390c:	a413 320d      	macw %a5l,%d3l,<<,%a3@,%d2,%acc1
    3910:	a493 321d      	macw %a5l,%d3l,<<,%a3@,%d2,%acc2
    3914:	ae53 320d      	macw %a5l,%d3l,<<,%a3@,%sp,%acc1
    3918:	aed3 321d      	macw %a5l,%d3l,<<,%a3@,%sp,%acc2
    391c:	a213 322d      	macw %a5l,%d3l,<<,%a3@&,%d1,%acc1
    3920:	a293 323d      	macw %a5l,%d3l,<<,%a3@&,%d1,%acc2
    3924:	a653 322d      	macw %a5l,%d3l,<<,%a3@&,%a3,%acc1
    3928:	a6d3 323d      	macw %a5l,%d3l,<<,%a3@&,%a3,%acc2
    392c:	a413 322d      	macw %a5l,%d3l,<<,%a3@&,%d2,%acc1
    3930:	a493 323d      	macw %a5l,%d3l,<<,%a3@&,%d2,%acc2
    3934:	ae53 322d      	macw %a5l,%d3l,<<,%a3@&,%sp,%acc1
    3938:	aed3 323d      	macw %a5l,%d3l,<<,%a3@&,%sp,%acc2
    393c:	a21a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d1,%acc1
    3940:	a29a 321d      	macw %a5l,%d3l,<<,%a2@\+,%d1,%acc2
    3944:	a65a 320d      	macw %a5l,%d3l,<<,%a2@\+,%a3,%acc1
    3948:	a6da 321d      	macw %a5l,%d3l,<<,%a2@\+,%a3,%acc2
    394c:	a41a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d2,%acc1
    3950:	a49a 321d      	macw %a5l,%d3l,<<,%a2@\+,%d2,%acc2
    3954:	ae5a 320d      	macw %a5l,%d3l,<<,%a2@\+,%sp,%acc1
    3958:	aeda 321d      	macw %a5l,%d3l,<<,%a2@\+,%sp,%acc2
    395c:	a21a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d1,%acc1
    3960:	a29a 323d      	macw %a5l,%d3l,<<,%a2@\+&,%d1,%acc2
    3964:	a65a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%a3,%acc1
    3968:	a6da 323d      	macw %a5l,%d3l,<<,%a2@\+&,%a3,%acc2
    396c:	a41a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d2,%acc1
    3970:	a49a 323d      	macw %a5l,%d3l,<<,%a2@\+&,%d2,%acc2
    3974:	ae5a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%sp,%acc1
    3978:	aeda 323d      	macw %a5l,%d3l,<<,%a2@\+&,%sp,%acc2
    397c:	a22e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1,%acc1
    3982:	a2ae 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1,%acc2
    3988:	a66e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3,%acc1
    398e:	a6ee 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3,%acc2
    3994:	a42e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2,%acc1
    399a:	a4ae 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2,%acc2
    39a0:	ae6e 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp,%acc1
    39a6:	aeee 321d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp,%acc2
    39ac:	a22e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    39b2:	a2ae 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    39b8:	a66e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    39be:	a6ee 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    39c4:	a42e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    39ca:	a4ae 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    39d0:	ae6e 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    39d6:	aeee 323d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    39dc:	a221 320d      	macw %a5l,%d3l,<<,%a1@-,%d1,%acc1
    39e0:	a2a1 321d      	macw %a5l,%d3l,<<,%a1@-,%d1,%acc2
    39e4:	a661 320d      	macw %a5l,%d3l,<<,%a1@-,%a3,%acc1
    39e8:	a6e1 321d      	macw %a5l,%d3l,<<,%a1@-,%a3,%acc2
    39ec:	a421 320d      	macw %a5l,%d3l,<<,%a1@-,%d2,%acc1
    39f0:	a4a1 321d      	macw %a5l,%d3l,<<,%a1@-,%d2,%acc2
    39f4:	ae61 320d      	macw %a5l,%d3l,<<,%a1@-,%sp,%acc1
    39f8:	aee1 321d      	macw %a5l,%d3l,<<,%a1@-,%sp,%acc2
    39fc:	a221 322d      	macw %a5l,%d3l,<<,%a1@-&,%d1,%acc1
    3a00:	a2a1 323d      	macw %a5l,%d3l,<<,%a1@-&,%d1,%acc2
    3a04:	a661 322d      	macw %a5l,%d3l,<<,%a1@-&,%a3,%acc1
    3a08:	a6e1 323d      	macw %a5l,%d3l,<<,%a1@-&,%a3,%acc2
    3a0c:	a421 322d      	macw %a5l,%d3l,<<,%a1@-&,%d2,%acc1
    3a10:	a4a1 323d      	macw %a5l,%d3l,<<,%a1@-&,%d2,%acc2
    3a14:	ae61 322d      	macw %a5l,%d3l,<<,%a1@-&,%sp,%acc1
    3a18:	aee1 323d      	macw %a5l,%d3l,<<,%a1@-&,%sp,%acc2
    3a1c:	a213 360d      	macw %a5l,%d3l,>>,%a3@,%d1,%acc1
    3a20:	a293 361d      	macw %a5l,%d3l,>>,%a3@,%d1,%acc2
    3a24:	a653 360d      	macw %a5l,%d3l,>>,%a3@,%a3,%acc1
    3a28:	a6d3 361d      	macw %a5l,%d3l,>>,%a3@,%a3,%acc2
    3a2c:	a413 360d      	macw %a5l,%d3l,>>,%a3@,%d2,%acc1
    3a30:	a493 361d      	macw %a5l,%d3l,>>,%a3@,%d2,%acc2
    3a34:	ae53 360d      	macw %a5l,%d3l,>>,%a3@,%sp,%acc1
    3a38:	aed3 361d      	macw %a5l,%d3l,>>,%a3@,%sp,%acc2
    3a3c:	a213 362d      	macw %a5l,%d3l,>>,%a3@&,%d1,%acc1
    3a40:	a293 363d      	macw %a5l,%d3l,>>,%a3@&,%d1,%acc2
    3a44:	a653 362d      	macw %a5l,%d3l,>>,%a3@&,%a3,%acc1
    3a48:	a6d3 363d      	macw %a5l,%d3l,>>,%a3@&,%a3,%acc2
    3a4c:	a413 362d      	macw %a5l,%d3l,>>,%a3@&,%d2,%acc1
    3a50:	a493 363d      	macw %a5l,%d3l,>>,%a3@&,%d2,%acc2
    3a54:	ae53 362d      	macw %a5l,%d3l,>>,%a3@&,%sp,%acc1
    3a58:	aed3 363d      	macw %a5l,%d3l,>>,%a3@&,%sp,%acc2
    3a5c:	a21a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d1,%acc1
    3a60:	a29a 361d      	macw %a5l,%d3l,>>,%a2@\+,%d1,%acc2
    3a64:	a65a 360d      	macw %a5l,%d3l,>>,%a2@\+,%a3,%acc1
    3a68:	a6da 361d      	macw %a5l,%d3l,>>,%a2@\+,%a3,%acc2
    3a6c:	a41a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d2,%acc1
    3a70:	a49a 361d      	macw %a5l,%d3l,>>,%a2@\+,%d2,%acc2
    3a74:	ae5a 360d      	macw %a5l,%d3l,>>,%a2@\+,%sp,%acc1
    3a78:	aeda 361d      	macw %a5l,%d3l,>>,%a2@\+,%sp,%acc2
    3a7c:	a21a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d1,%acc1
    3a80:	a29a 363d      	macw %a5l,%d3l,>>,%a2@\+&,%d1,%acc2
    3a84:	a65a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%a3,%acc1
    3a88:	a6da 363d      	macw %a5l,%d3l,>>,%a2@\+&,%a3,%acc2
    3a8c:	a41a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d2,%acc1
    3a90:	a49a 363d      	macw %a5l,%d3l,>>,%a2@\+&,%d2,%acc2
    3a94:	ae5a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%sp,%acc1
    3a98:	aeda 363d      	macw %a5l,%d3l,>>,%a2@\+&,%sp,%acc2
    3a9c:	a22e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1,%acc1
    3aa2:	a2ae 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1,%acc2
    3aa8:	a66e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3,%acc1
    3aae:	a6ee 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3,%acc2
    3ab4:	a42e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2,%acc1
    3aba:	a4ae 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2,%acc2
    3ac0:	ae6e 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp,%acc1
    3ac6:	aeee 361d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp,%acc2
    3acc:	a22e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    3ad2:	a2ae 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    3ad8:	a66e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    3ade:	a6ee 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    3ae4:	a42e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    3aea:	a4ae 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    3af0:	ae6e 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    3af6:	aeee 363d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    3afc:	a221 360d      	macw %a5l,%d3l,>>,%a1@-,%d1,%acc1
    3b00:	a2a1 361d      	macw %a5l,%d3l,>>,%a1@-,%d1,%acc2
    3b04:	a661 360d      	macw %a5l,%d3l,>>,%a1@-,%a3,%acc1
    3b08:	a6e1 361d      	macw %a5l,%d3l,>>,%a1@-,%a3,%acc2
    3b0c:	a421 360d      	macw %a5l,%d3l,>>,%a1@-,%d2,%acc1
    3b10:	a4a1 361d      	macw %a5l,%d3l,>>,%a1@-,%d2,%acc2
    3b14:	ae61 360d      	macw %a5l,%d3l,>>,%a1@-,%sp,%acc1
    3b18:	aee1 361d      	macw %a5l,%d3l,>>,%a1@-,%sp,%acc2
    3b1c:	a221 362d      	macw %a5l,%d3l,>>,%a1@-&,%d1,%acc1
    3b20:	a2a1 363d      	macw %a5l,%d3l,>>,%a1@-&,%d1,%acc2
    3b24:	a661 362d      	macw %a5l,%d3l,>>,%a1@-&,%a3,%acc1
    3b28:	a6e1 363d      	macw %a5l,%d3l,>>,%a1@-&,%a3,%acc2
    3b2c:	a421 362d      	macw %a5l,%d3l,>>,%a1@-&,%d2,%acc1
    3b30:	a4a1 363d      	macw %a5l,%d3l,>>,%a1@-&,%d2,%acc2
    3b34:	ae61 362d      	macw %a5l,%d3l,>>,%a1@-&,%sp,%acc1
    3b38:	aee1 363d      	macw %a5l,%d3l,>>,%a1@-&,%sp,%acc2
    3b3c:	a213 f08d      	macw %a5l,%a7u,%a3@,%d1,%acc1
    3b40:	a293 f09d      	macw %a5l,%a7u,%a3@,%d1,%acc2
    3b44:	a653 f08d      	macw %a5l,%a7u,%a3@,%a3,%acc1
    3b48:	a6d3 f09d      	macw %a5l,%a7u,%a3@,%a3,%acc2
    3b4c:	a413 f08d      	macw %a5l,%a7u,%a3@,%d2,%acc1
    3b50:	a493 f09d      	macw %a5l,%a7u,%a3@,%d2,%acc2
    3b54:	ae53 f08d      	macw %a5l,%a7u,%a3@,%sp,%acc1
    3b58:	aed3 f09d      	macw %a5l,%a7u,%a3@,%sp,%acc2
    3b5c:	a213 f0ad      	macw %a5l,%a7u,%a3@&,%d1,%acc1
    3b60:	a293 f0bd      	macw %a5l,%a7u,%a3@&,%d1,%acc2
    3b64:	a653 f0ad      	macw %a5l,%a7u,%a3@&,%a3,%acc1
    3b68:	a6d3 f0bd      	macw %a5l,%a7u,%a3@&,%a3,%acc2
    3b6c:	a413 f0ad      	macw %a5l,%a7u,%a3@&,%d2,%acc1
    3b70:	a493 f0bd      	macw %a5l,%a7u,%a3@&,%d2,%acc2
    3b74:	ae53 f0ad      	macw %a5l,%a7u,%a3@&,%sp,%acc1
    3b78:	aed3 f0bd      	macw %a5l,%a7u,%a3@&,%sp,%acc2
    3b7c:	a21a f08d      	macw %a5l,%a7u,%a2@\+,%d1,%acc1
    3b80:	a29a f09d      	macw %a5l,%a7u,%a2@\+,%d1,%acc2
    3b84:	a65a f08d      	macw %a5l,%a7u,%a2@\+,%a3,%acc1
    3b88:	a6da f09d      	macw %a5l,%a7u,%a2@\+,%a3,%acc2
    3b8c:	a41a f08d      	macw %a5l,%a7u,%a2@\+,%d2,%acc1
    3b90:	a49a f09d      	macw %a5l,%a7u,%a2@\+,%d2,%acc2
    3b94:	ae5a f08d      	macw %a5l,%a7u,%a2@\+,%sp,%acc1
    3b98:	aeda f09d      	macw %a5l,%a7u,%a2@\+,%sp,%acc2
    3b9c:	a21a f0ad      	macw %a5l,%a7u,%a2@\+&,%d1,%acc1
    3ba0:	a29a f0bd      	macw %a5l,%a7u,%a2@\+&,%d1,%acc2
    3ba4:	a65a f0ad      	macw %a5l,%a7u,%a2@\+&,%a3,%acc1
    3ba8:	a6da f0bd      	macw %a5l,%a7u,%a2@\+&,%a3,%acc2
    3bac:	a41a f0ad      	macw %a5l,%a7u,%a2@\+&,%d2,%acc1
    3bb0:	a49a f0bd      	macw %a5l,%a7u,%a2@\+&,%d2,%acc2
    3bb4:	ae5a f0ad      	macw %a5l,%a7u,%a2@\+&,%sp,%acc1
    3bb8:	aeda f0bd      	macw %a5l,%a7u,%a2@\+&,%sp,%acc2
    3bbc:	a22e f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%d1,%acc1
    3bc2:	a2ae f09d 000a 	macw %a5l,%a7u,%fp@\(10\),%d1,%acc2
    3bc8:	a66e f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%a3,%acc1
    3bce:	a6ee f09d 000a 	macw %a5l,%a7u,%fp@\(10\),%a3,%acc2
    3bd4:	a42e f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%d2,%acc1
    3bda:	a4ae f09d 000a 	macw %a5l,%a7u,%fp@\(10\),%d2,%acc2
    3be0:	ae6e f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%sp,%acc1
    3be6:	aeee f09d 000a 	macw %a5l,%a7u,%fp@\(10\),%sp,%acc2
    3bec:	a22e f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d1,%acc1
    3bf2:	a2ae f0bd 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d1,%acc2
    3bf8:	a66e f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%a3,%acc1
    3bfe:	a6ee f0bd 000a 	macw %a5l,%a7u,%fp@\(10\)&,%a3,%acc2
    3c04:	a42e f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d2,%acc1
    3c0a:	a4ae f0bd 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d2,%acc2
    3c10:	ae6e f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%sp,%acc1
    3c16:	aeee f0bd 000a 	macw %a5l,%a7u,%fp@\(10\)&,%sp,%acc2
    3c1c:	a221 f08d      	macw %a5l,%a7u,%a1@-,%d1,%acc1
    3c20:	a2a1 f09d      	macw %a5l,%a7u,%a1@-,%d1,%acc2
    3c24:	a661 f08d      	macw %a5l,%a7u,%a1@-,%a3,%acc1
    3c28:	a6e1 f09d      	macw %a5l,%a7u,%a1@-,%a3,%acc2
    3c2c:	a421 f08d      	macw %a5l,%a7u,%a1@-,%d2,%acc1
    3c30:	a4a1 f09d      	macw %a5l,%a7u,%a1@-,%d2,%acc2
    3c34:	ae61 f08d      	macw %a5l,%a7u,%a1@-,%sp,%acc1
    3c38:	aee1 f09d      	macw %a5l,%a7u,%a1@-,%sp,%acc2
    3c3c:	a221 f0ad      	macw %a5l,%a7u,%a1@-&,%d1,%acc1
    3c40:	a2a1 f0bd      	macw %a5l,%a7u,%a1@-&,%d1,%acc2
    3c44:	a661 f0ad      	macw %a5l,%a7u,%a1@-&,%a3,%acc1
    3c48:	a6e1 f0bd      	macw %a5l,%a7u,%a1@-&,%a3,%acc2
    3c4c:	a421 f0ad      	macw %a5l,%a7u,%a1@-&,%d2,%acc1
    3c50:	a4a1 f0bd      	macw %a5l,%a7u,%a1@-&,%d2,%acc2
    3c54:	ae61 f0ad      	macw %a5l,%a7u,%a1@-&,%sp,%acc1
    3c58:	aee1 f0bd      	macw %a5l,%a7u,%a1@-&,%sp,%acc2
    3c5c:	a213 f28d      	macw %a5l,%a7u,<<,%a3@,%d1,%acc1
    3c60:	a293 f29d      	macw %a5l,%a7u,<<,%a3@,%d1,%acc2
    3c64:	a653 f28d      	macw %a5l,%a7u,<<,%a3@,%a3,%acc1
    3c68:	a6d3 f29d      	macw %a5l,%a7u,<<,%a3@,%a3,%acc2
    3c6c:	a413 f28d      	macw %a5l,%a7u,<<,%a3@,%d2,%acc1
    3c70:	a493 f29d      	macw %a5l,%a7u,<<,%a3@,%d2,%acc2
    3c74:	ae53 f28d      	macw %a5l,%a7u,<<,%a3@,%sp,%acc1
    3c78:	aed3 f29d      	macw %a5l,%a7u,<<,%a3@,%sp,%acc2
    3c7c:	a213 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d1,%acc1
    3c80:	a293 f2bd      	macw %a5l,%a7u,<<,%a3@&,%d1,%acc2
    3c84:	a653 f2ad      	macw %a5l,%a7u,<<,%a3@&,%a3,%acc1
    3c88:	a6d3 f2bd      	macw %a5l,%a7u,<<,%a3@&,%a3,%acc2
    3c8c:	a413 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d2,%acc1
    3c90:	a493 f2bd      	macw %a5l,%a7u,<<,%a3@&,%d2,%acc2
    3c94:	ae53 f2ad      	macw %a5l,%a7u,<<,%a3@&,%sp,%acc1
    3c98:	aed3 f2bd      	macw %a5l,%a7u,<<,%a3@&,%sp,%acc2
    3c9c:	a21a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d1,%acc1
    3ca0:	a29a f29d      	macw %a5l,%a7u,<<,%a2@\+,%d1,%acc2
    3ca4:	a65a f28d      	macw %a5l,%a7u,<<,%a2@\+,%a3,%acc1
    3ca8:	a6da f29d      	macw %a5l,%a7u,<<,%a2@\+,%a3,%acc2
    3cac:	a41a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d2,%acc1
    3cb0:	a49a f29d      	macw %a5l,%a7u,<<,%a2@\+,%d2,%acc2
    3cb4:	ae5a f28d      	macw %a5l,%a7u,<<,%a2@\+,%sp,%acc1
    3cb8:	aeda f29d      	macw %a5l,%a7u,<<,%a2@\+,%sp,%acc2
    3cbc:	a21a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d1,%acc1
    3cc0:	a29a f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%d1,%acc2
    3cc4:	a65a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%a3,%acc1
    3cc8:	a6da f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%a3,%acc2
    3ccc:	a41a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d2,%acc1
    3cd0:	a49a f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%d2,%acc2
    3cd4:	ae5a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%sp,%acc1
    3cd8:	aeda f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%sp,%acc2
    3cdc:	a22e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1,%acc1
    3ce2:	a2ae f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1,%acc2
    3ce8:	a66e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3,%acc1
    3cee:	a6ee f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3,%acc2
    3cf4:	a42e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2,%acc1
    3cfa:	a4ae f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2,%acc2
    3d00:	ae6e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp,%acc1
    3d06:	aeee f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp,%acc2
    3d0c:	a22e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    3d12:	a2ae f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    3d18:	a66e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    3d1e:	a6ee f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    3d24:	a42e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    3d2a:	a4ae f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    3d30:	ae6e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    3d36:	aeee f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    3d3c:	a221 f28d      	macw %a5l,%a7u,<<,%a1@-,%d1,%acc1
    3d40:	a2a1 f29d      	macw %a5l,%a7u,<<,%a1@-,%d1,%acc2
    3d44:	a661 f28d      	macw %a5l,%a7u,<<,%a1@-,%a3,%acc1
    3d48:	a6e1 f29d      	macw %a5l,%a7u,<<,%a1@-,%a3,%acc2
    3d4c:	a421 f28d      	macw %a5l,%a7u,<<,%a1@-,%d2,%acc1
    3d50:	a4a1 f29d      	macw %a5l,%a7u,<<,%a1@-,%d2,%acc2
    3d54:	ae61 f28d      	macw %a5l,%a7u,<<,%a1@-,%sp,%acc1
    3d58:	aee1 f29d      	macw %a5l,%a7u,<<,%a1@-,%sp,%acc2
    3d5c:	a221 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d1,%acc1
    3d60:	a2a1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%d1,%acc2
    3d64:	a661 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%a3,%acc1
    3d68:	a6e1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%a3,%acc2
    3d6c:	a421 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d2,%acc1
    3d70:	a4a1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%d2,%acc2
    3d74:	ae61 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%sp,%acc1
    3d78:	aee1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%sp,%acc2
    3d7c:	a213 f68d      	macw %a5l,%a7u,>>,%a3@,%d1,%acc1
    3d80:	a293 f69d      	macw %a5l,%a7u,>>,%a3@,%d1,%acc2
    3d84:	a653 f68d      	macw %a5l,%a7u,>>,%a3@,%a3,%acc1
    3d88:	a6d3 f69d      	macw %a5l,%a7u,>>,%a3@,%a3,%acc2
    3d8c:	a413 f68d      	macw %a5l,%a7u,>>,%a3@,%d2,%acc1
    3d90:	a493 f69d      	macw %a5l,%a7u,>>,%a3@,%d2,%acc2
    3d94:	ae53 f68d      	macw %a5l,%a7u,>>,%a3@,%sp,%acc1
    3d98:	aed3 f69d      	macw %a5l,%a7u,>>,%a3@,%sp,%acc2
    3d9c:	a213 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d1,%acc1
    3da0:	a293 f6bd      	macw %a5l,%a7u,>>,%a3@&,%d1,%acc2
    3da4:	a653 f6ad      	macw %a5l,%a7u,>>,%a3@&,%a3,%acc1
    3da8:	a6d3 f6bd      	macw %a5l,%a7u,>>,%a3@&,%a3,%acc2
    3dac:	a413 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d2,%acc1
    3db0:	a493 f6bd      	macw %a5l,%a7u,>>,%a3@&,%d2,%acc2
    3db4:	ae53 f6ad      	macw %a5l,%a7u,>>,%a3@&,%sp,%acc1
    3db8:	aed3 f6bd      	macw %a5l,%a7u,>>,%a3@&,%sp,%acc2
    3dbc:	a21a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d1,%acc1
    3dc0:	a29a f69d      	macw %a5l,%a7u,>>,%a2@\+,%d1,%acc2
    3dc4:	a65a f68d      	macw %a5l,%a7u,>>,%a2@\+,%a3,%acc1
    3dc8:	a6da f69d      	macw %a5l,%a7u,>>,%a2@\+,%a3,%acc2
    3dcc:	a41a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d2,%acc1
    3dd0:	a49a f69d      	macw %a5l,%a7u,>>,%a2@\+,%d2,%acc2
    3dd4:	ae5a f68d      	macw %a5l,%a7u,>>,%a2@\+,%sp,%acc1
    3dd8:	aeda f69d      	macw %a5l,%a7u,>>,%a2@\+,%sp,%acc2
    3ddc:	a21a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d1,%acc1
    3de0:	a29a f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%d1,%acc2
    3de4:	a65a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%a3,%acc1
    3de8:	a6da f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%a3,%acc2
    3dec:	a41a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d2,%acc1
    3df0:	a49a f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%d2,%acc2
    3df4:	ae5a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%sp,%acc1
    3df8:	aeda f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%sp,%acc2
    3dfc:	a22e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1,%acc1
    3e02:	a2ae f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1,%acc2
    3e08:	a66e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3,%acc1
    3e0e:	a6ee f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3,%acc2
    3e14:	a42e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2,%acc1
    3e1a:	a4ae f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2,%acc2
    3e20:	ae6e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp,%acc1
    3e26:	aeee f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp,%acc2
    3e2c:	a22e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    3e32:	a2ae f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    3e38:	a66e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    3e3e:	a6ee f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    3e44:	a42e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    3e4a:	a4ae f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    3e50:	ae6e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    3e56:	aeee f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    3e5c:	a221 f68d      	macw %a5l,%a7u,>>,%a1@-,%d1,%acc1
    3e60:	a2a1 f69d      	macw %a5l,%a7u,>>,%a1@-,%d1,%acc2
    3e64:	a661 f68d      	macw %a5l,%a7u,>>,%a1@-,%a3,%acc1
    3e68:	a6e1 f69d      	macw %a5l,%a7u,>>,%a1@-,%a3,%acc2
    3e6c:	a421 f68d      	macw %a5l,%a7u,>>,%a1@-,%d2,%acc1
    3e70:	a4a1 f69d      	macw %a5l,%a7u,>>,%a1@-,%d2,%acc2
    3e74:	ae61 f68d      	macw %a5l,%a7u,>>,%a1@-,%sp,%acc1
    3e78:	aee1 f69d      	macw %a5l,%a7u,>>,%a1@-,%sp,%acc2
    3e7c:	a221 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d1,%acc1
    3e80:	a2a1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%d1,%acc2
    3e84:	a661 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%a3,%acc1
    3e88:	a6e1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%a3,%acc2
    3e8c:	a421 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d2,%acc1
    3e90:	a4a1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%d2,%acc2
    3e94:	ae61 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%sp,%acc1
    3e98:	aee1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%sp,%acc2
    3e9c:	a213 f28d      	macw %a5l,%a7u,<<,%a3@,%d1,%acc1
    3ea0:	a293 f29d      	macw %a5l,%a7u,<<,%a3@,%d1,%acc2
    3ea4:	a653 f28d      	macw %a5l,%a7u,<<,%a3@,%a3,%acc1
    3ea8:	a6d3 f29d      	macw %a5l,%a7u,<<,%a3@,%a3,%acc2
    3eac:	a413 f28d      	macw %a5l,%a7u,<<,%a3@,%d2,%acc1
    3eb0:	a493 f29d      	macw %a5l,%a7u,<<,%a3@,%d2,%acc2
    3eb4:	ae53 f28d      	macw %a5l,%a7u,<<,%a3@,%sp,%acc1
    3eb8:	aed3 f29d      	macw %a5l,%a7u,<<,%a3@,%sp,%acc2
    3ebc:	a213 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d1,%acc1
    3ec0:	a293 f2bd      	macw %a5l,%a7u,<<,%a3@&,%d1,%acc2
    3ec4:	a653 f2ad      	macw %a5l,%a7u,<<,%a3@&,%a3,%acc1
    3ec8:	a6d3 f2bd      	macw %a5l,%a7u,<<,%a3@&,%a3,%acc2
    3ecc:	a413 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d2,%acc1
    3ed0:	a493 f2bd      	macw %a5l,%a7u,<<,%a3@&,%d2,%acc2
    3ed4:	ae53 f2ad      	macw %a5l,%a7u,<<,%a3@&,%sp,%acc1
    3ed8:	aed3 f2bd      	macw %a5l,%a7u,<<,%a3@&,%sp,%acc2
    3edc:	a21a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d1,%acc1
    3ee0:	a29a f29d      	macw %a5l,%a7u,<<,%a2@\+,%d1,%acc2
    3ee4:	a65a f28d      	macw %a5l,%a7u,<<,%a2@\+,%a3,%acc1
    3ee8:	a6da f29d      	macw %a5l,%a7u,<<,%a2@\+,%a3,%acc2
    3eec:	a41a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d2,%acc1
    3ef0:	a49a f29d      	macw %a5l,%a7u,<<,%a2@\+,%d2,%acc2
    3ef4:	ae5a f28d      	macw %a5l,%a7u,<<,%a2@\+,%sp,%acc1
    3ef8:	aeda f29d      	macw %a5l,%a7u,<<,%a2@\+,%sp,%acc2
    3efc:	a21a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d1,%acc1
    3f00:	a29a f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%d1,%acc2
    3f04:	a65a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%a3,%acc1
    3f08:	a6da f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%a3,%acc2
    3f0c:	a41a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d2,%acc1
    3f10:	a49a f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%d2,%acc2
    3f14:	ae5a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%sp,%acc1
    3f18:	aeda f2bd      	macw %a5l,%a7u,<<,%a2@\+&,%sp,%acc2
    3f1c:	a22e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1,%acc1
    3f22:	a2ae f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1,%acc2
    3f28:	a66e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3,%acc1
    3f2e:	a6ee f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3,%acc2
    3f34:	a42e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2,%acc1
    3f3a:	a4ae f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2,%acc2
    3f40:	ae6e f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp,%acc1
    3f46:	aeee f29d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp,%acc2
    3f4c:	a22e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    3f52:	a2ae f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    3f58:	a66e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    3f5e:	a6ee f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    3f64:	a42e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    3f6a:	a4ae f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    3f70:	ae6e f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    3f76:	aeee f2bd 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    3f7c:	a221 f28d      	macw %a5l,%a7u,<<,%a1@-,%d1,%acc1
    3f80:	a2a1 f29d      	macw %a5l,%a7u,<<,%a1@-,%d1,%acc2
    3f84:	a661 f28d      	macw %a5l,%a7u,<<,%a1@-,%a3,%acc1
    3f88:	a6e1 f29d      	macw %a5l,%a7u,<<,%a1@-,%a3,%acc2
    3f8c:	a421 f28d      	macw %a5l,%a7u,<<,%a1@-,%d2,%acc1
    3f90:	a4a1 f29d      	macw %a5l,%a7u,<<,%a1@-,%d2,%acc2
    3f94:	ae61 f28d      	macw %a5l,%a7u,<<,%a1@-,%sp,%acc1
    3f98:	aee1 f29d      	macw %a5l,%a7u,<<,%a1@-,%sp,%acc2
    3f9c:	a221 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d1,%acc1
    3fa0:	a2a1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%d1,%acc2
    3fa4:	a661 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%a3,%acc1
    3fa8:	a6e1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%a3,%acc2
    3fac:	a421 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d2,%acc1
    3fb0:	a4a1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%d2,%acc2
    3fb4:	ae61 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%sp,%acc1
    3fb8:	aee1 f2bd      	macw %a5l,%a7u,<<,%a1@-&,%sp,%acc2
    3fbc:	a213 f68d      	macw %a5l,%a7u,>>,%a3@,%d1,%acc1
    3fc0:	a293 f69d      	macw %a5l,%a7u,>>,%a3@,%d1,%acc2
    3fc4:	a653 f68d      	macw %a5l,%a7u,>>,%a3@,%a3,%acc1
    3fc8:	a6d3 f69d      	macw %a5l,%a7u,>>,%a3@,%a3,%acc2
    3fcc:	a413 f68d      	macw %a5l,%a7u,>>,%a3@,%d2,%acc1
    3fd0:	a493 f69d      	macw %a5l,%a7u,>>,%a3@,%d2,%acc2
    3fd4:	ae53 f68d      	macw %a5l,%a7u,>>,%a3@,%sp,%acc1
    3fd8:	aed3 f69d      	macw %a5l,%a7u,>>,%a3@,%sp,%acc2
    3fdc:	a213 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d1,%acc1
    3fe0:	a293 f6bd      	macw %a5l,%a7u,>>,%a3@&,%d1,%acc2
    3fe4:	a653 f6ad      	macw %a5l,%a7u,>>,%a3@&,%a3,%acc1
    3fe8:	a6d3 f6bd      	macw %a5l,%a7u,>>,%a3@&,%a3,%acc2
    3fec:	a413 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d2,%acc1
    3ff0:	a493 f6bd      	macw %a5l,%a7u,>>,%a3@&,%d2,%acc2
    3ff4:	ae53 f6ad      	macw %a5l,%a7u,>>,%a3@&,%sp,%acc1
    3ff8:	aed3 f6bd      	macw %a5l,%a7u,>>,%a3@&,%sp,%acc2
    3ffc:	a21a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d1,%acc1
    4000:	a29a f69d      	macw %a5l,%a7u,>>,%a2@\+,%d1,%acc2
    4004:	a65a f68d      	macw %a5l,%a7u,>>,%a2@\+,%a3,%acc1
    4008:	a6da f69d      	macw %a5l,%a7u,>>,%a2@\+,%a3,%acc2
    400c:	a41a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d2,%acc1
    4010:	a49a f69d      	macw %a5l,%a7u,>>,%a2@\+,%d2,%acc2
    4014:	ae5a f68d      	macw %a5l,%a7u,>>,%a2@\+,%sp,%acc1
    4018:	aeda f69d      	macw %a5l,%a7u,>>,%a2@\+,%sp,%acc2
    401c:	a21a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d1,%acc1
    4020:	a29a f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%d1,%acc2
    4024:	a65a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%a3,%acc1
    4028:	a6da f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%a3,%acc2
    402c:	a41a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d2,%acc1
    4030:	a49a f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%d2,%acc2
    4034:	ae5a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%sp,%acc1
    4038:	aeda f6bd      	macw %a5l,%a7u,>>,%a2@\+&,%sp,%acc2
    403c:	a22e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1,%acc1
    4042:	a2ae f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1,%acc2
    4048:	a66e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3,%acc1
    404e:	a6ee f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3,%acc2
    4054:	a42e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2,%acc1
    405a:	a4ae f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2,%acc2
    4060:	ae6e f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp,%acc1
    4066:	aeee f69d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp,%acc2
    406c:	a22e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    4072:	a2ae f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    4078:	a66e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    407e:	a6ee f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    4084:	a42e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    408a:	a4ae f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    4090:	ae6e f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    4096:	aeee f6bd 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    409c:	a221 f68d      	macw %a5l,%a7u,>>,%a1@-,%d1,%acc1
    40a0:	a2a1 f69d      	macw %a5l,%a7u,>>,%a1@-,%d1,%acc2
    40a4:	a661 f68d      	macw %a5l,%a7u,>>,%a1@-,%a3,%acc1
    40a8:	a6e1 f69d      	macw %a5l,%a7u,>>,%a1@-,%a3,%acc2
    40ac:	a421 f68d      	macw %a5l,%a7u,>>,%a1@-,%d2,%acc1
    40b0:	a4a1 f69d      	macw %a5l,%a7u,>>,%a1@-,%d2,%acc2
    40b4:	ae61 f68d      	macw %a5l,%a7u,>>,%a1@-,%sp,%acc1
    40b8:	aee1 f69d      	macw %a5l,%a7u,>>,%a1@-,%sp,%acc2
    40bc:	a221 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d1,%acc1
    40c0:	a2a1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%d1,%acc2
    40c4:	a661 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%a3,%acc1
    40c8:	a6e1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%a3,%acc2
    40cc:	a421 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d2,%acc1
    40d0:	a4a1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%d2,%acc2
    40d4:	ae61 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%sp,%acc1
    40d8:	aee1 f6bd      	macw %a5l,%a7u,>>,%a1@-&,%sp,%acc2
    40dc:	a213 100d      	macw %a5l,%d1l,%a3@,%d1,%acc1
    40e0:	a293 101d      	macw %a5l,%d1l,%a3@,%d1,%acc2
    40e4:	a653 100d      	macw %a5l,%d1l,%a3@,%a3,%acc1
    40e8:	a6d3 101d      	macw %a5l,%d1l,%a3@,%a3,%acc2
    40ec:	a413 100d      	macw %a5l,%d1l,%a3@,%d2,%acc1
    40f0:	a493 101d      	macw %a5l,%d1l,%a3@,%d2,%acc2
    40f4:	ae53 100d      	macw %a5l,%d1l,%a3@,%sp,%acc1
    40f8:	aed3 101d      	macw %a5l,%d1l,%a3@,%sp,%acc2
    40fc:	a213 102d      	macw %a5l,%d1l,%a3@&,%d1,%acc1
    4100:	a293 103d      	macw %a5l,%d1l,%a3@&,%d1,%acc2
    4104:	a653 102d      	macw %a5l,%d1l,%a3@&,%a3,%acc1
    4108:	a6d3 103d      	macw %a5l,%d1l,%a3@&,%a3,%acc2
    410c:	a413 102d      	macw %a5l,%d1l,%a3@&,%d2,%acc1
    4110:	a493 103d      	macw %a5l,%d1l,%a3@&,%d2,%acc2
    4114:	ae53 102d      	macw %a5l,%d1l,%a3@&,%sp,%acc1
    4118:	aed3 103d      	macw %a5l,%d1l,%a3@&,%sp,%acc2
    411c:	a21a 100d      	macw %a5l,%d1l,%a2@\+,%d1,%acc1
    4120:	a29a 101d      	macw %a5l,%d1l,%a2@\+,%d1,%acc2
    4124:	a65a 100d      	macw %a5l,%d1l,%a2@\+,%a3,%acc1
    4128:	a6da 101d      	macw %a5l,%d1l,%a2@\+,%a3,%acc2
    412c:	a41a 100d      	macw %a5l,%d1l,%a2@\+,%d2,%acc1
    4130:	a49a 101d      	macw %a5l,%d1l,%a2@\+,%d2,%acc2
    4134:	ae5a 100d      	macw %a5l,%d1l,%a2@\+,%sp,%acc1
    4138:	aeda 101d      	macw %a5l,%d1l,%a2@\+,%sp,%acc2
    413c:	a21a 102d      	macw %a5l,%d1l,%a2@\+&,%d1,%acc1
    4140:	a29a 103d      	macw %a5l,%d1l,%a2@\+&,%d1,%acc2
    4144:	a65a 102d      	macw %a5l,%d1l,%a2@\+&,%a3,%acc1
    4148:	a6da 103d      	macw %a5l,%d1l,%a2@\+&,%a3,%acc2
    414c:	a41a 102d      	macw %a5l,%d1l,%a2@\+&,%d2,%acc1
    4150:	a49a 103d      	macw %a5l,%d1l,%a2@\+&,%d2,%acc2
    4154:	ae5a 102d      	macw %a5l,%d1l,%a2@\+&,%sp,%acc1
    4158:	aeda 103d      	macw %a5l,%d1l,%a2@\+&,%sp,%acc2
    415c:	a22e 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%d1,%acc1
    4162:	a2ae 101d 000a 	macw %a5l,%d1l,%fp@\(10\),%d1,%acc2
    4168:	a66e 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%a3,%acc1
    416e:	a6ee 101d 000a 	macw %a5l,%d1l,%fp@\(10\),%a3,%acc2
    4174:	a42e 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%d2,%acc1
    417a:	a4ae 101d 000a 	macw %a5l,%d1l,%fp@\(10\),%d2,%acc2
    4180:	ae6e 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%sp,%acc1
    4186:	aeee 101d 000a 	macw %a5l,%d1l,%fp@\(10\),%sp,%acc2
    418c:	a22e 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d1,%acc1
    4192:	a2ae 103d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d1,%acc2
    4198:	a66e 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%a3,%acc1
    419e:	a6ee 103d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%a3,%acc2
    41a4:	a42e 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d2,%acc1
    41aa:	a4ae 103d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d2,%acc2
    41b0:	ae6e 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%sp,%acc1
    41b6:	aeee 103d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%sp,%acc2
    41bc:	a221 100d      	macw %a5l,%d1l,%a1@-,%d1,%acc1
    41c0:	a2a1 101d      	macw %a5l,%d1l,%a1@-,%d1,%acc2
    41c4:	a661 100d      	macw %a5l,%d1l,%a1@-,%a3,%acc1
    41c8:	a6e1 101d      	macw %a5l,%d1l,%a1@-,%a3,%acc2
    41cc:	a421 100d      	macw %a5l,%d1l,%a1@-,%d2,%acc1
    41d0:	a4a1 101d      	macw %a5l,%d1l,%a1@-,%d2,%acc2
    41d4:	ae61 100d      	macw %a5l,%d1l,%a1@-,%sp,%acc1
    41d8:	aee1 101d      	macw %a5l,%d1l,%a1@-,%sp,%acc2
    41dc:	a221 102d      	macw %a5l,%d1l,%a1@-&,%d1,%acc1
    41e0:	a2a1 103d      	macw %a5l,%d1l,%a1@-&,%d1,%acc2
    41e4:	a661 102d      	macw %a5l,%d1l,%a1@-&,%a3,%acc1
    41e8:	a6e1 103d      	macw %a5l,%d1l,%a1@-&,%a3,%acc2
    41ec:	a421 102d      	macw %a5l,%d1l,%a1@-&,%d2,%acc1
    41f0:	a4a1 103d      	macw %a5l,%d1l,%a1@-&,%d2,%acc2
    41f4:	ae61 102d      	macw %a5l,%d1l,%a1@-&,%sp,%acc1
    41f8:	aee1 103d      	macw %a5l,%d1l,%a1@-&,%sp,%acc2
    41fc:	a213 120d      	macw %a5l,%d1l,<<,%a3@,%d1,%acc1
    4200:	a293 121d      	macw %a5l,%d1l,<<,%a3@,%d1,%acc2
    4204:	a653 120d      	macw %a5l,%d1l,<<,%a3@,%a3,%acc1
    4208:	a6d3 121d      	macw %a5l,%d1l,<<,%a3@,%a3,%acc2
    420c:	a413 120d      	macw %a5l,%d1l,<<,%a3@,%d2,%acc1
    4210:	a493 121d      	macw %a5l,%d1l,<<,%a3@,%d2,%acc2
    4214:	ae53 120d      	macw %a5l,%d1l,<<,%a3@,%sp,%acc1
    4218:	aed3 121d      	macw %a5l,%d1l,<<,%a3@,%sp,%acc2
    421c:	a213 122d      	macw %a5l,%d1l,<<,%a3@&,%d1,%acc1
    4220:	a293 123d      	macw %a5l,%d1l,<<,%a3@&,%d1,%acc2
    4224:	a653 122d      	macw %a5l,%d1l,<<,%a3@&,%a3,%acc1
    4228:	a6d3 123d      	macw %a5l,%d1l,<<,%a3@&,%a3,%acc2
    422c:	a413 122d      	macw %a5l,%d1l,<<,%a3@&,%d2,%acc1
    4230:	a493 123d      	macw %a5l,%d1l,<<,%a3@&,%d2,%acc2
    4234:	ae53 122d      	macw %a5l,%d1l,<<,%a3@&,%sp,%acc1
    4238:	aed3 123d      	macw %a5l,%d1l,<<,%a3@&,%sp,%acc2
    423c:	a21a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d1,%acc1
    4240:	a29a 121d      	macw %a5l,%d1l,<<,%a2@\+,%d1,%acc2
    4244:	a65a 120d      	macw %a5l,%d1l,<<,%a2@\+,%a3,%acc1
    4248:	a6da 121d      	macw %a5l,%d1l,<<,%a2@\+,%a3,%acc2
    424c:	a41a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d2,%acc1
    4250:	a49a 121d      	macw %a5l,%d1l,<<,%a2@\+,%d2,%acc2
    4254:	ae5a 120d      	macw %a5l,%d1l,<<,%a2@\+,%sp,%acc1
    4258:	aeda 121d      	macw %a5l,%d1l,<<,%a2@\+,%sp,%acc2
    425c:	a21a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d1,%acc1
    4260:	a29a 123d      	macw %a5l,%d1l,<<,%a2@\+&,%d1,%acc2
    4264:	a65a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%a3,%acc1
    4268:	a6da 123d      	macw %a5l,%d1l,<<,%a2@\+&,%a3,%acc2
    426c:	a41a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d2,%acc1
    4270:	a49a 123d      	macw %a5l,%d1l,<<,%a2@\+&,%d2,%acc2
    4274:	ae5a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%sp,%acc1
    4278:	aeda 123d      	macw %a5l,%d1l,<<,%a2@\+&,%sp,%acc2
    427c:	a22e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1,%acc1
    4282:	a2ae 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1,%acc2
    4288:	a66e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3,%acc1
    428e:	a6ee 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3,%acc2
    4294:	a42e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2,%acc1
    429a:	a4ae 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2,%acc2
    42a0:	ae6e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp,%acc1
    42a6:	aeee 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp,%acc2
    42ac:	a22e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    42b2:	a2ae 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    42b8:	a66e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    42be:	a6ee 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    42c4:	a42e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    42ca:	a4ae 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    42d0:	ae6e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    42d6:	aeee 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    42dc:	a221 120d      	macw %a5l,%d1l,<<,%a1@-,%d1,%acc1
    42e0:	a2a1 121d      	macw %a5l,%d1l,<<,%a1@-,%d1,%acc2
    42e4:	a661 120d      	macw %a5l,%d1l,<<,%a1@-,%a3,%acc1
    42e8:	a6e1 121d      	macw %a5l,%d1l,<<,%a1@-,%a3,%acc2
    42ec:	a421 120d      	macw %a5l,%d1l,<<,%a1@-,%d2,%acc1
    42f0:	a4a1 121d      	macw %a5l,%d1l,<<,%a1@-,%d2,%acc2
    42f4:	ae61 120d      	macw %a5l,%d1l,<<,%a1@-,%sp,%acc1
    42f8:	aee1 121d      	macw %a5l,%d1l,<<,%a1@-,%sp,%acc2
    42fc:	a221 122d      	macw %a5l,%d1l,<<,%a1@-&,%d1,%acc1
    4300:	a2a1 123d      	macw %a5l,%d1l,<<,%a1@-&,%d1,%acc2
    4304:	a661 122d      	macw %a5l,%d1l,<<,%a1@-&,%a3,%acc1
    4308:	a6e1 123d      	macw %a5l,%d1l,<<,%a1@-&,%a3,%acc2
    430c:	a421 122d      	macw %a5l,%d1l,<<,%a1@-&,%d2,%acc1
    4310:	a4a1 123d      	macw %a5l,%d1l,<<,%a1@-&,%d2,%acc2
    4314:	ae61 122d      	macw %a5l,%d1l,<<,%a1@-&,%sp,%acc1
    4318:	aee1 123d      	macw %a5l,%d1l,<<,%a1@-&,%sp,%acc2
    431c:	a213 160d      	macw %a5l,%d1l,>>,%a3@,%d1,%acc1
    4320:	a293 161d      	macw %a5l,%d1l,>>,%a3@,%d1,%acc2
    4324:	a653 160d      	macw %a5l,%d1l,>>,%a3@,%a3,%acc1
    4328:	a6d3 161d      	macw %a5l,%d1l,>>,%a3@,%a3,%acc2
    432c:	a413 160d      	macw %a5l,%d1l,>>,%a3@,%d2,%acc1
    4330:	a493 161d      	macw %a5l,%d1l,>>,%a3@,%d2,%acc2
    4334:	ae53 160d      	macw %a5l,%d1l,>>,%a3@,%sp,%acc1
    4338:	aed3 161d      	macw %a5l,%d1l,>>,%a3@,%sp,%acc2
    433c:	a213 162d      	macw %a5l,%d1l,>>,%a3@&,%d1,%acc1
    4340:	a293 163d      	macw %a5l,%d1l,>>,%a3@&,%d1,%acc2
    4344:	a653 162d      	macw %a5l,%d1l,>>,%a3@&,%a3,%acc1
    4348:	a6d3 163d      	macw %a5l,%d1l,>>,%a3@&,%a3,%acc2
    434c:	a413 162d      	macw %a5l,%d1l,>>,%a3@&,%d2,%acc1
    4350:	a493 163d      	macw %a5l,%d1l,>>,%a3@&,%d2,%acc2
    4354:	ae53 162d      	macw %a5l,%d1l,>>,%a3@&,%sp,%acc1
    4358:	aed3 163d      	macw %a5l,%d1l,>>,%a3@&,%sp,%acc2
    435c:	a21a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d1,%acc1
    4360:	a29a 161d      	macw %a5l,%d1l,>>,%a2@\+,%d1,%acc2
    4364:	a65a 160d      	macw %a5l,%d1l,>>,%a2@\+,%a3,%acc1
    4368:	a6da 161d      	macw %a5l,%d1l,>>,%a2@\+,%a3,%acc2
    436c:	a41a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d2,%acc1
    4370:	a49a 161d      	macw %a5l,%d1l,>>,%a2@\+,%d2,%acc2
    4374:	ae5a 160d      	macw %a5l,%d1l,>>,%a2@\+,%sp,%acc1
    4378:	aeda 161d      	macw %a5l,%d1l,>>,%a2@\+,%sp,%acc2
    437c:	a21a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d1,%acc1
    4380:	a29a 163d      	macw %a5l,%d1l,>>,%a2@\+&,%d1,%acc2
    4384:	a65a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%a3,%acc1
    4388:	a6da 163d      	macw %a5l,%d1l,>>,%a2@\+&,%a3,%acc2
    438c:	a41a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d2,%acc1
    4390:	a49a 163d      	macw %a5l,%d1l,>>,%a2@\+&,%d2,%acc2
    4394:	ae5a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%sp,%acc1
    4398:	aeda 163d      	macw %a5l,%d1l,>>,%a2@\+&,%sp,%acc2
    439c:	a22e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1,%acc1
    43a2:	a2ae 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1,%acc2
    43a8:	a66e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3,%acc1
    43ae:	a6ee 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3,%acc2
    43b4:	a42e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2,%acc1
    43ba:	a4ae 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2,%acc2
    43c0:	ae6e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp,%acc1
    43c6:	aeee 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp,%acc2
    43cc:	a22e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    43d2:	a2ae 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    43d8:	a66e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    43de:	a6ee 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    43e4:	a42e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    43ea:	a4ae 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    43f0:	ae6e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    43f6:	aeee 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    43fc:	a221 160d      	macw %a5l,%d1l,>>,%a1@-,%d1,%acc1
    4400:	a2a1 161d      	macw %a5l,%d1l,>>,%a1@-,%d1,%acc2
    4404:	a661 160d      	macw %a5l,%d1l,>>,%a1@-,%a3,%acc1
    4408:	a6e1 161d      	macw %a5l,%d1l,>>,%a1@-,%a3,%acc2
    440c:	a421 160d      	macw %a5l,%d1l,>>,%a1@-,%d2,%acc1
    4410:	a4a1 161d      	macw %a5l,%d1l,>>,%a1@-,%d2,%acc2
    4414:	ae61 160d      	macw %a5l,%d1l,>>,%a1@-,%sp,%acc1
    4418:	aee1 161d      	macw %a5l,%d1l,>>,%a1@-,%sp,%acc2
    441c:	a221 162d      	macw %a5l,%d1l,>>,%a1@-&,%d1,%acc1
    4420:	a2a1 163d      	macw %a5l,%d1l,>>,%a1@-&,%d1,%acc2
    4424:	a661 162d      	macw %a5l,%d1l,>>,%a1@-&,%a3,%acc1
    4428:	a6e1 163d      	macw %a5l,%d1l,>>,%a1@-&,%a3,%acc2
    442c:	a421 162d      	macw %a5l,%d1l,>>,%a1@-&,%d2,%acc1
    4430:	a4a1 163d      	macw %a5l,%d1l,>>,%a1@-&,%d2,%acc2
    4434:	ae61 162d      	macw %a5l,%d1l,>>,%a1@-&,%sp,%acc1
    4438:	aee1 163d      	macw %a5l,%d1l,>>,%a1@-&,%sp,%acc2
    443c:	a213 120d      	macw %a5l,%d1l,<<,%a3@,%d1,%acc1
    4440:	a293 121d      	macw %a5l,%d1l,<<,%a3@,%d1,%acc2
    4444:	a653 120d      	macw %a5l,%d1l,<<,%a3@,%a3,%acc1
    4448:	a6d3 121d      	macw %a5l,%d1l,<<,%a3@,%a3,%acc2
    444c:	a413 120d      	macw %a5l,%d1l,<<,%a3@,%d2,%acc1
    4450:	a493 121d      	macw %a5l,%d1l,<<,%a3@,%d2,%acc2
    4454:	ae53 120d      	macw %a5l,%d1l,<<,%a3@,%sp,%acc1
    4458:	aed3 121d      	macw %a5l,%d1l,<<,%a3@,%sp,%acc2
    445c:	a213 122d      	macw %a5l,%d1l,<<,%a3@&,%d1,%acc1
    4460:	a293 123d      	macw %a5l,%d1l,<<,%a3@&,%d1,%acc2
    4464:	a653 122d      	macw %a5l,%d1l,<<,%a3@&,%a3,%acc1
    4468:	a6d3 123d      	macw %a5l,%d1l,<<,%a3@&,%a3,%acc2
    446c:	a413 122d      	macw %a5l,%d1l,<<,%a3@&,%d2,%acc1
    4470:	a493 123d      	macw %a5l,%d1l,<<,%a3@&,%d2,%acc2
    4474:	ae53 122d      	macw %a5l,%d1l,<<,%a3@&,%sp,%acc1
    4478:	aed3 123d      	macw %a5l,%d1l,<<,%a3@&,%sp,%acc2
    447c:	a21a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d1,%acc1
    4480:	a29a 121d      	macw %a5l,%d1l,<<,%a2@\+,%d1,%acc2
    4484:	a65a 120d      	macw %a5l,%d1l,<<,%a2@\+,%a3,%acc1
    4488:	a6da 121d      	macw %a5l,%d1l,<<,%a2@\+,%a3,%acc2
    448c:	a41a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d2,%acc1
    4490:	a49a 121d      	macw %a5l,%d1l,<<,%a2@\+,%d2,%acc2
    4494:	ae5a 120d      	macw %a5l,%d1l,<<,%a2@\+,%sp,%acc1
    4498:	aeda 121d      	macw %a5l,%d1l,<<,%a2@\+,%sp,%acc2
    449c:	a21a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d1,%acc1
    44a0:	a29a 123d      	macw %a5l,%d1l,<<,%a2@\+&,%d1,%acc2
    44a4:	a65a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%a3,%acc1
    44a8:	a6da 123d      	macw %a5l,%d1l,<<,%a2@\+&,%a3,%acc2
    44ac:	a41a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d2,%acc1
    44b0:	a49a 123d      	macw %a5l,%d1l,<<,%a2@\+&,%d2,%acc2
    44b4:	ae5a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%sp,%acc1
    44b8:	aeda 123d      	macw %a5l,%d1l,<<,%a2@\+&,%sp,%acc2
    44bc:	a22e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1,%acc1
    44c2:	a2ae 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1,%acc2
    44c8:	a66e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3,%acc1
    44ce:	a6ee 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3,%acc2
    44d4:	a42e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2,%acc1
    44da:	a4ae 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2,%acc2
    44e0:	ae6e 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp,%acc1
    44e6:	aeee 121d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp,%acc2
    44ec:	a22e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    44f2:	a2ae 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    44f8:	a66e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    44fe:	a6ee 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    4504:	a42e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    450a:	a4ae 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    4510:	ae6e 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    4516:	aeee 123d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    451c:	a221 120d      	macw %a5l,%d1l,<<,%a1@-,%d1,%acc1
    4520:	a2a1 121d      	macw %a5l,%d1l,<<,%a1@-,%d1,%acc2
    4524:	a661 120d      	macw %a5l,%d1l,<<,%a1@-,%a3,%acc1
    4528:	a6e1 121d      	macw %a5l,%d1l,<<,%a1@-,%a3,%acc2
    452c:	a421 120d      	macw %a5l,%d1l,<<,%a1@-,%d2,%acc1
    4530:	a4a1 121d      	macw %a5l,%d1l,<<,%a1@-,%d2,%acc2
    4534:	ae61 120d      	macw %a5l,%d1l,<<,%a1@-,%sp,%acc1
    4538:	aee1 121d      	macw %a5l,%d1l,<<,%a1@-,%sp,%acc2
    453c:	a221 122d      	macw %a5l,%d1l,<<,%a1@-&,%d1,%acc1
    4540:	a2a1 123d      	macw %a5l,%d1l,<<,%a1@-&,%d1,%acc2
    4544:	a661 122d      	macw %a5l,%d1l,<<,%a1@-&,%a3,%acc1
    4548:	a6e1 123d      	macw %a5l,%d1l,<<,%a1@-&,%a3,%acc2
    454c:	a421 122d      	macw %a5l,%d1l,<<,%a1@-&,%d2,%acc1
    4550:	a4a1 123d      	macw %a5l,%d1l,<<,%a1@-&,%d2,%acc2
    4554:	ae61 122d      	macw %a5l,%d1l,<<,%a1@-&,%sp,%acc1
    4558:	aee1 123d      	macw %a5l,%d1l,<<,%a1@-&,%sp,%acc2
    455c:	a213 160d      	macw %a5l,%d1l,>>,%a3@,%d1,%acc1
    4560:	a293 161d      	macw %a5l,%d1l,>>,%a3@,%d1,%acc2
    4564:	a653 160d      	macw %a5l,%d1l,>>,%a3@,%a3,%acc1
    4568:	a6d3 161d      	macw %a5l,%d1l,>>,%a3@,%a3,%acc2
    456c:	a413 160d      	macw %a5l,%d1l,>>,%a3@,%d2,%acc1
    4570:	a493 161d      	macw %a5l,%d1l,>>,%a3@,%d2,%acc2
    4574:	ae53 160d      	macw %a5l,%d1l,>>,%a3@,%sp,%acc1
    4578:	aed3 161d      	macw %a5l,%d1l,>>,%a3@,%sp,%acc2
    457c:	a213 162d      	macw %a5l,%d1l,>>,%a3@&,%d1,%acc1
    4580:	a293 163d      	macw %a5l,%d1l,>>,%a3@&,%d1,%acc2
    4584:	a653 162d      	macw %a5l,%d1l,>>,%a3@&,%a3,%acc1
    4588:	a6d3 163d      	macw %a5l,%d1l,>>,%a3@&,%a3,%acc2
    458c:	a413 162d      	macw %a5l,%d1l,>>,%a3@&,%d2,%acc1
    4590:	a493 163d      	macw %a5l,%d1l,>>,%a3@&,%d2,%acc2
    4594:	ae53 162d      	macw %a5l,%d1l,>>,%a3@&,%sp,%acc1
    4598:	aed3 163d      	macw %a5l,%d1l,>>,%a3@&,%sp,%acc2
    459c:	a21a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d1,%acc1
    45a0:	a29a 161d      	macw %a5l,%d1l,>>,%a2@\+,%d1,%acc2
    45a4:	a65a 160d      	macw %a5l,%d1l,>>,%a2@\+,%a3,%acc1
    45a8:	a6da 161d      	macw %a5l,%d1l,>>,%a2@\+,%a3,%acc2
    45ac:	a41a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d2,%acc1
    45b0:	a49a 161d      	macw %a5l,%d1l,>>,%a2@\+,%d2,%acc2
    45b4:	ae5a 160d      	macw %a5l,%d1l,>>,%a2@\+,%sp,%acc1
    45b8:	aeda 161d      	macw %a5l,%d1l,>>,%a2@\+,%sp,%acc2
    45bc:	a21a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d1,%acc1
    45c0:	a29a 163d      	macw %a5l,%d1l,>>,%a2@\+&,%d1,%acc2
    45c4:	a65a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%a3,%acc1
    45c8:	a6da 163d      	macw %a5l,%d1l,>>,%a2@\+&,%a3,%acc2
    45cc:	a41a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d2,%acc1
    45d0:	a49a 163d      	macw %a5l,%d1l,>>,%a2@\+&,%d2,%acc2
    45d4:	ae5a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%sp,%acc1
    45d8:	aeda 163d      	macw %a5l,%d1l,>>,%a2@\+&,%sp,%acc2
    45dc:	a22e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1,%acc1
    45e2:	a2ae 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1,%acc2
    45e8:	a66e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3,%acc1
    45ee:	a6ee 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3,%acc2
    45f4:	a42e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2,%acc1
    45fa:	a4ae 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2,%acc2
    4600:	ae6e 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp,%acc1
    4606:	aeee 161d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp,%acc2
    460c:	a22e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    4612:	a2ae 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    4618:	a66e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    461e:	a6ee 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    4624:	a42e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    462a:	a4ae 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    4630:	ae6e 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    4636:	aeee 163d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    463c:	a221 160d      	macw %a5l,%d1l,>>,%a1@-,%d1,%acc1
    4640:	a2a1 161d      	macw %a5l,%d1l,>>,%a1@-,%d1,%acc2
    4644:	a661 160d      	macw %a5l,%d1l,>>,%a1@-,%a3,%acc1
    4648:	a6e1 161d      	macw %a5l,%d1l,>>,%a1@-,%a3,%acc2
    464c:	a421 160d      	macw %a5l,%d1l,>>,%a1@-,%d2,%acc1
    4650:	a4a1 161d      	macw %a5l,%d1l,>>,%a1@-,%d2,%acc2
    4654:	ae61 160d      	macw %a5l,%d1l,>>,%a1@-,%sp,%acc1
    4658:	aee1 161d      	macw %a5l,%d1l,>>,%a1@-,%sp,%acc2
    465c:	a221 162d      	macw %a5l,%d1l,>>,%a1@-&,%d1,%acc1
    4660:	a2a1 163d      	macw %a5l,%d1l,>>,%a1@-&,%d1,%acc2
    4664:	a661 162d      	macw %a5l,%d1l,>>,%a1@-&,%a3,%acc1
    4668:	a6e1 163d      	macw %a5l,%d1l,>>,%a1@-&,%a3,%acc2
    466c:	a421 162d      	macw %a5l,%d1l,>>,%a1@-&,%d2,%acc1
    4670:	a4a1 163d      	macw %a5l,%d1l,>>,%a1@-&,%d2,%acc2
    4674:	ae61 162d      	macw %a5l,%d1l,>>,%a1@-&,%sp,%acc1
    4678:	aee1 163d      	macw %a5l,%d1l,>>,%a1@-&,%sp,%acc2
    467c:	a213 a0c6      	macw %d6u,%a2u,%a3@,%d1,%acc1
    4680:	a293 a0d6      	macw %d6u,%a2u,%a3@,%d1,%acc2
    4684:	a653 a0c6      	macw %d6u,%a2u,%a3@,%a3,%acc1
    4688:	a6d3 a0d6      	macw %d6u,%a2u,%a3@,%a3,%acc2
    468c:	a413 a0c6      	macw %d6u,%a2u,%a3@,%d2,%acc1
    4690:	a493 a0d6      	macw %d6u,%a2u,%a3@,%d2,%acc2
    4694:	ae53 a0c6      	macw %d6u,%a2u,%a3@,%sp,%acc1
    4698:	aed3 a0d6      	macw %d6u,%a2u,%a3@,%sp,%acc2
    469c:	a213 a0e6      	macw %d6u,%a2u,%a3@&,%d1,%acc1
    46a0:	a293 a0f6      	macw %d6u,%a2u,%a3@&,%d1,%acc2
    46a4:	a653 a0e6      	macw %d6u,%a2u,%a3@&,%a3,%acc1
    46a8:	a6d3 a0f6      	macw %d6u,%a2u,%a3@&,%a3,%acc2
    46ac:	a413 a0e6      	macw %d6u,%a2u,%a3@&,%d2,%acc1
    46b0:	a493 a0f6      	macw %d6u,%a2u,%a3@&,%d2,%acc2
    46b4:	ae53 a0e6      	macw %d6u,%a2u,%a3@&,%sp,%acc1
    46b8:	aed3 a0f6      	macw %d6u,%a2u,%a3@&,%sp,%acc2
    46bc:	a21a a0c6      	macw %d6u,%a2u,%a2@\+,%d1,%acc1
    46c0:	a29a a0d6      	macw %d6u,%a2u,%a2@\+,%d1,%acc2
    46c4:	a65a a0c6      	macw %d6u,%a2u,%a2@\+,%a3,%acc1
    46c8:	a6da a0d6      	macw %d6u,%a2u,%a2@\+,%a3,%acc2
    46cc:	a41a a0c6      	macw %d6u,%a2u,%a2@\+,%d2,%acc1
    46d0:	a49a a0d6      	macw %d6u,%a2u,%a2@\+,%d2,%acc2
    46d4:	ae5a a0c6      	macw %d6u,%a2u,%a2@\+,%sp,%acc1
    46d8:	aeda a0d6      	macw %d6u,%a2u,%a2@\+,%sp,%acc2
    46dc:	a21a a0e6      	macw %d6u,%a2u,%a2@\+&,%d1,%acc1
    46e0:	a29a a0f6      	macw %d6u,%a2u,%a2@\+&,%d1,%acc2
    46e4:	a65a a0e6      	macw %d6u,%a2u,%a2@\+&,%a3,%acc1
    46e8:	a6da a0f6      	macw %d6u,%a2u,%a2@\+&,%a3,%acc2
    46ec:	a41a a0e6      	macw %d6u,%a2u,%a2@\+&,%d2,%acc1
    46f0:	a49a a0f6      	macw %d6u,%a2u,%a2@\+&,%d2,%acc2
    46f4:	ae5a a0e6      	macw %d6u,%a2u,%a2@\+&,%sp,%acc1
    46f8:	aeda a0f6      	macw %d6u,%a2u,%a2@\+&,%sp,%acc2
    46fc:	a22e a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%d1,%acc1
    4702:	a2ae a0d6 000a 	macw %d6u,%a2u,%fp@\(10\),%d1,%acc2
    4708:	a66e a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%a3,%acc1
    470e:	a6ee a0d6 000a 	macw %d6u,%a2u,%fp@\(10\),%a3,%acc2
    4714:	a42e a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%d2,%acc1
    471a:	a4ae a0d6 000a 	macw %d6u,%a2u,%fp@\(10\),%d2,%acc2
    4720:	ae6e a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%sp,%acc1
    4726:	aeee a0d6 000a 	macw %d6u,%a2u,%fp@\(10\),%sp,%acc2
    472c:	a22e a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d1,%acc1
    4732:	a2ae a0f6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d1,%acc2
    4738:	a66e a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%a3,%acc1
    473e:	a6ee a0f6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%a3,%acc2
    4744:	a42e a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d2,%acc1
    474a:	a4ae a0f6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d2,%acc2
    4750:	ae6e a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%sp,%acc1
    4756:	aeee a0f6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%sp,%acc2
    475c:	a221 a0c6      	macw %d6u,%a2u,%a1@-,%d1,%acc1
    4760:	a2a1 a0d6      	macw %d6u,%a2u,%a1@-,%d1,%acc2
    4764:	a661 a0c6      	macw %d6u,%a2u,%a1@-,%a3,%acc1
    4768:	a6e1 a0d6      	macw %d6u,%a2u,%a1@-,%a3,%acc2
    476c:	a421 a0c6      	macw %d6u,%a2u,%a1@-,%d2,%acc1
    4770:	a4a1 a0d6      	macw %d6u,%a2u,%a1@-,%d2,%acc2
    4774:	ae61 a0c6      	macw %d6u,%a2u,%a1@-,%sp,%acc1
    4778:	aee1 a0d6      	macw %d6u,%a2u,%a1@-,%sp,%acc2
    477c:	a221 a0e6      	macw %d6u,%a2u,%a1@-&,%d1,%acc1
    4780:	a2a1 a0f6      	macw %d6u,%a2u,%a1@-&,%d1,%acc2
    4784:	a661 a0e6      	macw %d6u,%a2u,%a1@-&,%a3,%acc1
    4788:	a6e1 a0f6      	macw %d6u,%a2u,%a1@-&,%a3,%acc2
    478c:	a421 a0e6      	macw %d6u,%a2u,%a1@-&,%d2,%acc1
    4790:	a4a1 a0f6      	macw %d6u,%a2u,%a1@-&,%d2,%acc2
    4794:	ae61 a0e6      	macw %d6u,%a2u,%a1@-&,%sp,%acc1
    4798:	aee1 a0f6      	macw %d6u,%a2u,%a1@-&,%sp,%acc2
    479c:	a213 a2c6      	macw %d6u,%a2u,<<,%a3@,%d1,%acc1
    47a0:	a293 a2d6      	macw %d6u,%a2u,<<,%a3@,%d1,%acc2
    47a4:	a653 a2c6      	macw %d6u,%a2u,<<,%a3@,%a3,%acc1
    47a8:	a6d3 a2d6      	macw %d6u,%a2u,<<,%a3@,%a3,%acc2
    47ac:	a413 a2c6      	macw %d6u,%a2u,<<,%a3@,%d2,%acc1
    47b0:	a493 a2d6      	macw %d6u,%a2u,<<,%a3@,%d2,%acc2
    47b4:	ae53 a2c6      	macw %d6u,%a2u,<<,%a3@,%sp,%acc1
    47b8:	aed3 a2d6      	macw %d6u,%a2u,<<,%a3@,%sp,%acc2
    47bc:	a213 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d1,%acc1
    47c0:	a293 a2f6      	macw %d6u,%a2u,<<,%a3@&,%d1,%acc2
    47c4:	a653 a2e6      	macw %d6u,%a2u,<<,%a3@&,%a3,%acc1
    47c8:	a6d3 a2f6      	macw %d6u,%a2u,<<,%a3@&,%a3,%acc2
    47cc:	a413 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d2,%acc1
    47d0:	a493 a2f6      	macw %d6u,%a2u,<<,%a3@&,%d2,%acc2
    47d4:	ae53 a2e6      	macw %d6u,%a2u,<<,%a3@&,%sp,%acc1
    47d8:	aed3 a2f6      	macw %d6u,%a2u,<<,%a3@&,%sp,%acc2
    47dc:	a21a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d1,%acc1
    47e0:	a29a a2d6      	macw %d6u,%a2u,<<,%a2@\+,%d1,%acc2
    47e4:	a65a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%a3,%acc1
    47e8:	a6da a2d6      	macw %d6u,%a2u,<<,%a2@\+,%a3,%acc2
    47ec:	a41a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d2,%acc1
    47f0:	a49a a2d6      	macw %d6u,%a2u,<<,%a2@\+,%d2,%acc2
    47f4:	ae5a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%sp,%acc1
    47f8:	aeda a2d6      	macw %d6u,%a2u,<<,%a2@\+,%sp,%acc2
    47fc:	a21a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d1,%acc1
    4800:	a29a a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%d1,%acc2
    4804:	a65a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%a3,%acc1
    4808:	a6da a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%a3,%acc2
    480c:	a41a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d2,%acc1
    4810:	a49a a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%d2,%acc2
    4814:	ae5a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%sp,%acc1
    4818:	aeda a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%sp,%acc2
    481c:	a22e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1,%acc1
    4822:	a2ae a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1,%acc2
    4828:	a66e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3,%acc1
    482e:	a6ee a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3,%acc2
    4834:	a42e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2,%acc1
    483a:	a4ae a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2,%acc2
    4840:	ae6e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp,%acc1
    4846:	aeee a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp,%acc2
    484c:	a22e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    4852:	a2ae a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    4858:	a66e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    485e:	a6ee a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    4864:	a42e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    486a:	a4ae a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    4870:	ae6e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    4876:	aeee a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    487c:	a221 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d1,%acc1
    4880:	a2a1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%d1,%acc2
    4884:	a661 a2c6      	macw %d6u,%a2u,<<,%a1@-,%a3,%acc1
    4888:	a6e1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%a3,%acc2
    488c:	a421 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d2,%acc1
    4890:	a4a1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%d2,%acc2
    4894:	ae61 a2c6      	macw %d6u,%a2u,<<,%a1@-,%sp,%acc1
    4898:	aee1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%sp,%acc2
    489c:	a221 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d1,%acc1
    48a0:	a2a1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%d1,%acc2
    48a4:	a661 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%a3,%acc1
    48a8:	a6e1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%a3,%acc2
    48ac:	a421 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d2,%acc1
    48b0:	a4a1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%d2,%acc2
    48b4:	ae61 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%sp,%acc1
    48b8:	aee1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%sp,%acc2
    48bc:	a213 a6c6      	macw %d6u,%a2u,>>,%a3@,%d1,%acc1
    48c0:	a293 a6d6      	macw %d6u,%a2u,>>,%a3@,%d1,%acc2
    48c4:	a653 a6c6      	macw %d6u,%a2u,>>,%a3@,%a3,%acc1
    48c8:	a6d3 a6d6      	macw %d6u,%a2u,>>,%a3@,%a3,%acc2
    48cc:	a413 a6c6      	macw %d6u,%a2u,>>,%a3@,%d2,%acc1
    48d0:	a493 a6d6      	macw %d6u,%a2u,>>,%a3@,%d2,%acc2
    48d4:	ae53 a6c6      	macw %d6u,%a2u,>>,%a3@,%sp,%acc1
    48d8:	aed3 a6d6      	macw %d6u,%a2u,>>,%a3@,%sp,%acc2
    48dc:	a213 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d1,%acc1
    48e0:	a293 a6f6      	macw %d6u,%a2u,>>,%a3@&,%d1,%acc2
    48e4:	a653 a6e6      	macw %d6u,%a2u,>>,%a3@&,%a3,%acc1
    48e8:	a6d3 a6f6      	macw %d6u,%a2u,>>,%a3@&,%a3,%acc2
    48ec:	a413 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d2,%acc1
    48f0:	a493 a6f6      	macw %d6u,%a2u,>>,%a3@&,%d2,%acc2
    48f4:	ae53 a6e6      	macw %d6u,%a2u,>>,%a3@&,%sp,%acc1
    48f8:	aed3 a6f6      	macw %d6u,%a2u,>>,%a3@&,%sp,%acc2
    48fc:	a21a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d1,%acc1
    4900:	a29a a6d6      	macw %d6u,%a2u,>>,%a2@\+,%d1,%acc2
    4904:	a65a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%a3,%acc1
    4908:	a6da a6d6      	macw %d6u,%a2u,>>,%a2@\+,%a3,%acc2
    490c:	a41a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d2,%acc1
    4910:	a49a a6d6      	macw %d6u,%a2u,>>,%a2@\+,%d2,%acc2
    4914:	ae5a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%sp,%acc1
    4918:	aeda a6d6      	macw %d6u,%a2u,>>,%a2@\+,%sp,%acc2
    491c:	a21a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d1,%acc1
    4920:	a29a a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%d1,%acc2
    4924:	a65a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%a3,%acc1
    4928:	a6da a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%a3,%acc2
    492c:	a41a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d2,%acc1
    4930:	a49a a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%d2,%acc2
    4934:	ae5a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%sp,%acc1
    4938:	aeda a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%sp,%acc2
    493c:	a22e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1,%acc1
    4942:	a2ae a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1,%acc2
    4948:	a66e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3,%acc1
    494e:	a6ee a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3,%acc2
    4954:	a42e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2,%acc1
    495a:	a4ae a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2,%acc2
    4960:	ae6e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp,%acc1
    4966:	aeee a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp,%acc2
    496c:	a22e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    4972:	a2ae a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    4978:	a66e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    497e:	a6ee a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    4984:	a42e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    498a:	a4ae a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    4990:	ae6e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    4996:	aeee a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    499c:	a221 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d1,%acc1
    49a0:	a2a1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%d1,%acc2
    49a4:	a661 a6c6      	macw %d6u,%a2u,>>,%a1@-,%a3,%acc1
    49a8:	a6e1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%a3,%acc2
    49ac:	a421 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d2,%acc1
    49b0:	a4a1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%d2,%acc2
    49b4:	ae61 a6c6      	macw %d6u,%a2u,>>,%a1@-,%sp,%acc1
    49b8:	aee1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%sp,%acc2
    49bc:	a221 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d1,%acc1
    49c0:	a2a1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%d1,%acc2
    49c4:	a661 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%a3,%acc1
    49c8:	a6e1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%a3,%acc2
    49cc:	a421 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d2,%acc1
    49d0:	a4a1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%d2,%acc2
    49d4:	ae61 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%sp,%acc1
    49d8:	aee1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%sp,%acc2
    49dc:	a213 a2c6      	macw %d6u,%a2u,<<,%a3@,%d1,%acc1
    49e0:	a293 a2d6      	macw %d6u,%a2u,<<,%a3@,%d1,%acc2
    49e4:	a653 a2c6      	macw %d6u,%a2u,<<,%a3@,%a3,%acc1
    49e8:	a6d3 a2d6      	macw %d6u,%a2u,<<,%a3@,%a3,%acc2
    49ec:	a413 a2c6      	macw %d6u,%a2u,<<,%a3@,%d2,%acc1
    49f0:	a493 a2d6      	macw %d6u,%a2u,<<,%a3@,%d2,%acc2
    49f4:	ae53 a2c6      	macw %d6u,%a2u,<<,%a3@,%sp,%acc1
    49f8:	aed3 a2d6      	macw %d6u,%a2u,<<,%a3@,%sp,%acc2
    49fc:	a213 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d1,%acc1
    4a00:	a293 a2f6      	macw %d6u,%a2u,<<,%a3@&,%d1,%acc2
    4a04:	a653 a2e6      	macw %d6u,%a2u,<<,%a3@&,%a3,%acc1
    4a08:	a6d3 a2f6      	macw %d6u,%a2u,<<,%a3@&,%a3,%acc2
    4a0c:	a413 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d2,%acc1
    4a10:	a493 a2f6      	macw %d6u,%a2u,<<,%a3@&,%d2,%acc2
    4a14:	ae53 a2e6      	macw %d6u,%a2u,<<,%a3@&,%sp,%acc1
    4a18:	aed3 a2f6      	macw %d6u,%a2u,<<,%a3@&,%sp,%acc2
    4a1c:	a21a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d1,%acc1
    4a20:	a29a a2d6      	macw %d6u,%a2u,<<,%a2@\+,%d1,%acc2
    4a24:	a65a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%a3,%acc1
    4a28:	a6da a2d6      	macw %d6u,%a2u,<<,%a2@\+,%a3,%acc2
    4a2c:	a41a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d2,%acc1
    4a30:	a49a a2d6      	macw %d6u,%a2u,<<,%a2@\+,%d2,%acc2
    4a34:	ae5a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%sp,%acc1
    4a38:	aeda a2d6      	macw %d6u,%a2u,<<,%a2@\+,%sp,%acc2
    4a3c:	a21a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d1,%acc1
    4a40:	a29a a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%d1,%acc2
    4a44:	a65a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%a3,%acc1
    4a48:	a6da a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%a3,%acc2
    4a4c:	a41a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d2,%acc1
    4a50:	a49a a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%d2,%acc2
    4a54:	ae5a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%sp,%acc1
    4a58:	aeda a2f6      	macw %d6u,%a2u,<<,%a2@\+&,%sp,%acc2
    4a5c:	a22e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1,%acc1
    4a62:	a2ae a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1,%acc2
    4a68:	a66e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3,%acc1
    4a6e:	a6ee a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3,%acc2
    4a74:	a42e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2,%acc1
    4a7a:	a4ae a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2,%acc2
    4a80:	ae6e a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp,%acc1
    4a86:	aeee a2d6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp,%acc2
    4a8c:	a22e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1,%acc1
    4a92:	a2ae a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1,%acc2
    4a98:	a66e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3,%acc1
    4a9e:	a6ee a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3,%acc2
    4aa4:	a42e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2,%acc1
    4aaa:	a4ae a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2,%acc2
    4ab0:	ae6e a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp,%acc1
    4ab6:	aeee a2f6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp,%acc2
    4abc:	a221 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d1,%acc1
    4ac0:	a2a1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%d1,%acc2
    4ac4:	a661 a2c6      	macw %d6u,%a2u,<<,%a1@-,%a3,%acc1
    4ac8:	a6e1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%a3,%acc2
    4acc:	a421 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d2,%acc1
    4ad0:	a4a1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%d2,%acc2
    4ad4:	ae61 a2c6      	macw %d6u,%a2u,<<,%a1@-,%sp,%acc1
    4ad8:	aee1 a2d6      	macw %d6u,%a2u,<<,%a1@-,%sp,%acc2
    4adc:	a221 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d1,%acc1
    4ae0:	a2a1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%d1,%acc2
    4ae4:	a661 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%a3,%acc1
    4ae8:	a6e1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%a3,%acc2
    4aec:	a421 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d2,%acc1
    4af0:	a4a1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%d2,%acc2
    4af4:	ae61 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%sp,%acc1
    4af8:	aee1 a2f6      	macw %d6u,%a2u,<<,%a1@-&,%sp,%acc2
    4afc:	a213 a6c6      	macw %d6u,%a2u,>>,%a3@,%d1,%acc1
    4b00:	a293 a6d6      	macw %d6u,%a2u,>>,%a3@,%d1,%acc2
    4b04:	a653 a6c6      	macw %d6u,%a2u,>>,%a3@,%a3,%acc1
    4b08:	a6d3 a6d6      	macw %d6u,%a2u,>>,%a3@,%a3,%acc2
    4b0c:	a413 a6c6      	macw %d6u,%a2u,>>,%a3@,%d2,%acc1
    4b10:	a493 a6d6      	macw %d6u,%a2u,>>,%a3@,%d2,%acc2
    4b14:	ae53 a6c6      	macw %d6u,%a2u,>>,%a3@,%sp,%acc1
    4b18:	aed3 a6d6      	macw %d6u,%a2u,>>,%a3@,%sp,%acc2
    4b1c:	a213 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d1,%acc1
    4b20:	a293 a6f6      	macw %d6u,%a2u,>>,%a3@&,%d1,%acc2
    4b24:	a653 a6e6      	macw %d6u,%a2u,>>,%a3@&,%a3,%acc1
    4b28:	a6d3 a6f6      	macw %d6u,%a2u,>>,%a3@&,%a3,%acc2
    4b2c:	a413 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d2,%acc1
    4b30:	a493 a6f6      	macw %d6u,%a2u,>>,%a3@&,%d2,%acc2
    4b34:	ae53 a6e6      	macw %d6u,%a2u,>>,%a3@&,%sp,%acc1
    4b38:	aed3 a6f6      	macw %d6u,%a2u,>>,%a3@&,%sp,%acc2
    4b3c:	a21a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d1,%acc1
    4b40:	a29a a6d6      	macw %d6u,%a2u,>>,%a2@\+,%d1,%acc2
    4b44:	a65a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%a3,%acc1
    4b48:	a6da a6d6      	macw %d6u,%a2u,>>,%a2@\+,%a3,%acc2
    4b4c:	a41a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d2,%acc1
    4b50:	a49a a6d6      	macw %d6u,%a2u,>>,%a2@\+,%d2,%acc2
    4b54:	ae5a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%sp,%acc1
    4b58:	aeda a6d6      	macw %d6u,%a2u,>>,%a2@\+,%sp,%acc2
    4b5c:	a21a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d1,%acc1
    4b60:	a29a a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%d1,%acc2
    4b64:	a65a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%a3,%acc1
    4b68:	a6da a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%a3,%acc2
    4b6c:	a41a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d2,%acc1
    4b70:	a49a a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%d2,%acc2
    4b74:	ae5a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%sp,%acc1
    4b78:	aeda a6f6      	macw %d6u,%a2u,>>,%a2@\+&,%sp,%acc2
    4b7c:	a22e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1,%acc1
    4b82:	a2ae a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1,%acc2
    4b88:	a66e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3,%acc1
    4b8e:	a6ee a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3,%acc2
    4b94:	a42e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2,%acc1
    4b9a:	a4ae a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2,%acc2
    4ba0:	ae6e a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp,%acc1
    4ba6:	aeee a6d6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp,%acc2
    4bac:	a22e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1,%acc1
    4bb2:	a2ae a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1,%acc2
    4bb8:	a66e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3,%acc1
    4bbe:	a6ee a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3,%acc2
    4bc4:	a42e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2,%acc1
    4bca:	a4ae a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2,%acc2
    4bd0:	ae6e a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp,%acc1
    4bd6:	aeee a6f6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp,%acc2
    4bdc:	a221 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d1,%acc1
    4be0:	a2a1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%d1,%acc2
    4be4:	a661 a6c6      	macw %d6u,%a2u,>>,%a1@-,%a3,%acc1
    4be8:	a6e1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%a3,%acc2
    4bec:	a421 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d2,%acc1
    4bf0:	a4a1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%d2,%acc2
    4bf4:	ae61 a6c6      	macw %d6u,%a2u,>>,%a1@-,%sp,%acc1
    4bf8:	aee1 a6d6      	macw %d6u,%a2u,>>,%a1@-,%sp,%acc2
    4bfc:	a221 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d1,%acc1
    4c00:	a2a1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%d1,%acc2
    4c04:	a661 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%a3,%acc1
    4c08:	a6e1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%a3,%acc2
    4c0c:	a421 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d2,%acc1
    4c10:	a4a1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%d2,%acc2
    4c14:	ae61 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%sp,%acc1
    4c18:	aee1 a6f6      	macw %d6u,%a2u,>>,%a1@-&,%sp,%acc2
    4c1c:	a213 3046      	macw %d6u,%d3l,%a3@,%d1,%acc1
    4c20:	a293 3056      	macw %d6u,%d3l,%a3@,%d1,%acc2
    4c24:	a653 3046      	macw %d6u,%d3l,%a3@,%a3,%acc1
    4c28:	a6d3 3056      	macw %d6u,%d3l,%a3@,%a3,%acc2
    4c2c:	a413 3046      	macw %d6u,%d3l,%a3@,%d2,%acc1
    4c30:	a493 3056      	macw %d6u,%d3l,%a3@,%d2,%acc2
    4c34:	ae53 3046      	macw %d6u,%d3l,%a3@,%sp,%acc1
    4c38:	aed3 3056      	macw %d6u,%d3l,%a3@,%sp,%acc2
    4c3c:	a213 3066      	macw %d6u,%d3l,%a3@&,%d1,%acc1
    4c40:	a293 3076      	macw %d6u,%d3l,%a3@&,%d1,%acc2
    4c44:	a653 3066      	macw %d6u,%d3l,%a3@&,%a3,%acc1
    4c48:	a6d3 3076      	macw %d6u,%d3l,%a3@&,%a3,%acc2
    4c4c:	a413 3066      	macw %d6u,%d3l,%a3@&,%d2,%acc1
    4c50:	a493 3076      	macw %d6u,%d3l,%a3@&,%d2,%acc2
    4c54:	ae53 3066      	macw %d6u,%d3l,%a3@&,%sp,%acc1
    4c58:	aed3 3076      	macw %d6u,%d3l,%a3@&,%sp,%acc2
    4c5c:	a21a 3046      	macw %d6u,%d3l,%a2@\+,%d1,%acc1
    4c60:	a29a 3056      	macw %d6u,%d3l,%a2@\+,%d1,%acc2
    4c64:	a65a 3046      	macw %d6u,%d3l,%a2@\+,%a3,%acc1
    4c68:	a6da 3056      	macw %d6u,%d3l,%a2@\+,%a3,%acc2
    4c6c:	a41a 3046      	macw %d6u,%d3l,%a2@\+,%d2,%acc1
    4c70:	a49a 3056      	macw %d6u,%d3l,%a2@\+,%d2,%acc2
    4c74:	ae5a 3046      	macw %d6u,%d3l,%a2@\+,%sp,%acc1
    4c78:	aeda 3056      	macw %d6u,%d3l,%a2@\+,%sp,%acc2
    4c7c:	a21a 3066      	macw %d6u,%d3l,%a2@\+&,%d1,%acc1
    4c80:	a29a 3076      	macw %d6u,%d3l,%a2@\+&,%d1,%acc2
    4c84:	a65a 3066      	macw %d6u,%d3l,%a2@\+&,%a3,%acc1
    4c88:	a6da 3076      	macw %d6u,%d3l,%a2@\+&,%a3,%acc2
    4c8c:	a41a 3066      	macw %d6u,%d3l,%a2@\+&,%d2,%acc1
    4c90:	a49a 3076      	macw %d6u,%d3l,%a2@\+&,%d2,%acc2
    4c94:	ae5a 3066      	macw %d6u,%d3l,%a2@\+&,%sp,%acc1
    4c98:	aeda 3076      	macw %d6u,%d3l,%a2@\+&,%sp,%acc2
    4c9c:	a22e 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%d1,%acc1
    4ca2:	a2ae 3056 000a 	macw %d6u,%d3l,%fp@\(10\),%d1,%acc2
    4ca8:	a66e 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%a3,%acc1
    4cae:	a6ee 3056 000a 	macw %d6u,%d3l,%fp@\(10\),%a3,%acc2
    4cb4:	a42e 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%d2,%acc1
    4cba:	a4ae 3056 000a 	macw %d6u,%d3l,%fp@\(10\),%d2,%acc2
    4cc0:	ae6e 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%sp,%acc1
    4cc6:	aeee 3056 000a 	macw %d6u,%d3l,%fp@\(10\),%sp,%acc2
    4ccc:	a22e 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d1,%acc1
    4cd2:	a2ae 3076 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d1,%acc2
    4cd8:	a66e 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%a3,%acc1
    4cde:	a6ee 3076 000a 	macw %d6u,%d3l,%fp@\(10\)&,%a3,%acc2
    4ce4:	a42e 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d2,%acc1
    4cea:	a4ae 3076 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d2,%acc2
    4cf0:	ae6e 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%sp,%acc1
    4cf6:	aeee 3076 000a 	macw %d6u,%d3l,%fp@\(10\)&,%sp,%acc2
    4cfc:	a221 3046      	macw %d6u,%d3l,%a1@-,%d1,%acc1
    4d00:	a2a1 3056      	macw %d6u,%d3l,%a1@-,%d1,%acc2
    4d04:	a661 3046      	macw %d6u,%d3l,%a1@-,%a3,%acc1
    4d08:	a6e1 3056      	macw %d6u,%d3l,%a1@-,%a3,%acc2
    4d0c:	a421 3046      	macw %d6u,%d3l,%a1@-,%d2,%acc1
    4d10:	a4a1 3056      	macw %d6u,%d3l,%a1@-,%d2,%acc2
    4d14:	ae61 3046      	macw %d6u,%d3l,%a1@-,%sp,%acc1
    4d18:	aee1 3056      	macw %d6u,%d3l,%a1@-,%sp,%acc2
    4d1c:	a221 3066      	macw %d6u,%d3l,%a1@-&,%d1,%acc1
    4d20:	a2a1 3076      	macw %d6u,%d3l,%a1@-&,%d1,%acc2
    4d24:	a661 3066      	macw %d6u,%d3l,%a1@-&,%a3,%acc1
    4d28:	a6e1 3076      	macw %d6u,%d3l,%a1@-&,%a3,%acc2
    4d2c:	a421 3066      	macw %d6u,%d3l,%a1@-&,%d2,%acc1
    4d30:	a4a1 3076      	macw %d6u,%d3l,%a1@-&,%d2,%acc2
    4d34:	ae61 3066      	macw %d6u,%d3l,%a1@-&,%sp,%acc1
    4d38:	aee1 3076      	macw %d6u,%d3l,%a1@-&,%sp,%acc2
    4d3c:	a213 3246      	macw %d6u,%d3l,<<,%a3@,%d1,%acc1
    4d40:	a293 3256      	macw %d6u,%d3l,<<,%a3@,%d1,%acc2
    4d44:	a653 3246      	macw %d6u,%d3l,<<,%a3@,%a3,%acc1
    4d48:	a6d3 3256      	macw %d6u,%d3l,<<,%a3@,%a3,%acc2
    4d4c:	a413 3246      	macw %d6u,%d3l,<<,%a3@,%d2,%acc1
    4d50:	a493 3256      	macw %d6u,%d3l,<<,%a3@,%d2,%acc2
    4d54:	ae53 3246      	macw %d6u,%d3l,<<,%a3@,%sp,%acc1
    4d58:	aed3 3256      	macw %d6u,%d3l,<<,%a3@,%sp,%acc2
    4d5c:	a213 3266      	macw %d6u,%d3l,<<,%a3@&,%d1,%acc1
    4d60:	a293 3276      	macw %d6u,%d3l,<<,%a3@&,%d1,%acc2
    4d64:	a653 3266      	macw %d6u,%d3l,<<,%a3@&,%a3,%acc1
    4d68:	a6d3 3276      	macw %d6u,%d3l,<<,%a3@&,%a3,%acc2
    4d6c:	a413 3266      	macw %d6u,%d3l,<<,%a3@&,%d2,%acc1
    4d70:	a493 3276      	macw %d6u,%d3l,<<,%a3@&,%d2,%acc2
    4d74:	ae53 3266      	macw %d6u,%d3l,<<,%a3@&,%sp,%acc1
    4d78:	aed3 3276      	macw %d6u,%d3l,<<,%a3@&,%sp,%acc2
    4d7c:	a21a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d1,%acc1
    4d80:	a29a 3256      	macw %d6u,%d3l,<<,%a2@\+,%d1,%acc2
    4d84:	a65a 3246      	macw %d6u,%d3l,<<,%a2@\+,%a3,%acc1
    4d88:	a6da 3256      	macw %d6u,%d3l,<<,%a2@\+,%a3,%acc2
    4d8c:	a41a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d2,%acc1
    4d90:	a49a 3256      	macw %d6u,%d3l,<<,%a2@\+,%d2,%acc2
    4d94:	ae5a 3246      	macw %d6u,%d3l,<<,%a2@\+,%sp,%acc1
    4d98:	aeda 3256      	macw %d6u,%d3l,<<,%a2@\+,%sp,%acc2
    4d9c:	a21a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d1,%acc1
    4da0:	a29a 3276      	macw %d6u,%d3l,<<,%a2@\+&,%d1,%acc2
    4da4:	a65a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%a3,%acc1
    4da8:	a6da 3276      	macw %d6u,%d3l,<<,%a2@\+&,%a3,%acc2
    4dac:	a41a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d2,%acc1
    4db0:	a49a 3276      	macw %d6u,%d3l,<<,%a2@\+&,%d2,%acc2
    4db4:	ae5a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%sp,%acc1
    4db8:	aeda 3276      	macw %d6u,%d3l,<<,%a2@\+&,%sp,%acc2
    4dbc:	a22e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1,%acc1
    4dc2:	a2ae 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1,%acc2
    4dc8:	a66e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3,%acc1
    4dce:	a6ee 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3,%acc2
    4dd4:	a42e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2,%acc1
    4dda:	a4ae 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2,%acc2
    4de0:	ae6e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp,%acc1
    4de6:	aeee 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp,%acc2
    4dec:	a22e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    4df2:	a2ae 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    4df8:	a66e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    4dfe:	a6ee 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    4e04:	a42e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    4e0a:	a4ae 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    4e10:	ae6e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    4e16:	aeee 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    4e1c:	a221 3246      	macw %d6u,%d3l,<<,%a1@-,%d1,%acc1
    4e20:	a2a1 3256      	macw %d6u,%d3l,<<,%a1@-,%d1,%acc2
    4e24:	a661 3246      	macw %d6u,%d3l,<<,%a1@-,%a3,%acc1
    4e28:	a6e1 3256      	macw %d6u,%d3l,<<,%a1@-,%a3,%acc2
    4e2c:	a421 3246      	macw %d6u,%d3l,<<,%a1@-,%d2,%acc1
    4e30:	a4a1 3256      	macw %d6u,%d3l,<<,%a1@-,%d2,%acc2
    4e34:	ae61 3246      	macw %d6u,%d3l,<<,%a1@-,%sp,%acc1
    4e38:	aee1 3256      	macw %d6u,%d3l,<<,%a1@-,%sp,%acc2
    4e3c:	a221 3266      	macw %d6u,%d3l,<<,%a1@-&,%d1,%acc1
    4e40:	a2a1 3276      	macw %d6u,%d3l,<<,%a1@-&,%d1,%acc2
    4e44:	a661 3266      	macw %d6u,%d3l,<<,%a1@-&,%a3,%acc1
    4e48:	a6e1 3276      	macw %d6u,%d3l,<<,%a1@-&,%a3,%acc2
    4e4c:	a421 3266      	macw %d6u,%d3l,<<,%a1@-&,%d2,%acc1
    4e50:	a4a1 3276      	macw %d6u,%d3l,<<,%a1@-&,%d2,%acc2
    4e54:	ae61 3266      	macw %d6u,%d3l,<<,%a1@-&,%sp,%acc1
    4e58:	aee1 3276      	macw %d6u,%d3l,<<,%a1@-&,%sp,%acc2
    4e5c:	a213 3646      	macw %d6u,%d3l,>>,%a3@,%d1,%acc1
    4e60:	a293 3656      	macw %d6u,%d3l,>>,%a3@,%d1,%acc2
    4e64:	a653 3646      	macw %d6u,%d3l,>>,%a3@,%a3,%acc1
    4e68:	a6d3 3656      	macw %d6u,%d3l,>>,%a3@,%a3,%acc2
    4e6c:	a413 3646      	macw %d6u,%d3l,>>,%a3@,%d2,%acc1
    4e70:	a493 3656      	macw %d6u,%d3l,>>,%a3@,%d2,%acc2
    4e74:	ae53 3646      	macw %d6u,%d3l,>>,%a3@,%sp,%acc1
    4e78:	aed3 3656      	macw %d6u,%d3l,>>,%a3@,%sp,%acc2
    4e7c:	a213 3666      	macw %d6u,%d3l,>>,%a3@&,%d1,%acc1
    4e80:	a293 3676      	macw %d6u,%d3l,>>,%a3@&,%d1,%acc2
    4e84:	a653 3666      	macw %d6u,%d3l,>>,%a3@&,%a3,%acc1
    4e88:	a6d3 3676      	macw %d6u,%d3l,>>,%a3@&,%a3,%acc2
    4e8c:	a413 3666      	macw %d6u,%d3l,>>,%a3@&,%d2,%acc1
    4e90:	a493 3676      	macw %d6u,%d3l,>>,%a3@&,%d2,%acc2
    4e94:	ae53 3666      	macw %d6u,%d3l,>>,%a3@&,%sp,%acc1
    4e98:	aed3 3676      	macw %d6u,%d3l,>>,%a3@&,%sp,%acc2
    4e9c:	a21a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d1,%acc1
    4ea0:	a29a 3656      	macw %d6u,%d3l,>>,%a2@\+,%d1,%acc2
    4ea4:	a65a 3646      	macw %d6u,%d3l,>>,%a2@\+,%a3,%acc1
    4ea8:	a6da 3656      	macw %d6u,%d3l,>>,%a2@\+,%a3,%acc2
    4eac:	a41a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d2,%acc1
    4eb0:	a49a 3656      	macw %d6u,%d3l,>>,%a2@\+,%d2,%acc2
    4eb4:	ae5a 3646      	macw %d6u,%d3l,>>,%a2@\+,%sp,%acc1
    4eb8:	aeda 3656      	macw %d6u,%d3l,>>,%a2@\+,%sp,%acc2
    4ebc:	a21a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d1,%acc1
    4ec0:	a29a 3676      	macw %d6u,%d3l,>>,%a2@\+&,%d1,%acc2
    4ec4:	a65a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%a3,%acc1
    4ec8:	a6da 3676      	macw %d6u,%d3l,>>,%a2@\+&,%a3,%acc2
    4ecc:	a41a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d2,%acc1
    4ed0:	a49a 3676      	macw %d6u,%d3l,>>,%a2@\+&,%d2,%acc2
    4ed4:	ae5a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%sp,%acc1
    4ed8:	aeda 3676      	macw %d6u,%d3l,>>,%a2@\+&,%sp,%acc2
    4edc:	a22e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1,%acc1
    4ee2:	a2ae 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1,%acc2
    4ee8:	a66e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3,%acc1
    4eee:	a6ee 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3,%acc2
    4ef4:	a42e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2,%acc1
    4efa:	a4ae 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2,%acc2
    4f00:	ae6e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp,%acc1
    4f06:	aeee 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp,%acc2
    4f0c:	a22e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    4f12:	a2ae 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    4f18:	a66e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    4f1e:	a6ee 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    4f24:	a42e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    4f2a:	a4ae 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    4f30:	ae6e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    4f36:	aeee 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    4f3c:	a221 3646      	macw %d6u,%d3l,>>,%a1@-,%d1,%acc1
    4f40:	a2a1 3656      	macw %d6u,%d3l,>>,%a1@-,%d1,%acc2
    4f44:	a661 3646      	macw %d6u,%d3l,>>,%a1@-,%a3,%acc1
    4f48:	a6e1 3656      	macw %d6u,%d3l,>>,%a1@-,%a3,%acc2
    4f4c:	a421 3646      	macw %d6u,%d3l,>>,%a1@-,%d2,%acc1
    4f50:	a4a1 3656      	macw %d6u,%d3l,>>,%a1@-,%d2,%acc2
    4f54:	ae61 3646      	macw %d6u,%d3l,>>,%a1@-,%sp,%acc1
    4f58:	aee1 3656      	macw %d6u,%d3l,>>,%a1@-,%sp,%acc2
    4f5c:	a221 3666      	macw %d6u,%d3l,>>,%a1@-&,%d1,%acc1
    4f60:	a2a1 3676      	macw %d6u,%d3l,>>,%a1@-&,%d1,%acc2
    4f64:	a661 3666      	macw %d6u,%d3l,>>,%a1@-&,%a3,%acc1
    4f68:	a6e1 3676      	macw %d6u,%d3l,>>,%a1@-&,%a3,%acc2
    4f6c:	a421 3666      	macw %d6u,%d3l,>>,%a1@-&,%d2,%acc1
    4f70:	a4a1 3676      	macw %d6u,%d3l,>>,%a1@-&,%d2,%acc2
    4f74:	ae61 3666      	macw %d6u,%d3l,>>,%a1@-&,%sp,%acc1
    4f78:	aee1 3676      	macw %d6u,%d3l,>>,%a1@-&,%sp,%acc2
    4f7c:	a213 3246      	macw %d6u,%d3l,<<,%a3@,%d1,%acc1
    4f80:	a293 3256      	macw %d6u,%d3l,<<,%a3@,%d1,%acc2
    4f84:	a653 3246      	macw %d6u,%d3l,<<,%a3@,%a3,%acc1
    4f88:	a6d3 3256      	macw %d6u,%d3l,<<,%a3@,%a3,%acc2
    4f8c:	a413 3246      	macw %d6u,%d3l,<<,%a3@,%d2,%acc1
    4f90:	a493 3256      	macw %d6u,%d3l,<<,%a3@,%d2,%acc2
    4f94:	ae53 3246      	macw %d6u,%d3l,<<,%a3@,%sp,%acc1
    4f98:	aed3 3256      	macw %d6u,%d3l,<<,%a3@,%sp,%acc2
    4f9c:	a213 3266      	macw %d6u,%d3l,<<,%a3@&,%d1,%acc1
    4fa0:	a293 3276      	macw %d6u,%d3l,<<,%a3@&,%d1,%acc2
    4fa4:	a653 3266      	macw %d6u,%d3l,<<,%a3@&,%a3,%acc1
    4fa8:	a6d3 3276      	macw %d6u,%d3l,<<,%a3@&,%a3,%acc2
    4fac:	a413 3266      	macw %d6u,%d3l,<<,%a3@&,%d2,%acc1
    4fb0:	a493 3276      	macw %d6u,%d3l,<<,%a3@&,%d2,%acc2
    4fb4:	ae53 3266      	macw %d6u,%d3l,<<,%a3@&,%sp,%acc1
    4fb8:	aed3 3276      	macw %d6u,%d3l,<<,%a3@&,%sp,%acc2
    4fbc:	a21a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d1,%acc1
    4fc0:	a29a 3256      	macw %d6u,%d3l,<<,%a2@\+,%d1,%acc2
    4fc4:	a65a 3246      	macw %d6u,%d3l,<<,%a2@\+,%a3,%acc1
    4fc8:	a6da 3256      	macw %d6u,%d3l,<<,%a2@\+,%a3,%acc2
    4fcc:	a41a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d2,%acc1
    4fd0:	a49a 3256      	macw %d6u,%d3l,<<,%a2@\+,%d2,%acc2
    4fd4:	ae5a 3246      	macw %d6u,%d3l,<<,%a2@\+,%sp,%acc1
    4fd8:	aeda 3256      	macw %d6u,%d3l,<<,%a2@\+,%sp,%acc2
    4fdc:	a21a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d1,%acc1
    4fe0:	a29a 3276      	macw %d6u,%d3l,<<,%a2@\+&,%d1,%acc2
    4fe4:	a65a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%a3,%acc1
    4fe8:	a6da 3276      	macw %d6u,%d3l,<<,%a2@\+&,%a3,%acc2
    4fec:	a41a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d2,%acc1
    4ff0:	a49a 3276      	macw %d6u,%d3l,<<,%a2@\+&,%d2,%acc2
    4ff4:	ae5a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%sp,%acc1
    4ff8:	aeda 3276      	macw %d6u,%d3l,<<,%a2@\+&,%sp,%acc2
    4ffc:	a22e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1,%acc1
    5002:	a2ae 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1,%acc2
    5008:	a66e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3,%acc1
    500e:	a6ee 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3,%acc2
    5014:	a42e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2,%acc1
    501a:	a4ae 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2,%acc2
    5020:	ae6e 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp,%acc1
    5026:	aeee 3256 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp,%acc2
    502c:	a22e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1,%acc1
    5032:	a2ae 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1,%acc2
    5038:	a66e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3,%acc1
    503e:	a6ee 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3,%acc2
    5044:	a42e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2,%acc1
    504a:	a4ae 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2,%acc2
    5050:	ae6e 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp,%acc1
    5056:	aeee 3276 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp,%acc2
    505c:	a221 3246      	macw %d6u,%d3l,<<,%a1@-,%d1,%acc1
    5060:	a2a1 3256      	macw %d6u,%d3l,<<,%a1@-,%d1,%acc2
    5064:	a661 3246      	macw %d6u,%d3l,<<,%a1@-,%a3,%acc1
    5068:	a6e1 3256      	macw %d6u,%d3l,<<,%a1@-,%a3,%acc2
    506c:	a421 3246      	macw %d6u,%d3l,<<,%a1@-,%d2,%acc1
    5070:	a4a1 3256      	macw %d6u,%d3l,<<,%a1@-,%d2,%acc2
    5074:	ae61 3246      	macw %d6u,%d3l,<<,%a1@-,%sp,%acc1
    5078:	aee1 3256      	macw %d6u,%d3l,<<,%a1@-,%sp,%acc2
    507c:	a221 3266      	macw %d6u,%d3l,<<,%a1@-&,%d1,%acc1
    5080:	a2a1 3276      	macw %d6u,%d3l,<<,%a1@-&,%d1,%acc2
    5084:	a661 3266      	macw %d6u,%d3l,<<,%a1@-&,%a3,%acc1
    5088:	a6e1 3276      	macw %d6u,%d3l,<<,%a1@-&,%a3,%acc2
    508c:	a421 3266      	macw %d6u,%d3l,<<,%a1@-&,%d2,%acc1
    5090:	a4a1 3276      	macw %d6u,%d3l,<<,%a1@-&,%d2,%acc2
    5094:	ae61 3266      	macw %d6u,%d3l,<<,%a1@-&,%sp,%acc1
    5098:	aee1 3276      	macw %d6u,%d3l,<<,%a1@-&,%sp,%acc2
    509c:	a213 3646      	macw %d6u,%d3l,>>,%a3@,%d1,%acc1
    50a0:	a293 3656      	macw %d6u,%d3l,>>,%a3@,%d1,%acc2
    50a4:	a653 3646      	macw %d6u,%d3l,>>,%a3@,%a3,%acc1
    50a8:	a6d3 3656      	macw %d6u,%d3l,>>,%a3@,%a3,%acc2
    50ac:	a413 3646      	macw %d6u,%d3l,>>,%a3@,%d2,%acc1
    50b0:	a493 3656      	macw %d6u,%d3l,>>,%a3@,%d2,%acc2
    50b4:	ae53 3646      	macw %d6u,%d3l,>>,%a3@,%sp,%acc1
    50b8:	aed3 3656      	macw %d6u,%d3l,>>,%a3@,%sp,%acc2
    50bc:	a213 3666      	macw %d6u,%d3l,>>,%a3@&,%d1,%acc1
    50c0:	a293 3676      	macw %d6u,%d3l,>>,%a3@&,%d1,%acc2
    50c4:	a653 3666      	macw %d6u,%d3l,>>,%a3@&,%a3,%acc1
    50c8:	a6d3 3676      	macw %d6u,%d3l,>>,%a3@&,%a3,%acc2
    50cc:	a413 3666      	macw %d6u,%d3l,>>,%a3@&,%d2,%acc1
    50d0:	a493 3676      	macw %d6u,%d3l,>>,%a3@&,%d2,%acc2
    50d4:	ae53 3666      	macw %d6u,%d3l,>>,%a3@&,%sp,%acc1
    50d8:	aed3 3676      	macw %d6u,%d3l,>>,%a3@&,%sp,%acc2
    50dc:	a21a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d1,%acc1
    50e0:	a29a 3656      	macw %d6u,%d3l,>>,%a2@\+,%d1,%acc2
    50e4:	a65a 3646      	macw %d6u,%d3l,>>,%a2@\+,%a3,%acc1
    50e8:	a6da 3656      	macw %d6u,%d3l,>>,%a2@\+,%a3,%acc2
    50ec:	a41a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d2,%acc1
    50f0:	a49a 3656      	macw %d6u,%d3l,>>,%a2@\+,%d2,%acc2
    50f4:	ae5a 3646      	macw %d6u,%d3l,>>,%a2@\+,%sp,%acc1
    50f8:	aeda 3656      	macw %d6u,%d3l,>>,%a2@\+,%sp,%acc2
    50fc:	a21a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d1,%acc1
    5100:	a29a 3676      	macw %d6u,%d3l,>>,%a2@\+&,%d1,%acc2
    5104:	a65a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%a3,%acc1
    5108:	a6da 3676      	macw %d6u,%d3l,>>,%a2@\+&,%a3,%acc2
    510c:	a41a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d2,%acc1
    5110:	a49a 3676      	macw %d6u,%d3l,>>,%a2@\+&,%d2,%acc2
    5114:	ae5a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%sp,%acc1
    5118:	aeda 3676      	macw %d6u,%d3l,>>,%a2@\+&,%sp,%acc2
    511c:	a22e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1,%acc1
    5122:	a2ae 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1,%acc2
    5128:	a66e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3,%acc1
    512e:	a6ee 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3,%acc2
    5134:	a42e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2,%acc1
    513a:	a4ae 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2,%acc2
    5140:	ae6e 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp,%acc1
    5146:	aeee 3656 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp,%acc2
    514c:	a22e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1,%acc1
    5152:	a2ae 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1,%acc2
    5158:	a66e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3,%acc1
    515e:	a6ee 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3,%acc2
    5164:	a42e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2,%acc1
    516a:	a4ae 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2,%acc2
    5170:	ae6e 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp,%acc1
    5176:	aeee 3676 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp,%acc2
    517c:	a221 3646      	macw %d6u,%d3l,>>,%a1@-,%d1,%acc1
    5180:	a2a1 3656      	macw %d6u,%d3l,>>,%a1@-,%d1,%acc2
    5184:	a661 3646      	macw %d6u,%d3l,>>,%a1@-,%a3,%acc1
    5188:	a6e1 3656      	macw %d6u,%d3l,>>,%a1@-,%a3,%acc2
    518c:	a421 3646      	macw %d6u,%d3l,>>,%a1@-,%d2,%acc1
    5190:	a4a1 3656      	macw %d6u,%d3l,>>,%a1@-,%d2,%acc2
    5194:	ae61 3646      	macw %d6u,%d3l,>>,%a1@-,%sp,%acc1
    5198:	aee1 3656      	macw %d6u,%d3l,>>,%a1@-,%sp,%acc2
    519c:	a221 3666      	macw %d6u,%d3l,>>,%a1@-&,%d1,%acc1
    51a0:	a2a1 3676      	macw %d6u,%d3l,>>,%a1@-&,%d1,%acc2
    51a4:	a661 3666      	macw %d6u,%d3l,>>,%a1@-&,%a3,%acc1
    51a8:	a6e1 3676      	macw %d6u,%d3l,>>,%a1@-&,%a3,%acc2
    51ac:	a421 3666      	macw %d6u,%d3l,>>,%a1@-&,%d2,%acc1
    51b0:	a4a1 3676      	macw %d6u,%d3l,>>,%a1@-&,%d2,%acc2
    51b4:	ae61 3666      	macw %d6u,%d3l,>>,%a1@-&,%sp,%acc1
    51b8:	aee1 3676      	macw %d6u,%d3l,>>,%a1@-&,%sp,%acc2
    51bc:	a213 f0c6      	macw %d6u,%a7u,%a3@,%d1,%acc1
    51c0:	a293 f0d6      	macw %d6u,%a7u,%a3@,%d1,%acc2
    51c4:	a653 f0c6      	macw %d6u,%a7u,%a3@,%a3,%acc1
    51c8:	a6d3 f0d6      	macw %d6u,%a7u,%a3@,%a3,%acc2
    51cc:	a413 f0c6      	macw %d6u,%a7u,%a3@,%d2,%acc1
    51d0:	a493 f0d6      	macw %d6u,%a7u,%a3@,%d2,%acc2
    51d4:	ae53 f0c6      	macw %d6u,%a7u,%a3@,%sp,%acc1
    51d8:	aed3 f0d6      	macw %d6u,%a7u,%a3@,%sp,%acc2
    51dc:	a213 f0e6      	macw %d6u,%a7u,%a3@&,%d1,%acc1
    51e0:	a293 f0f6      	macw %d6u,%a7u,%a3@&,%d1,%acc2
    51e4:	a653 f0e6      	macw %d6u,%a7u,%a3@&,%a3,%acc1
    51e8:	a6d3 f0f6      	macw %d6u,%a7u,%a3@&,%a3,%acc2
    51ec:	a413 f0e6      	macw %d6u,%a7u,%a3@&,%d2,%acc1
    51f0:	a493 f0f6      	macw %d6u,%a7u,%a3@&,%d2,%acc2
    51f4:	ae53 f0e6      	macw %d6u,%a7u,%a3@&,%sp,%acc1
    51f8:	aed3 f0f6      	macw %d6u,%a7u,%a3@&,%sp,%acc2
    51fc:	a21a f0c6      	macw %d6u,%a7u,%a2@\+,%d1,%acc1
    5200:	a29a f0d6      	macw %d6u,%a7u,%a2@\+,%d1,%acc2
    5204:	a65a f0c6      	macw %d6u,%a7u,%a2@\+,%a3,%acc1
    5208:	a6da f0d6      	macw %d6u,%a7u,%a2@\+,%a3,%acc2
    520c:	a41a f0c6      	macw %d6u,%a7u,%a2@\+,%d2,%acc1
    5210:	a49a f0d6      	macw %d6u,%a7u,%a2@\+,%d2,%acc2
    5214:	ae5a f0c6      	macw %d6u,%a7u,%a2@\+,%sp,%acc1
    5218:	aeda f0d6      	macw %d6u,%a7u,%a2@\+,%sp,%acc2
    521c:	a21a f0e6      	macw %d6u,%a7u,%a2@\+&,%d1,%acc1
    5220:	a29a f0f6      	macw %d6u,%a7u,%a2@\+&,%d1,%acc2
    5224:	a65a f0e6      	macw %d6u,%a7u,%a2@\+&,%a3,%acc1
    5228:	a6da f0f6      	macw %d6u,%a7u,%a2@\+&,%a3,%acc2
    522c:	a41a f0e6      	macw %d6u,%a7u,%a2@\+&,%d2,%acc1
    5230:	a49a f0f6      	macw %d6u,%a7u,%a2@\+&,%d2,%acc2
    5234:	ae5a f0e6      	macw %d6u,%a7u,%a2@\+&,%sp,%acc1
    5238:	aeda f0f6      	macw %d6u,%a7u,%a2@\+&,%sp,%acc2
    523c:	a22e f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%d1,%acc1
    5242:	a2ae f0d6 000a 	macw %d6u,%a7u,%fp@\(10\),%d1,%acc2
    5248:	a66e f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%a3,%acc1
    524e:	a6ee f0d6 000a 	macw %d6u,%a7u,%fp@\(10\),%a3,%acc2
    5254:	a42e f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%d2,%acc1
    525a:	a4ae f0d6 000a 	macw %d6u,%a7u,%fp@\(10\),%d2,%acc2
    5260:	ae6e f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%sp,%acc1
    5266:	aeee f0d6 000a 	macw %d6u,%a7u,%fp@\(10\),%sp,%acc2
    526c:	a22e f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d1,%acc1
    5272:	a2ae f0f6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d1,%acc2
    5278:	a66e f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%a3,%acc1
    527e:	a6ee f0f6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%a3,%acc2
    5284:	a42e f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d2,%acc1
    528a:	a4ae f0f6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d2,%acc2
    5290:	ae6e f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%sp,%acc1
    5296:	aeee f0f6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%sp,%acc2
    529c:	a221 f0c6      	macw %d6u,%a7u,%a1@-,%d1,%acc1
    52a0:	a2a1 f0d6      	macw %d6u,%a7u,%a1@-,%d1,%acc2
    52a4:	a661 f0c6      	macw %d6u,%a7u,%a1@-,%a3,%acc1
    52a8:	a6e1 f0d6      	macw %d6u,%a7u,%a1@-,%a3,%acc2
    52ac:	a421 f0c6      	macw %d6u,%a7u,%a1@-,%d2,%acc1
    52b0:	a4a1 f0d6      	macw %d6u,%a7u,%a1@-,%d2,%acc2
    52b4:	ae61 f0c6      	macw %d6u,%a7u,%a1@-,%sp,%acc1
    52b8:	aee1 f0d6      	macw %d6u,%a7u,%a1@-,%sp,%acc2
    52bc:	a221 f0e6      	macw %d6u,%a7u,%a1@-&,%d1,%acc1
    52c0:	a2a1 f0f6      	macw %d6u,%a7u,%a1@-&,%d1,%acc2
    52c4:	a661 f0e6      	macw %d6u,%a7u,%a1@-&,%a3,%acc1
    52c8:	a6e1 f0f6      	macw %d6u,%a7u,%a1@-&,%a3,%acc2
    52cc:	a421 f0e6      	macw %d6u,%a7u,%a1@-&,%d2,%acc1
    52d0:	a4a1 f0f6      	macw %d6u,%a7u,%a1@-&,%d2,%acc2
    52d4:	ae61 f0e6      	macw %d6u,%a7u,%a1@-&,%sp,%acc1
    52d8:	aee1 f0f6      	macw %d6u,%a7u,%a1@-&,%sp,%acc2
    52dc:	a213 f2c6      	macw %d6u,%a7u,<<,%a3@,%d1,%acc1
    52e0:	a293 f2d6      	macw %d6u,%a7u,<<,%a3@,%d1,%acc2
    52e4:	a653 f2c6      	macw %d6u,%a7u,<<,%a3@,%a3,%acc1
    52e8:	a6d3 f2d6      	macw %d6u,%a7u,<<,%a3@,%a3,%acc2
    52ec:	a413 f2c6      	macw %d6u,%a7u,<<,%a3@,%d2,%acc1
    52f0:	a493 f2d6      	macw %d6u,%a7u,<<,%a3@,%d2,%acc2
    52f4:	ae53 f2c6      	macw %d6u,%a7u,<<,%a3@,%sp,%acc1
    52f8:	aed3 f2d6      	macw %d6u,%a7u,<<,%a3@,%sp,%acc2
    52fc:	a213 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d1,%acc1
    5300:	a293 f2f6      	macw %d6u,%a7u,<<,%a3@&,%d1,%acc2
    5304:	a653 f2e6      	macw %d6u,%a7u,<<,%a3@&,%a3,%acc1
    5308:	a6d3 f2f6      	macw %d6u,%a7u,<<,%a3@&,%a3,%acc2
    530c:	a413 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d2,%acc1
    5310:	a493 f2f6      	macw %d6u,%a7u,<<,%a3@&,%d2,%acc2
    5314:	ae53 f2e6      	macw %d6u,%a7u,<<,%a3@&,%sp,%acc1
    5318:	aed3 f2f6      	macw %d6u,%a7u,<<,%a3@&,%sp,%acc2
    531c:	a21a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d1,%acc1
    5320:	a29a f2d6      	macw %d6u,%a7u,<<,%a2@\+,%d1,%acc2
    5324:	a65a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%a3,%acc1
    5328:	a6da f2d6      	macw %d6u,%a7u,<<,%a2@\+,%a3,%acc2
    532c:	a41a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d2,%acc1
    5330:	a49a f2d6      	macw %d6u,%a7u,<<,%a2@\+,%d2,%acc2
    5334:	ae5a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%sp,%acc1
    5338:	aeda f2d6      	macw %d6u,%a7u,<<,%a2@\+,%sp,%acc2
    533c:	a21a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d1,%acc1
    5340:	a29a f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%d1,%acc2
    5344:	a65a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%a3,%acc1
    5348:	a6da f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%a3,%acc2
    534c:	a41a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d2,%acc1
    5350:	a49a f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%d2,%acc2
    5354:	ae5a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%sp,%acc1
    5358:	aeda f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%sp,%acc2
    535c:	a22e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1,%acc1
    5362:	a2ae f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1,%acc2
    5368:	a66e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3,%acc1
    536e:	a6ee f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3,%acc2
    5374:	a42e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2,%acc1
    537a:	a4ae f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2,%acc2
    5380:	ae6e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp,%acc1
    5386:	aeee f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp,%acc2
    538c:	a22e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    5392:	a2ae f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    5398:	a66e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    539e:	a6ee f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    53a4:	a42e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    53aa:	a4ae f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    53b0:	ae6e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    53b6:	aeee f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    53bc:	a221 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d1,%acc1
    53c0:	a2a1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%d1,%acc2
    53c4:	a661 f2c6      	macw %d6u,%a7u,<<,%a1@-,%a3,%acc1
    53c8:	a6e1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%a3,%acc2
    53cc:	a421 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d2,%acc1
    53d0:	a4a1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%d2,%acc2
    53d4:	ae61 f2c6      	macw %d6u,%a7u,<<,%a1@-,%sp,%acc1
    53d8:	aee1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%sp,%acc2
    53dc:	a221 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d1,%acc1
    53e0:	a2a1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%d1,%acc2
    53e4:	a661 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%a3,%acc1
    53e8:	a6e1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%a3,%acc2
    53ec:	a421 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d2,%acc1
    53f0:	a4a1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%d2,%acc2
    53f4:	ae61 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%sp,%acc1
    53f8:	aee1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%sp,%acc2
    53fc:	a213 f6c6      	macw %d6u,%a7u,>>,%a3@,%d1,%acc1
    5400:	a293 f6d6      	macw %d6u,%a7u,>>,%a3@,%d1,%acc2
    5404:	a653 f6c6      	macw %d6u,%a7u,>>,%a3@,%a3,%acc1
    5408:	a6d3 f6d6      	macw %d6u,%a7u,>>,%a3@,%a3,%acc2
    540c:	a413 f6c6      	macw %d6u,%a7u,>>,%a3@,%d2,%acc1
    5410:	a493 f6d6      	macw %d6u,%a7u,>>,%a3@,%d2,%acc2
    5414:	ae53 f6c6      	macw %d6u,%a7u,>>,%a3@,%sp,%acc1
    5418:	aed3 f6d6      	macw %d6u,%a7u,>>,%a3@,%sp,%acc2
    541c:	a213 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d1,%acc1
    5420:	a293 f6f6      	macw %d6u,%a7u,>>,%a3@&,%d1,%acc2
    5424:	a653 f6e6      	macw %d6u,%a7u,>>,%a3@&,%a3,%acc1
    5428:	a6d3 f6f6      	macw %d6u,%a7u,>>,%a3@&,%a3,%acc2
    542c:	a413 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d2,%acc1
    5430:	a493 f6f6      	macw %d6u,%a7u,>>,%a3@&,%d2,%acc2
    5434:	ae53 f6e6      	macw %d6u,%a7u,>>,%a3@&,%sp,%acc1
    5438:	aed3 f6f6      	macw %d6u,%a7u,>>,%a3@&,%sp,%acc2
    543c:	a21a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d1,%acc1
    5440:	a29a f6d6      	macw %d6u,%a7u,>>,%a2@\+,%d1,%acc2
    5444:	a65a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%a3,%acc1
    5448:	a6da f6d6      	macw %d6u,%a7u,>>,%a2@\+,%a3,%acc2
    544c:	a41a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d2,%acc1
    5450:	a49a f6d6      	macw %d6u,%a7u,>>,%a2@\+,%d2,%acc2
    5454:	ae5a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%sp,%acc1
    5458:	aeda f6d6      	macw %d6u,%a7u,>>,%a2@\+,%sp,%acc2
    545c:	a21a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d1,%acc1
    5460:	a29a f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%d1,%acc2
    5464:	a65a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%a3,%acc1
    5468:	a6da f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%a3,%acc2
    546c:	a41a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d2,%acc1
    5470:	a49a f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%d2,%acc2
    5474:	ae5a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%sp,%acc1
    5478:	aeda f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%sp,%acc2
    547c:	a22e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1,%acc1
    5482:	a2ae f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1,%acc2
    5488:	a66e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3,%acc1
    548e:	a6ee f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3,%acc2
    5494:	a42e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2,%acc1
    549a:	a4ae f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2,%acc2
    54a0:	ae6e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp,%acc1
    54a6:	aeee f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp,%acc2
    54ac:	a22e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    54b2:	a2ae f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    54b8:	a66e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    54be:	a6ee f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    54c4:	a42e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    54ca:	a4ae f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    54d0:	ae6e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    54d6:	aeee f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    54dc:	a221 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d1,%acc1
    54e0:	a2a1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%d1,%acc2
    54e4:	a661 f6c6      	macw %d6u,%a7u,>>,%a1@-,%a3,%acc1
    54e8:	a6e1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%a3,%acc2
    54ec:	a421 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d2,%acc1
    54f0:	a4a1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%d2,%acc2
    54f4:	ae61 f6c6      	macw %d6u,%a7u,>>,%a1@-,%sp,%acc1
    54f8:	aee1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%sp,%acc2
    54fc:	a221 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d1,%acc1
    5500:	a2a1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%d1,%acc2
    5504:	a661 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%a3,%acc1
    5508:	a6e1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%a3,%acc2
    550c:	a421 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d2,%acc1
    5510:	a4a1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%d2,%acc2
    5514:	ae61 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%sp,%acc1
    5518:	aee1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%sp,%acc2
    551c:	a213 f2c6      	macw %d6u,%a7u,<<,%a3@,%d1,%acc1
    5520:	a293 f2d6      	macw %d6u,%a7u,<<,%a3@,%d1,%acc2
    5524:	a653 f2c6      	macw %d6u,%a7u,<<,%a3@,%a3,%acc1
    5528:	a6d3 f2d6      	macw %d6u,%a7u,<<,%a3@,%a3,%acc2
    552c:	a413 f2c6      	macw %d6u,%a7u,<<,%a3@,%d2,%acc1
    5530:	a493 f2d6      	macw %d6u,%a7u,<<,%a3@,%d2,%acc2
    5534:	ae53 f2c6      	macw %d6u,%a7u,<<,%a3@,%sp,%acc1
    5538:	aed3 f2d6      	macw %d6u,%a7u,<<,%a3@,%sp,%acc2
    553c:	a213 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d1,%acc1
    5540:	a293 f2f6      	macw %d6u,%a7u,<<,%a3@&,%d1,%acc2
    5544:	a653 f2e6      	macw %d6u,%a7u,<<,%a3@&,%a3,%acc1
    5548:	a6d3 f2f6      	macw %d6u,%a7u,<<,%a3@&,%a3,%acc2
    554c:	a413 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d2,%acc1
    5550:	a493 f2f6      	macw %d6u,%a7u,<<,%a3@&,%d2,%acc2
    5554:	ae53 f2e6      	macw %d6u,%a7u,<<,%a3@&,%sp,%acc1
    5558:	aed3 f2f6      	macw %d6u,%a7u,<<,%a3@&,%sp,%acc2
    555c:	a21a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d1,%acc1
    5560:	a29a f2d6      	macw %d6u,%a7u,<<,%a2@\+,%d1,%acc2
    5564:	a65a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%a3,%acc1
    5568:	a6da f2d6      	macw %d6u,%a7u,<<,%a2@\+,%a3,%acc2
    556c:	a41a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d2,%acc1
    5570:	a49a f2d6      	macw %d6u,%a7u,<<,%a2@\+,%d2,%acc2
    5574:	ae5a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%sp,%acc1
    5578:	aeda f2d6      	macw %d6u,%a7u,<<,%a2@\+,%sp,%acc2
    557c:	a21a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d1,%acc1
    5580:	a29a f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%d1,%acc2
    5584:	a65a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%a3,%acc1
    5588:	a6da f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%a3,%acc2
    558c:	a41a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d2,%acc1
    5590:	a49a f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%d2,%acc2
    5594:	ae5a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%sp,%acc1
    5598:	aeda f2f6      	macw %d6u,%a7u,<<,%a2@\+&,%sp,%acc2
    559c:	a22e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1,%acc1
    55a2:	a2ae f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1,%acc2
    55a8:	a66e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3,%acc1
    55ae:	a6ee f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3,%acc2
    55b4:	a42e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2,%acc1
    55ba:	a4ae f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2,%acc2
    55c0:	ae6e f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp,%acc1
    55c6:	aeee f2d6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp,%acc2
    55cc:	a22e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1,%acc1
    55d2:	a2ae f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1,%acc2
    55d8:	a66e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3,%acc1
    55de:	a6ee f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3,%acc2
    55e4:	a42e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2,%acc1
    55ea:	a4ae f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2,%acc2
    55f0:	ae6e f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp,%acc1
    55f6:	aeee f2f6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp,%acc2
    55fc:	a221 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d1,%acc1
    5600:	a2a1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%d1,%acc2
    5604:	a661 f2c6      	macw %d6u,%a7u,<<,%a1@-,%a3,%acc1
    5608:	a6e1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%a3,%acc2
    560c:	a421 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d2,%acc1
    5610:	a4a1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%d2,%acc2
    5614:	ae61 f2c6      	macw %d6u,%a7u,<<,%a1@-,%sp,%acc1
    5618:	aee1 f2d6      	macw %d6u,%a7u,<<,%a1@-,%sp,%acc2
    561c:	a221 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d1,%acc1
    5620:	a2a1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%d1,%acc2
    5624:	a661 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%a3,%acc1
    5628:	a6e1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%a3,%acc2
    562c:	a421 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d2,%acc1
    5630:	a4a1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%d2,%acc2
    5634:	ae61 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%sp,%acc1
    5638:	aee1 f2f6      	macw %d6u,%a7u,<<,%a1@-&,%sp,%acc2
    563c:	a213 f6c6      	macw %d6u,%a7u,>>,%a3@,%d1,%acc1
    5640:	a293 f6d6      	macw %d6u,%a7u,>>,%a3@,%d1,%acc2
    5644:	a653 f6c6      	macw %d6u,%a7u,>>,%a3@,%a3,%acc1
    5648:	a6d3 f6d6      	macw %d6u,%a7u,>>,%a3@,%a3,%acc2
    564c:	a413 f6c6      	macw %d6u,%a7u,>>,%a3@,%d2,%acc1
    5650:	a493 f6d6      	macw %d6u,%a7u,>>,%a3@,%d2,%acc2
    5654:	ae53 f6c6      	macw %d6u,%a7u,>>,%a3@,%sp,%acc1
    5658:	aed3 f6d6      	macw %d6u,%a7u,>>,%a3@,%sp,%acc2
    565c:	a213 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d1,%acc1
    5660:	a293 f6f6      	macw %d6u,%a7u,>>,%a3@&,%d1,%acc2
    5664:	a653 f6e6      	macw %d6u,%a7u,>>,%a3@&,%a3,%acc1
    5668:	a6d3 f6f6      	macw %d6u,%a7u,>>,%a3@&,%a3,%acc2
    566c:	a413 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d2,%acc1
    5670:	a493 f6f6      	macw %d6u,%a7u,>>,%a3@&,%d2,%acc2
    5674:	ae53 f6e6      	macw %d6u,%a7u,>>,%a3@&,%sp,%acc1
    5678:	aed3 f6f6      	macw %d6u,%a7u,>>,%a3@&,%sp,%acc2
    567c:	a21a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d1,%acc1
    5680:	a29a f6d6      	macw %d6u,%a7u,>>,%a2@\+,%d1,%acc2
    5684:	a65a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%a3,%acc1
    5688:	a6da f6d6      	macw %d6u,%a7u,>>,%a2@\+,%a3,%acc2
    568c:	a41a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d2,%acc1
    5690:	a49a f6d6      	macw %d6u,%a7u,>>,%a2@\+,%d2,%acc2
    5694:	ae5a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%sp,%acc1
    5698:	aeda f6d6      	macw %d6u,%a7u,>>,%a2@\+,%sp,%acc2
    569c:	a21a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d1,%acc1
    56a0:	a29a f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%d1,%acc2
    56a4:	a65a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%a3,%acc1
    56a8:	a6da f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%a3,%acc2
    56ac:	a41a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d2,%acc1
    56b0:	a49a f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%d2,%acc2
    56b4:	ae5a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%sp,%acc1
    56b8:	aeda f6f6      	macw %d6u,%a7u,>>,%a2@\+&,%sp,%acc2
    56bc:	a22e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1,%acc1
    56c2:	a2ae f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1,%acc2
    56c8:	a66e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3,%acc1
    56ce:	a6ee f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3,%acc2
    56d4:	a42e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2,%acc1
    56da:	a4ae f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2,%acc2
    56e0:	ae6e f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp,%acc1
    56e6:	aeee f6d6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp,%acc2
    56ec:	a22e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1,%acc1
    56f2:	a2ae f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1,%acc2
    56f8:	a66e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3,%acc1
    56fe:	a6ee f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3,%acc2
    5704:	a42e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2,%acc1
    570a:	a4ae f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2,%acc2
    5710:	ae6e f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp,%acc1
    5716:	aeee f6f6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp,%acc2
    571c:	a221 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d1,%acc1
    5720:	a2a1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%d1,%acc2
    5724:	a661 f6c6      	macw %d6u,%a7u,>>,%a1@-,%a3,%acc1
    5728:	a6e1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%a3,%acc2
    572c:	a421 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d2,%acc1
    5730:	a4a1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%d2,%acc2
    5734:	ae61 f6c6      	macw %d6u,%a7u,>>,%a1@-,%sp,%acc1
    5738:	aee1 f6d6      	macw %d6u,%a7u,>>,%a1@-,%sp,%acc2
    573c:	a221 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d1,%acc1
    5740:	a2a1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%d1,%acc2
    5744:	a661 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%a3,%acc1
    5748:	a6e1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%a3,%acc2
    574c:	a421 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d2,%acc1
    5750:	a4a1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%d2,%acc2
    5754:	ae61 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%sp,%acc1
    5758:	aee1 f6f6      	macw %d6u,%a7u,>>,%a1@-&,%sp,%acc2
    575c:	a213 1046      	macw %d6u,%d1l,%a3@,%d1,%acc1
    5760:	a293 1056      	macw %d6u,%d1l,%a3@,%d1,%acc2
    5764:	a653 1046      	macw %d6u,%d1l,%a3@,%a3,%acc1
    5768:	a6d3 1056      	macw %d6u,%d1l,%a3@,%a3,%acc2
    576c:	a413 1046      	macw %d6u,%d1l,%a3@,%d2,%acc1
    5770:	a493 1056      	macw %d6u,%d1l,%a3@,%d2,%acc2
    5774:	ae53 1046      	macw %d6u,%d1l,%a3@,%sp,%acc1
    5778:	aed3 1056      	macw %d6u,%d1l,%a3@,%sp,%acc2
    577c:	a213 1066      	macw %d6u,%d1l,%a3@&,%d1,%acc1
    5780:	a293 1076      	macw %d6u,%d1l,%a3@&,%d1,%acc2
    5784:	a653 1066      	macw %d6u,%d1l,%a3@&,%a3,%acc1
    5788:	a6d3 1076      	macw %d6u,%d1l,%a3@&,%a3,%acc2
    578c:	a413 1066      	macw %d6u,%d1l,%a3@&,%d2,%acc1
    5790:	a493 1076      	macw %d6u,%d1l,%a3@&,%d2,%acc2
    5794:	ae53 1066      	macw %d6u,%d1l,%a3@&,%sp,%acc1
    5798:	aed3 1076      	macw %d6u,%d1l,%a3@&,%sp,%acc2
    579c:	a21a 1046      	macw %d6u,%d1l,%a2@\+,%d1,%acc1
    57a0:	a29a 1056      	macw %d6u,%d1l,%a2@\+,%d1,%acc2
    57a4:	a65a 1046      	macw %d6u,%d1l,%a2@\+,%a3,%acc1
    57a8:	a6da 1056      	macw %d6u,%d1l,%a2@\+,%a3,%acc2
    57ac:	a41a 1046      	macw %d6u,%d1l,%a2@\+,%d2,%acc1
    57b0:	a49a 1056      	macw %d6u,%d1l,%a2@\+,%d2,%acc2
    57b4:	ae5a 1046      	macw %d6u,%d1l,%a2@\+,%sp,%acc1
    57b8:	aeda 1056      	macw %d6u,%d1l,%a2@\+,%sp,%acc2
    57bc:	a21a 1066      	macw %d6u,%d1l,%a2@\+&,%d1,%acc1
    57c0:	a29a 1076      	macw %d6u,%d1l,%a2@\+&,%d1,%acc2
    57c4:	a65a 1066      	macw %d6u,%d1l,%a2@\+&,%a3,%acc1
    57c8:	a6da 1076      	macw %d6u,%d1l,%a2@\+&,%a3,%acc2
    57cc:	a41a 1066      	macw %d6u,%d1l,%a2@\+&,%d2,%acc1
    57d0:	a49a 1076      	macw %d6u,%d1l,%a2@\+&,%d2,%acc2
    57d4:	ae5a 1066      	macw %d6u,%d1l,%a2@\+&,%sp,%acc1
    57d8:	aeda 1076      	macw %d6u,%d1l,%a2@\+&,%sp,%acc2
    57dc:	a22e 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%d1,%acc1
    57e2:	a2ae 1056 000a 	macw %d6u,%d1l,%fp@\(10\),%d1,%acc2
    57e8:	a66e 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%a3,%acc1
    57ee:	a6ee 1056 000a 	macw %d6u,%d1l,%fp@\(10\),%a3,%acc2
    57f4:	a42e 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%d2,%acc1
    57fa:	a4ae 1056 000a 	macw %d6u,%d1l,%fp@\(10\),%d2,%acc2
    5800:	ae6e 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%sp,%acc1
    5806:	aeee 1056 000a 	macw %d6u,%d1l,%fp@\(10\),%sp,%acc2
    580c:	a22e 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d1,%acc1
    5812:	a2ae 1076 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d1,%acc2
    5818:	a66e 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%a3,%acc1
    581e:	a6ee 1076 000a 	macw %d6u,%d1l,%fp@\(10\)&,%a3,%acc2
    5824:	a42e 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d2,%acc1
    582a:	a4ae 1076 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d2,%acc2
    5830:	ae6e 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%sp,%acc1
    5836:	aeee 1076 000a 	macw %d6u,%d1l,%fp@\(10\)&,%sp,%acc2
    583c:	a221 1046      	macw %d6u,%d1l,%a1@-,%d1,%acc1
    5840:	a2a1 1056      	macw %d6u,%d1l,%a1@-,%d1,%acc2
    5844:	a661 1046      	macw %d6u,%d1l,%a1@-,%a3,%acc1
    5848:	a6e1 1056      	macw %d6u,%d1l,%a1@-,%a3,%acc2
    584c:	a421 1046      	macw %d6u,%d1l,%a1@-,%d2,%acc1
    5850:	a4a1 1056      	macw %d6u,%d1l,%a1@-,%d2,%acc2
    5854:	ae61 1046      	macw %d6u,%d1l,%a1@-,%sp,%acc1
    5858:	aee1 1056      	macw %d6u,%d1l,%a1@-,%sp,%acc2
    585c:	a221 1066      	macw %d6u,%d1l,%a1@-&,%d1,%acc1
    5860:	a2a1 1076      	macw %d6u,%d1l,%a1@-&,%d1,%acc2
    5864:	a661 1066      	macw %d6u,%d1l,%a1@-&,%a3,%acc1
    5868:	a6e1 1076      	macw %d6u,%d1l,%a1@-&,%a3,%acc2
    586c:	a421 1066      	macw %d6u,%d1l,%a1@-&,%d2,%acc1
    5870:	a4a1 1076      	macw %d6u,%d1l,%a1@-&,%d2,%acc2
    5874:	ae61 1066      	macw %d6u,%d1l,%a1@-&,%sp,%acc1
    5878:	aee1 1076      	macw %d6u,%d1l,%a1@-&,%sp,%acc2
    587c:	a213 1246      	macw %d6u,%d1l,<<,%a3@,%d1,%acc1
    5880:	a293 1256      	macw %d6u,%d1l,<<,%a3@,%d1,%acc2
    5884:	a653 1246      	macw %d6u,%d1l,<<,%a3@,%a3,%acc1
    5888:	a6d3 1256      	macw %d6u,%d1l,<<,%a3@,%a3,%acc2
    588c:	a413 1246      	macw %d6u,%d1l,<<,%a3@,%d2,%acc1
    5890:	a493 1256      	macw %d6u,%d1l,<<,%a3@,%d2,%acc2
    5894:	ae53 1246      	macw %d6u,%d1l,<<,%a3@,%sp,%acc1
    5898:	aed3 1256      	macw %d6u,%d1l,<<,%a3@,%sp,%acc2
    589c:	a213 1266      	macw %d6u,%d1l,<<,%a3@&,%d1,%acc1
    58a0:	a293 1276      	macw %d6u,%d1l,<<,%a3@&,%d1,%acc2
    58a4:	a653 1266      	macw %d6u,%d1l,<<,%a3@&,%a3,%acc1
    58a8:	a6d3 1276      	macw %d6u,%d1l,<<,%a3@&,%a3,%acc2
    58ac:	a413 1266      	macw %d6u,%d1l,<<,%a3@&,%d2,%acc1
    58b0:	a493 1276      	macw %d6u,%d1l,<<,%a3@&,%d2,%acc2
    58b4:	ae53 1266      	macw %d6u,%d1l,<<,%a3@&,%sp,%acc1
    58b8:	aed3 1276      	macw %d6u,%d1l,<<,%a3@&,%sp,%acc2
    58bc:	a21a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d1,%acc1
    58c0:	a29a 1256      	macw %d6u,%d1l,<<,%a2@\+,%d1,%acc2
    58c4:	a65a 1246      	macw %d6u,%d1l,<<,%a2@\+,%a3,%acc1
    58c8:	a6da 1256      	macw %d6u,%d1l,<<,%a2@\+,%a3,%acc2
    58cc:	a41a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d2,%acc1
    58d0:	a49a 1256      	macw %d6u,%d1l,<<,%a2@\+,%d2,%acc2
    58d4:	ae5a 1246      	macw %d6u,%d1l,<<,%a2@\+,%sp,%acc1
    58d8:	aeda 1256      	macw %d6u,%d1l,<<,%a2@\+,%sp,%acc2
    58dc:	a21a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d1,%acc1
    58e0:	a29a 1276      	macw %d6u,%d1l,<<,%a2@\+&,%d1,%acc2
    58e4:	a65a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%a3,%acc1
    58e8:	a6da 1276      	macw %d6u,%d1l,<<,%a2@\+&,%a3,%acc2
    58ec:	a41a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d2,%acc1
    58f0:	a49a 1276      	macw %d6u,%d1l,<<,%a2@\+&,%d2,%acc2
    58f4:	ae5a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%sp,%acc1
    58f8:	aeda 1276      	macw %d6u,%d1l,<<,%a2@\+&,%sp,%acc2
    58fc:	a22e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1,%acc1
    5902:	a2ae 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1,%acc2
    5908:	a66e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3,%acc1
    590e:	a6ee 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3,%acc2
    5914:	a42e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2,%acc1
    591a:	a4ae 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2,%acc2
    5920:	ae6e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp,%acc1
    5926:	aeee 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp,%acc2
    592c:	a22e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    5932:	a2ae 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    5938:	a66e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    593e:	a6ee 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    5944:	a42e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    594a:	a4ae 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    5950:	ae6e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    5956:	aeee 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    595c:	a221 1246      	macw %d6u,%d1l,<<,%a1@-,%d1,%acc1
    5960:	a2a1 1256      	macw %d6u,%d1l,<<,%a1@-,%d1,%acc2
    5964:	a661 1246      	macw %d6u,%d1l,<<,%a1@-,%a3,%acc1
    5968:	a6e1 1256      	macw %d6u,%d1l,<<,%a1@-,%a3,%acc2
    596c:	a421 1246      	macw %d6u,%d1l,<<,%a1@-,%d2,%acc1
    5970:	a4a1 1256      	macw %d6u,%d1l,<<,%a1@-,%d2,%acc2
    5974:	ae61 1246      	macw %d6u,%d1l,<<,%a1@-,%sp,%acc1
    5978:	aee1 1256      	macw %d6u,%d1l,<<,%a1@-,%sp,%acc2
    597c:	a221 1266      	macw %d6u,%d1l,<<,%a1@-&,%d1,%acc1
    5980:	a2a1 1276      	macw %d6u,%d1l,<<,%a1@-&,%d1,%acc2
    5984:	a661 1266      	macw %d6u,%d1l,<<,%a1@-&,%a3,%acc1
    5988:	a6e1 1276      	macw %d6u,%d1l,<<,%a1@-&,%a3,%acc2
    598c:	a421 1266      	macw %d6u,%d1l,<<,%a1@-&,%d2,%acc1
    5990:	a4a1 1276      	macw %d6u,%d1l,<<,%a1@-&,%d2,%acc2
    5994:	ae61 1266      	macw %d6u,%d1l,<<,%a1@-&,%sp,%acc1
    5998:	aee1 1276      	macw %d6u,%d1l,<<,%a1@-&,%sp,%acc2
    599c:	a213 1646      	macw %d6u,%d1l,>>,%a3@,%d1,%acc1
    59a0:	a293 1656      	macw %d6u,%d1l,>>,%a3@,%d1,%acc2
    59a4:	a653 1646      	macw %d6u,%d1l,>>,%a3@,%a3,%acc1
    59a8:	a6d3 1656      	macw %d6u,%d1l,>>,%a3@,%a3,%acc2
    59ac:	a413 1646      	macw %d6u,%d1l,>>,%a3@,%d2,%acc1
    59b0:	a493 1656      	macw %d6u,%d1l,>>,%a3@,%d2,%acc2
    59b4:	ae53 1646      	macw %d6u,%d1l,>>,%a3@,%sp,%acc1
    59b8:	aed3 1656      	macw %d6u,%d1l,>>,%a3@,%sp,%acc2
    59bc:	a213 1666      	macw %d6u,%d1l,>>,%a3@&,%d1,%acc1
    59c0:	a293 1676      	macw %d6u,%d1l,>>,%a3@&,%d1,%acc2
    59c4:	a653 1666      	macw %d6u,%d1l,>>,%a3@&,%a3,%acc1
    59c8:	a6d3 1676      	macw %d6u,%d1l,>>,%a3@&,%a3,%acc2
    59cc:	a413 1666      	macw %d6u,%d1l,>>,%a3@&,%d2,%acc1
    59d0:	a493 1676      	macw %d6u,%d1l,>>,%a3@&,%d2,%acc2
    59d4:	ae53 1666      	macw %d6u,%d1l,>>,%a3@&,%sp,%acc1
    59d8:	aed3 1676      	macw %d6u,%d1l,>>,%a3@&,%sp,%acc2
    59dc:	a21a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d1,%acc1
    59e0:	a29a 1656      	macw %d6u,%d1l,>>,%a2@\+,%d1,%acc2
    59e4:	a65a 1646      	macw %d6u,%d1l,>>,%a2@\+,%a3,%acc1
    59e8:	a6da 1656      	macw %d6u,%d1l,>>,%a2@\+,%a3,%acc2
    59ec:	a41a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d2,%acc1
    59f0:	a49a 1656      	macw %d6u,%d1l,>>,%a2@\+,%d2,%acc2
    59f4:	ae5a 1646      	macw %d6u,%d1l,>>,%a2@\+,%sp,%acc1
    59f8:	aeda 1656      	macw %d6u,%d1l,>>,%a2@\+,%sp,%acc2
    59fc:	a21a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d1,%acc1
    5a00:	a29a 1676      	macw %d6u,%d1l,>>,%a2@\+&,%d1,%acc2
    5a04:	a65a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%a3,%acc1
    5a08:	a6da 1676      	macw %d6u,%d1l,>>,%a2@\+&,%a3,%acc2
    5a0c:	a41a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d2,%acc1
    5a10:	a49a 1676      	macw %d6u,%d1l,>>,%a2@\+&,%d2,%acc2
    5a14:	ae5a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%sp,%acc1
    5a18:	aeda 1676      	macw %d6u,%d1l,>>,%a2@\+&,%sp,%acc2
    5a1c:	a22e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1,%acc1
    5a22:	a2ae 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1,%acc2
    5a28:	a66e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3,%acc1
    5a2e:	a6ee 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3,%acc2
    5a34:	a42e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2,%acc1
    5a3a:	a4ae 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2,%acc2
    5a40:	ae6e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp,%acc1
    5a46:	aeee 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp,%acc2
    5a4c:	a22e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    5a52:	a2ae 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    5a58:	a66e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    5a5e:	a6ee 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    5a64:	a42e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    5a6a:	a4ae 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    5a70:	ae6e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    5a76:	aeee 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    5a7c:	a221 1646      	macw %d6u,%d1l,>>,%a1@-,%d1,%acc1
    5a80:	a2a1 1656      	macw %d6u,%d1l,>>,%a1@-,%d1,%acc2
    5a84:	a661 1646      	macw %d6u,%d1l,>>,%a1@-,%a3,%acc1
    5a88:	a6e1 1656      	macw %d6u,%d1l,>>,%a1@-,%a3,%acc2
    5a8c:	a421 1646      	macw %d6u,%d1l,>>,%a1@-,%d2,%acc1
    5a90:	a4a1 1656      	macw %d6u,%d1l,>>,%a1@-,%d2,%acc2
    5a94:	ae61 1646      	macw %d6u,%d1l,>>,%a1@-,%sp,%acc1
    5a98:	aee1 1656      	macw %d6u,%d1l,>>,%a1@-,%sp,%acc2
    5a9c:	a221 1666      	macw %d6u,%d1l,>>,%a1@-&,%d1,%acc1
    5aa0:	a2a1 1676      	macw %d6u,%d1l,>>,%a1@-&,%d1,%acc2
    5aa4:	a661 1666      	macw %d6u,%d1l,>>,%a1@-&,%a3,%acc1
    5aa8:	a6e1 1676      	macw %d6u,%d1l,>>,%a1@-&,%a3,%acc2
    5aac:	a421 1666      	macw %d6u,%d1l,>>,%a1@-&,%d2,%acc1
    5ab0:	a4a1 1676      	macw %d6u,%d1l,>>,%a1@-&,%d2,%acc2
    5ab4:	ae61 1666      	macw %d6u,%d1l,>>,%a1@-&,%sp,%acc1
    5ab8:	aee1 1676      	macw %d6u,%d1l,>>,%a1@-&,%sp,%acc2
    5abc:	a213 1246      	macw %d6u,%d1l,<<,%a3@,%d1,%acc1
    5ac0:	a293 1256      	macw %d6u,%d1l,<<,%a3@,%d1,%acc2
    5ac4:	a653 1246      	macw %d6u,%d1l,<<,%a3@,%a3,%acc1
    5ac8:	a6d3 1256      	macw %d6u,%d1l,<<,%a3@,%a3,%acc2
    5acc:	a413 1246      	macw %d6u,%d1l,<<,%a3@,%d2,%acc1
    5ad0:	a493 1256      	macw %d6u,%d1l,<<,%a3@,%d2,%acc2
    5ad4:	ae53 1246      	macw %d6u,%d1l,<<,%a3@,%sp,%acc1
    5ad8:	aed3 1256      	macw %d6u,%d1l,<<,%a3@,%sp,%acc2
    5adc:	a213 1266      	macw %d6u,%d1l,<<,%a3@&,%d1,%acc1
    5ae0:	a293 1276      	macw %d6u,%d1l,<<,%a3@&,%d1,%acc2
    5ae4:	a653 1266      	macw %d6u,%d1l,<<,%a3@&,%a3,%acc1
    5ae8:	a6d3 1276      	macw %d6u,%d1l,<<,%a3@&,%a3,%acc2
    5aec:	a413 1266      	macw %d6u,%d1l,<<,%a3@&,%d2,%acc1
    5af0:	a493 1276      	macw %d6u,%d1l,<<,%a3@&,%d2,%acc2
    5af4:	ae53 1266      	macw %d6u,%d1l,<<,%a3@&,%sp,%acc1
    5af8:	aed3 1276      	macw %d6u,%d1l,<<,%a3@&,%sp,%acc2
    5afc:	a21a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d1,%acc1
    5b00:	a29a 1256      	macw %d6u,%d1l,<<,%a2@\+,%d1,%acc2
    5b04:	a65a 1246      	macw %d6u,%d1l,<<,%a2@\+,%a3,%acc1
    5b08:	a6da 1256      	macw %d6u,%d1l,<<,%a2@\+,%a3,%acc2
    5b0c:	a41a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d2,%acc1
    5b10:	a49a 1256      	macw %d6u,%d1l,<<,%a2@\+,%d2,%acc2
    5b14:	ae5a 1246      	macw %d6u,%d1l,<<,%a2@\+,%sp,%acc1
    5b18:	aeda 1256      	macw %d6u,%d1l,<<,%a2@\+,%sp,%acc2
    5b1c:	a21a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d1,%acc1
    5b20:	a29a 1276      	macw %d6u,%d1l,<<,%a2@\+&,%d1,%acc2
    5b24:	a65a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%a3,%acc1
    5b28:	a6da 1276      	macw %d6u,%d1l,<<,%a2@\+&,%a3,%acc2
    5b2c:	a41a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d2,%acc1
    5b30:	a49a 1276      	macw %d6u,%d1l,<<,%a2@\+&,%d2,%acc2
    5b34:	ae5a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%sp,%acc1
    5b38:	aeda 1276      	macw %d6u,%d1l,<<,%a2@\+&,%sp,%acc2
    5b3c:	a22e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1,%acc1
    5b42:	a2ae 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1,%acc2
    5b48:	a66e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3,%acc1
    5b4e:	a6ee 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3,%acc2
    5b54:	a42e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2,%acc1
    5b5a:	a4ae 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2,%acc2
    5b60:	ae6e 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp,%acc1
    5b66:	aeee 1256 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp,%acc2
    5b6c:	a22e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1,%acc1
    5b72:	a2ae 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1,%acc2
    5b78:	a66e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3,%acc1
    5b7e:	a6ee 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3,%acc2
    5b84:	a42e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2,%acc1
    5b8a:	a4ae 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2,%acc2
    5b90:	ae6e 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp,%acc1
    5b96:	aeee 1276 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp,%acc2
    5b9c:	a221 1246      	macw %d6u,%d1l,<<,%a1@-,%d1,%acc1
    5ba0:	a2a1 1256      	macw %d6u,%d1l,<<,%a1@-,%d1,%acc2
    5ba4:	a661 1246      	macw %d6u,%d1l,<<,%a1@-,%a3,%acc1
    5ba8:	a6e1 1256      	macw %d6u,%d1l,<<,%a1@-,%a3,%acc2
    5bac:	a421 1246      	macw %d6u,%d1l,<<,%a1@-,%d2,%acc1
    5bb0:	a4a1 1256      	macw %d6u,%d1l,<<,%a1@-,%d2,%acc2
    5bb4:	ae61 1246      	macw %d6u,%d1l,<<,%a1@-,%sp,%acc1
    5bb8:	aee1 1256      	macw %d6u,%d1l,<<,%a1@-,%sp,%acc2
    5bbc:	a221 1266      	macw %d6u,%d1l,<<,%a1@-&,%d1,%acc1
    5bc0:	a2a1 1276      	macw %d6u,%d1l,<<,%a1@-&,%d1,%acc2
    5bc4:	a661 1266      	macw %d6u,%d1l,<<,%a1@-&,%a3,%acc1
    5bc8:	a6e1 1276      	macw %d6u,%d1l,<<,%a1@-&,%a3,%acc2
    5bcc:	a421 1266      	macw %d6u,%d1l,<<,%a1@-&,%d2,%acc1
    5bd0:	a4a1 1276      	macw %d6u,%d1l,<<,%a1@-&,%d2,%acc2
    5bd4:	ae61 1266      	macw %d6u,%d1l,<<,%a1@-&,%sp,%acc1
    5bd8:	aee1 1276      	macw %d6u,%d1l,<<,%a1@-&,%sp,%acc2
    5bdc:	a213 1646      	macw %d6u,%d1l,>>,%a3@,%d1,%acc1
    5be0:	a293 1656      	macw %d6u,%d1l,>>,%a3@,%d1,%acc2
    5be4:	a653 1646      	macw %d6u,%d1l,>>,%a3@,%a3,%acc1
    5be8:	a6d3 1656      	macw %d6u,%d1l,>>,%a3@,%a3,%acc2
    5bec:	a413 1646      	macw %d6u,%d1l,>>,%a3@,%d2,%acc1
    5bf0:	a493 1656      	macw %d6u,%d1l,>>,%a3@,%d2,%acc2
    5bf4:	ae53 1646      	macw %d6u,%d1l,>>,%a3@,%sp,%acc1
    5bf8:	aed3 1656      	macw %d6u,%d1l,>>,%a3@,%sp,%acc2
    5bfc:	a213 1666      	macw %d6u,%d1l,>>,%a3@&,%d1,%acc1
    5c00:	a293 1676      	macw %d6u,%d1l,>>,%a3@&,%d1,%acc2
    5c04:	a653 1666      	macw %d6u,%d1l,>>,%a3@&,%a3,%acc1
    5c08:	a6d3 1676      	macw %d6u,%d1l,>>,%a3@&,%a3,%acc2
    5c0c:	a413 1666      	macw %d6u,%d1l,>>,%a3@&,%d2,%acc1
    5c10:	a493 1676      	macw %d6u,%d1l,>>,%a3@&,%d2,%acc2
    5c14:	ae53 1666      	macw %d6u,%d1l,>>,%a3@&,%sp,%acc1
    5c18:	aed3 1676      	macw %d6u,%d1l,>>,%a3@&,%sp,%acc2
    5c1c:	a21a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d1,%acc1
    5c20:	a29a 1656      	macw %d6u,%d1l,>>,%a2@\+,%d1,%acc2
    5c24:	a65a 1646      	macw %d6u,%d1l,>>,%a2@\+,%a3,%acc1
    5c28:	a6da 1656      	macw %d6u,%d1l,>>,%a2@\+,%a3,%acc2
    5c2c:	a41a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d2,%acc1
    5c30:	a49a 1656      	macw %d6u,%d1l,>>,%a2@\+,%d2,%acc2
    5c34:	ae5a 1646      	macw %d6u,%d1l,>>,%a2@\+,%sp,%acc1
    5c38:	aeda 1656      	macw %d6u,%d1l,>>,%a2@\+,%sp,%acc2
    5c3c:	a21a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d1,%acc1
    5c40:	a29a 1676      	macw %d6u,%d1l,>>,%a2@\+&,%d1,%acc2
    5c44:	a65a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%a3,%acc1
    5c48:	a6da 1676      	macw %d6u,%d1l,>>,%a2@\+&,%a3,%acc2
    5c4c:	a41a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d2,%acc1
    5c50:	a49a 1676      	macw %d6u,%d1l,>>,%a2@\+&,%d2,%acc2
    5c54:	ae5a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%sp,%acc1
    5c58:	aeda 1676      	macw %d6u,%d1l,>>,%a2@\+&,%sp,%acc2
    5c5c:	a22e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1,%acc1
    5c62:	a2ae 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1,%acc2
    5c68:	a66e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3,%acc1
    5c6e:	a6ee 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3,%acc2
    5c74:	a42e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2,%acc1
    5c7a:	a4ae 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2,%acc2
    5c80:	ae6e 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp,%acc1
    5c86:	aeee 1656 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp,%acc2
    5c8c:	a22e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1,%acc1
    5c92:	a2ae 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1,%acc2
    5c98:	a66e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3,%acc1
    5c9e:	a6ee 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3,%acc2
    5ca4:	a42e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2,%acc1
    5caa:	a4ae 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2,%acc2
    5cb0:	ae6e 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp,%acc1
    5cb6:	aeee 1676 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp,%acc2
    5cbc:	a221 1646      	macw %d6u,%d1l,>>,%a1@-,%d1,%acc1
    5cc0:	a2a1 1656      	macw %d6u,%d1l,>>,%a1@-,%d1,%acc2
    5cc4:	a661 1646      	macw %d6u,%d1l,>>,%a1@-,%a3,%acc1
    5cc8:	a6e1 1656      	macw %d6u,%d1l,>>,%a1@-,%a3,%acc2
    5ccc:	a421 1646      	macw %d6u,%d1l,>>,%a1@-,%d2,%acc1
    5cd0:	a4a1 1656      	macw %d6u,%d1l,>>,%a1@-,%d2,%acc2
    5cd4:	ae61 1646      	macw %d6u,%d1l,>>,%a1@-,%sp,%acc1
    5cd8:	aee1 1656      	macw %d6u,%d1l,>>,%a1@-,%sp,%acc2
    5cdc:	a221 1666      	macw %d6u,%d1l,>>,%a1@-&,%d1,%acc1
    5ce0:	a2a1 1676      	macw %d6u,%d1l,>>,%a1@-&,%d1,%acc2
    5ce4:	a661 1666      	macw %d6u,%d1l,>>,%a1@-&,%a3,%acc1
    5ce8:	a6e1 1676      	macw %d6u,%d1l,>>,%a1@-&,%a3,%acc2
    5cec:	a421 1666      	macw %d6u,%d1l,>>,%a1@-&,%d2,%acc1
    5cf0:	a4a1 1676      	macw %d6u,%d1l,>>,%a1@-&,%d2,%acc2
    5cf4:	ae61 1666      	macw %d6u,%d1l,>>,%a1@-&,%sp,%acc1
    5cf8:	aee1 1676      	macw %d6u,%d1l,>>,%a1@-&,%sp,%acc2
    5cfc:	a6c9 0800      	macl %a1,%a3,%acc1
    5d00:	a649 0810      	macl %a1,%a3,%acc2
    5d04:	a6c9 0a00      	macl %a1,%a3,<<,%acc1
    5d08:	a649 0a10      	macl %a1,%a3,<<,%acc2
    5d0c:	a6c9 0e00      	macl %a1,%a3,>>,%acc1
    5d10:	a649 0e10      	macl %a1,%a3,>>,%acc2
    5d14:	a6c9 0a00      	macl %a1,%a3,<<,%acc1
    5d18:	a649 0a10      	macl %a1,%a3,<<,%acc2
    5d1c:	a6c9 0e00      	macl %a1,%a3,>>,%acc1
    5d20:	a649 0e10      	macl %a1,%a3,>>,%acc2
    5d24:	a889 0800      	macl %a1,%d4,%acc1
    5d28:	a809 0810      	macl %a1,%d4,%acc2
    5d2c:	a889 0a00      	macl %a1,%d4,<<,%acc1
    5d30:	a809 0a10      	macl %a1,%d4,<<,%acc2
    5d34:	a889 0e00      	macl %a1,%d4,>>,%acc1
    5d38:	a809 0e10      	macl %a1,%d4,>>,%acc2
    5d3c:	a889 0a00      	macl %a1,%d4,<<,%acc1
    5d40:	a809 0a10      	macl %a1,%d4,<<,%acc2
    5d44:	a889 0e00      	macl %a1,%d4,>>,%acc1
    5d48:	a809 0e10      	macl %a1,%d4,>>,%acc2
    5d4c:	a6c6 0800      	macl %d6,%a3,%acc1
    5d50:	a646 0810      	macl %d6,%a3,%acc2
    5d54:	a6c6 0a00      	macl %d6,%a3,<<,%acc1
    5d58:	a646 0a10      	macl %d6,%a3,<<,%acc2
    5d5c:	a6c6 0e00      	macl %d6,%a3,>>,%acc1
    5d60:	a646 0e10      	macl %d6,%a3,>>,%acc2
    5d64:	a6c6 0a00      	macl %d6,%a3,<<,%acc1
    5d68:	a646 0a10      	macl %d6,%a3,<<,%acc2
    5d6c:	a6c6 0e00      	macl %d6,%a3,>>,%acc1
    5d70:	a646 0e10      	macl %d6,%a3,>>,%acc2
    5d74:	a886 0800      	macl %d6,%d4,%acc1
    5d78:	a806 0810      	macl %d6,%d4,%acc2
    5d7c:	a886 0a00      	macl %d6,%d4,<<,%acc1
    5d80:	a806 0a10      	macl %d6,%d4,<<,%acc2
    5d84:	a886 0e00      	macl %d6,%d4,>>,%acc1
    5d88:	a806 0e10      	macl %d6,%d4,>>,%acc2
    5d8c:	a886 0a00      	macl %d6,%d4,<<,%acc1
    5d90:	a806 0a10      	macl %d6,%d4,<<,%acc2
    5d94:	a886 0e00      	macl %d6,%d4,>>,%acc1
    5d98:	a806 0e10      	macl %d6,%d4,>>,%acc2
    5d9c:	a213 b809      	macl %a1,%a3,%a3@,%d1,%acc1
    5da0:	a293 b819      	macl %a1,%a3,%a3@,%d1,%acc2
    5da4:	a653 b809      	macl %a1,%a3,%a3@,%a3,%acc1
    5da8:	a6d3 b819      	macl %a1,%a3,%a3@,%a3,%acc2
    5dac:	a413 b809      	macl %a1,%a3,%a3@,%d2,%acc1
    5db0:	a493 b819      	macl %a1,%a3,%a3@,%d2,%acc2
    5db4:	ae53 b809      	macl %a1,%a3,%a3@,%sp,%acc1
    5db8:	aed3 b819      	macl %a1,%a3,%a3@,%sp,%acc2
    5dbc:	a213 b829      	macl %a1,%a3,%a3@&,%d1,%acc1
    5dc0:	a293 b839      	macl %a1,%a3,%a3@&,%d1,%acc2
    5dc4:	a653 b829      	macl %a1,%a3,%a3@&,%a3,%acc1
    5dc8:	a6d3 b839      	macl %a1,%a3,%a3@&,%a3,%acc2
    5dcc:	a413 b829      	macl %a1,%a3,%a3@&,%d2,%acc1
    5dd0:	a493 b839      	macl %a1,%a3,%a3@&,%d2,%acc2
    5dd4:	ae53 b829      	macl %a1,%a3,%a3@&,%sp,%acc1
    5dd8:	aed3 b839      	macl %a1,%a3,%a3@&,%sp,%acc2
    5ddc:	a21a b809      	macl %a1,%a3,%a2@\+,%d1,%acc1
    5de0:	a29a b819      	macl %a1,%a3,%a2@\+,%d1,%acc2
    5de4:	a65a b809      	macl %a1,%a3,%a2@\+,%a3,%acc1
    5de8:	a6da b819      	macl %a1,%a3,%a2@\+,%a3,%acc2
    5dec:	a41a b809      	macl %a1,%a3,%a2@\+,%d2,%acc1
    5df0:	a49a b819      	macl %a1,%a3,%a2@\+,%d2,%acc2
    5df4:	ae5a b809      	macl %a1,%a3,%a2@\+,%sp,%acc1
    5df8:	aeda b819      	macl %a1,%a3,%a2@\+,%sp,%acc2
    5dfc:	a21a b829      	macl %a1,%a3,%a2@\+&,%d1,%acc1
    5e00:	a29a b839      	macl %a1,%a3,%a2@\+&,%d1,%acc2
    5e04:	a65a b829      	macl %a1,%a3,%a2@\+&,%a3,%acc1
    5e08:	a6da b839      	macl %a1,%a3,%a2@\+&,%a3,%acc2
    5e0c:	a41a b829      	macl %a1,%a3,%a2@\+&,%d2,%acc1
    5e10:	a49a b839      	macl %a1,%a3,%a2@\+&,%d2,%acc2
    5e14:	ae5a b829      	macl %a1,%a3,%a2@\+&,%sp,%acc1
    5e18:	aeda b839      	macl %a1,%a3,%a2@\+&,%sp,%acc2
    5e1c:	a22e b809 000a 	macl %a1,%a3,%fp@\(10\),%d1,%acc1
    5e22:	a2ae b819 000a 	macl %a1,%a3,%fp@\(10\),%d1,%acc2
    5e28:	a66e b809 000a 	macl %a1,%a3,%fp@\(10\),%a3,%acc1
    5e2e:	a6ee b819 000a 	macl %a1,%a3,%fp@\(10\),%a3,%acc2
    5e34:	a42e b809 000a 	macl %a1,%a3,%fp@\(10\),%d2,%acc1
    5e3a:	a4ae b819 000a 	macl %a1,%a3,%fp@\(10\),%d2,%acc2
    5e40:	ae6e b809 000a 	macl %a1,%a3,%fp@\(10\),%sp,%acc1
    5e46:	aeee b819 000a 	macl %a1,%a3,%fp@\(10\),%sp,%acc2
    5e4c:	a22e b829 000a 	macl %a1,%a3,%fp@\(10\)&,%d1,%acc1
    5e52:	a2ae b839 000a 	macl %a1,%a3,%fp@\(10\)&,%d1,%acc2
    5e58:	a66e b829 000a 	macl %a1,%a3,%fp@\(10\)&,%a3,%acc1
    5e5e:	a6ee b839 000a 	macl %a1,%a3,%fp@\(10\)&,%a3,%acc2
    5e64:	a42e b829 000a 	macl %a1,%a3,%fp@\(10\)&,%d2,%acc1
    5e6a:	a4ae b839 000a 	macl %a1,%a3,%fp@\(10\)&,%d2,%acc2
    5e70:	ae6e b829 000a 	macl %a1,%a3,%fp@\(10\)&,%sp,%acc1
    5e76:	aeee b839 000a 	macl %a1,%a3,%fp@\(10\)&,%sp,%acc2
    5e7c:	a221 b809      	macl %a1,%a3,%a1@-,%d1,%acc1
    5e80:	a2a1 b819      	macl %a1,%a3,%a1@-,%d1,%acc2
    5e84:	a661 b809      	macl %a1,%a3,%a1@-,%a3,%acc1
    5e88:	a6e1 b819      	macl %a1,%a3,%a1@-,%a3,%acc2
    5e8c:	a421 b809      	macl %a1,%a3,%a1@-,%d2,%acc1
    5e90:	a4a1 b819      	macl %a1,%a3,%a1@-,%d2,%acc2
    5e94:	ae61 b809      	macl %a1,%a3,%a1@-,%sp,%acc1
    5e98:	aee1 b819      	macl %a1,%a3,%a1@-,%sp,%acc2
    5e9c:	a221 b829      	macl %a1,%a3,%a1@-&,%d1,%acc1
    5ea0:	a2a1 b839      	macl %a1,%a3,%a1@-&,%d1,%acc2
    5ea4:	a661 b829      	macl %a1,%a3,%a1@-&,%a3,%acc1
    5ea8:	a6e1 b839      	macl %a1,%a3,%a1@-&,%a3,%acc2
    5eac:	a421 b829      	macl %a1,%a3,%a1@-&,%d2,%acc1
    5eb0:	a4a1 b839      	macl %a1,%a3,%a1@-&,%d2,%acc2
    5eb4:	ae61 b829      	macl %a1,%a3,%a1@-&,%sp,%acc1
    5eb8:	aee1 b839      	macl %a1,%a3,%a1@-&,%sp,%acc2
    5ebc:	a213 ba09      	macl %a1,%a3,<<,%a3@,%d1,%acc1
    5ec0:	a293 ba19      	macl %a1,%a3,<<,%a3@,%d1,%acc2
    5ec4:	a653 ba09      	macl %a1,%a3,<<,%a3@,%a3,%acc1
    5ec8:	a6d3 ba19      	macl %a1,%a3,<<,%a3@,%a3,%acc2
    5ecc:	a413 ba09      	macl %a1,%a3,<<,%a3@,%d2,%acc1
    5ed0:	a493 ba19      	macl %a1,%a3,<<,%a3@,%d2,%acc2
    5ed4:	ae53 ba09      	macl %a1,%a3,<<,%a3@,%sp,%acc1
    5ed8:	aed3 ba19      	macl %a1,%a3,<<,%a3@,%sp,%acc2
    5edc:	a213 ba29      	macl %a1,%a3,<<,%a3@&,%d1,%acc1
    5ee0:	a293 ba39      	macl %a1,%a3,<<,%a3@&,%d1,%acc2
    5ee4:	a653 ba29      	macl %a1,%a3,<<,%a3@&,%a3,%acc1
    5ee8:	a6d3 ba39      	macl %a1,%a3,<<,%a3@&,%a3,%acc2
    5eec:	a413 ba29      	macl %a1,%a3,<<,%a3@&,%d2,%acc1
    5ef0:	a493 ba39      	macl %a1,%a3,<<,%a3@&,%d2,%acc2
    5ef4:	ae53 ba29      	macl %a1,%a3,<<,%a3@&,%sp,%acc1
    5ef8:	aed3 ba39      	macl %a1,%a3,<<,%a3@&,%sp,%acc2
    5efc:	a21a ba09      	macl %a1,%a3,<<,%a2@\+,%d1,%acc1
    5f00:	a29a ba19      	macl %a1,%a3,<<,%a2@\+,%d1,%acc2
    5f04:	a65a ba09      	macl %a1,%a3,<<,%a2@\+,%a3,%acc1
    5f08:	a6da ba19      	macl %a1,%a3,<<,%a2@\+,%a3,%acc2
    5f0c:	a41a ba09      	macl %a1,%a3,<<,%a2@\+,%d2,%acc1
    5f10:	a49a ba19      	macl %a1,%a3,<<,%a2@\+,%d2,%acc2
    5f14:	ae5a ba09      	macl %a1,%a3,<<,%a2@\+,%sp,%acc1
    5f18:	aeda ba19      	macl %a1,%a3,<<,%a2@\+,%sp,%acc2
    5f1c:	a21a ba29      	macl %a1,%a3,<<,%a2@\+&,%d1,%acc1
    5f20:	a29a ba39      	macl %a1,%a3,<<,%a2@\+&,%d1,%acc2
    5f24:	a65a ba29      	macl %a1,%a3,<<,%a2@\+&,%a3,%acc1
    5f28:	a6da ba39      	macl %a1,%a3,<<,%a2@\+&,%a3,%acc2
    5f2c:	a41a ba29      	macl %a1,%a3,<<,%a2@\+&,%d2,%acc1
    5f30:	a49a ba39      	macl %a1,%a3,<<,%a2@\+&,%d2,%acc2
    5f34:	ae5a ba29      	macl %a1,%a3,<<,%a2@\+&,%sp,%acc1
    5f38:	aeda ba39      	macl %a1,%a3,<<,%a2@\+&,%sp,%acc2
    5f3c:	a22e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1,%acc1
    5f42:	a2ae ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1,%acc2
    5f48:	a66e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3,%acc1
    5f4e:	a6ee ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3,%acc2
    5f54:	a42e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2,%acc1
    5f5a:	a4ae ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2,%acc2
    5f60:	ae6e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp,%acc1
    5f66:	aeee ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp,%acc2
    5f6c:	a22e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1,%acc1
    5f72:	a2ae ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1,%acc2
    5f78:	a66e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3,%acc1
    5f7e:	a6ee ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3,%acc2
    5f84:	a42e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2,%acc1
    5f8a:	a4ae ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2,%acc2
    5f90:	ae6e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp,%acc1
    5f96:	aeee ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp,%acc2
    5f9c:	a221 ba09      	macl %a1,%a3,<<,%a1@-,%d1,%acc1
    5fa0:	a2a1 ba19      	macl %a1,%a3,<<,%a1@-,%d1,%acc2
    5fa4:	a661 ba09      	macl %a1,%a3,<<,%a1@-,%a3,%acc1
    5fa8:	a6e1 ba19      	macl %a1,%a3,<<,%a1@-,%a3,%acc2
    5fac:	a421 ba09      	macl %a1,%a3,<<,%a1@-,%d2,%acc1
    5fb0:	a4a1 ba19      	macl %a1,%a3,<<,%a1@-,%d2,%acc2
    5fb4:	ae61 ba09      	macl %a1,%a3,<<,%a1@-,%sp,%acc1
    5fb8:	aee1 ba19      	macl %a1,%a3,<<,%a1@-,%sp,%acc2
    5fbc:	a221 ba29      	macl %a1,%a3,<<,%a1@-&,%d1,%acc1
    5fc0:	a2a1 ba39      	macl %a1,%a3,<<,%a1@-&,%d1,%acc2
    5fc4:	a661 ba29      	macl %a1,%a3,<<,%a1@-&,%a3,%acc1
    5fc8:	a6e1 ba39      	macl %a1,%a3,<<,%a1@-&,%a3,%acc2
    5fcc:	a421 ba29      	macl %a1,%a3,<<,%a1@-&,%d2,%acc1
    5fd0:	a4a1 ba39      	macl %a1,%a3,<<,%a1@-&,%d2,%acc2
    5fd4:	ae61 ba29      	macl %a1,%a3,<<,%a1@-&,%sp,%acc1
    5fd8:	aee1 ba39      	macl %a1,%a3,<<,%a1@-&,%sp,%acc2
    5fdc:	a213 be09      	macl %a1,%a3,>>,%a3@,%d1,%acc1
    5fe0:	a293 be19      	macl %a1,%a3,>>,%a3@,%d1,%acc2
    5fe4:	a653 be09      	macl %a1,%a3,>>,%a3@,%a3,%acc1
    5fe8:	a6d3 be19      	macl %a1,%a3,>>,%a3@,%a3,%acc2
    5fec:	a413 be09      	macl %a1,%a3,>>,%a3@,%d2,%acc1
    5ff0:	a493 be19      	macl %a1,%a3,>>,%a3@,%d2,%acc2
    5ff4:	ae53 be09      	macl %a1,%a3,>>,%a3@,%sp,%acc1
    5ff8:	aed3 be19      	macl %a1,%a3,>>,%a3@,%sp,%acc2
    5ffc:	a213 be29      	macl %a1,%a3,>>,%a3@&,%d1,%acc1
    6000:	a293 be39      	macl %a1,%a3,>>,%a3@&,%d1,%acc2
    6004:	a653 be29      	macl %a1,%a3,>>,%a3@&,%a3,%acc1
    6008:	a6d3 be39      	macl %a1,%a3,>>,%a3@&,%a3,%acc2
    600c:	a413 be29      	macl %a1,%a3,>>,%a3@&,%d2,%acc1
    6010:	a493 be39      	macl %a1,%a3,>>,%a3@&,%d2,%acc2
    6014:	ae53 be29      	macl %a1,%a3,>>,%a3@&,%sp,%acc1
    6018:	aed3 be39      	macl %a1,%a3,>>,%a3@&,%sp,%acc2
    601c:	a21a be09      	macl %a1,%a3,>>,%a2@\+,%d1,%acc1
    6020:	a29a be19      	macl %a1,%a3,>>,%a2@\+,%d1,%acc2
    6024:	a65a be09      	macl %a1,%a3,>>,%a2@\+,%a3,%acc1
    6028:	a6da be19      	macl %a1,%a3,>>,%a2@\+,%a3,%acc2
    602c:	a41a be09      	macl %a1,%a3,>>,%a2@\+,%d2,%acc1
    6030:	a49a be19      	macl %a1,%a3,>>,%a2@\+,%d2,%acc2
    6034:	ae5a be09      	macl %a1,%a3,>>,%a2@\+,%sp,%acc1
    6038:	aeda be19      	macl %a1,%a3,>>,%a2@\+,%sp,%acc2
    603c:	a21a be29      	macl %a1,%a3,>>,%a2@\+&,%d1,%acc1
    6040:	a29a be39      	macl %a1,%a3,>>,%a2@\+&,%d1,%acc2
    6044:	a65a be29      	macl %a1,%a3,>>,%a2@\+&,%a3,%acc1
    6048:	a6da be39      	macl %a1,%a3,>>,%a2@\+&,%a3,%acc2
    604c:	a41a be29      	macl %a1,%a3,>>,%a2@\+&,%d2,%acc1
    6050:	a49a be39      	macl %a1,%a3,>>,%a2@\+&,%d2,%acc2
    6054:	ae5a be29      	macl %a1,%a3,>>,%a2@\+&,%sp,%acc1
    6058:	aeda be39      	macl %a1,%a3,>>,%a2@\+&,%sp,%acc2
    605c:	a22e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1,%acc1
    6062:	a2ae be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1,%acc2
    6068:	a66e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3,%acc1
    606e:	a6ee be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3,%acc2
    6074:	a42e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2,%acc1
    607a:	a4ae be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2,%acc2
    6080:	ae6e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp,%acc1
    6086:	aeee be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp,%acc2
    608c:	a22e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1,%acc1
    6092:	a2ae be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1,%acc2
    6098:	a66e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3,%acc1
    609e:	a6ee be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3,%acc2
    60a4:	a42e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2,%acc1
    60aa:	a4ae be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2,%acc2
    60b0:	ae6e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp,%acc1
    60b6:	aeee be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp,%acc2
    60bc:	a221 be09      	macl %a1,%a3,>>,%a1@-,%d1,%acc1
    60c0:	a2a1 be19      	macl %a1,%a3,>>,%a1@-,%d1,%acc2
    60c4:	a661 be09      	macl %a1,%a3,>>,%a1@-,%a3,%acc1
    60c8:	a6e1 be19      	macl %a1,%a3,>>,%a1@-,%a3,%acc2
    60cc:	a421 be09      	macl %a1,%a3,>>,%a1@-,%d2,%acc1
    60d0:	a4a1 be19      	macl %a1,%a3,>>,%a1@-,%d2,%acc2
    60d4:	ae61 be09      	macl %a1,%a3,>>,%a1@-,%sp,%acc1
    60d8:	aee1 be19      	macl %a1,%a3,>>,%a1@-,%sp,%acc2
    60dc:	a221 be29      	macl %a1,%a3,>>,%a1@-&,%d1,%acc1
    60e0:	a2a1 be39      	macl %a1,%a3,>>,%a1@-&,%d1,%acc2
    60e4:	a661 be29      	macl %a1,%a3,>>,%a1@-&,%a3,%acc1
    60e8:	a6e1 be39      	macl %a1,%a3,>>,%a1@-&,%a3,%acc2
    60ec:	a421 be29      	macl %a1,%a3,>>,%a1@-&,%d2,%acc1
    60f0:	a4a1 be39      	macl %a1,%a3,>>,%a1@-&,%d2,%acc2
    60f4:	ae61 be29      	macl %a1,%a3,>>,%a1@-&,%sp,%acc1
    60f8:	aee1 be39      	macl %a1,%a3,>>,%a1@-&,%sp,%acc2
    60fc:	a213 ba09      	macl %a1,%a3,<<,%a3@,%d1,%acc1
    6100:	a293 ba19      	macl %a1,%a3,<<,%a3@,%d1,%acc2
    6104:	a653 ba09      	macl %a1,%a3,<<,%a3@,%a3,%acc1
    6108:	a6d3 ba19      	macl %a1,%a3,<<,%a3@,%a3,%acc2
    610c:	a413 ba09      	macl %a1,%a3,<<,%a3@,%d2,%acc1
    6110:	a493 ba19      	macl %a1,%a3,<<,%a3@,%d2,%acc2
    6114:	ae53 ba09      	macl %a1,%a3,<<,%a3@,%sp,%acc1
    6118:	aed3 ba19      	macl %a1,%a3,<<,%a3@,%sp,%acc2
    611c:	a213 ba29      	macl %a1,%a3,<<,%a3@&,%d1,%acc1
    6120:	a293 ba39      	macl %a1,%a3,<<,%a3@&,%d1,%acc2
    6124:	a653 ba29      	macl %a1,%a3,<<,%a3@&,%a3,%acc1
    6128:	a6d3 ba39      	macl %a1,%a3,<<,%a3@&,%a3,%acc2
    612c:	a413 ba29      	macl %a1,%a3,<<,%a3@&,%d2,%acc1
    6130:	a493 ba39      	macl %a1,%a3,<<,%a3@&,%d2,%acc2
    6134:	ae53 ba29      	macl %a1,%a3,<<,%a3@&,%sp,%acc1
    6138:	aed3 ba39      	macl %a1,%a3,<<,%a3@&,%sp,%acc2
    613c:	a21a ba09      	macl %a1,%a3,<<,%a2@\+,%d1,%acc1
    6140:	a29a ba19      	macl %a1,%a3,<<,%a2@\+,%d1,%acc2
    6144:	a65a ba09      	macl %a1,%a3,<<,%a2@\+,%a3,%acc1
    6148:	a6da ba19      	macl %a1,%a3,<<,%a2@\+,%a3,%acc2
    614c:	a41a ba09      	macl %a1,%a3,<<,%a2@\+,%d2,%acc1
    6150:	a49a ba19      	macl %a1,%a3,<<,%a2@\+,%d2,%acc2
    6154:	ae5a ba09      	macl %a1,%a3,<<,%a2@\+,%sp,%acc1
    6158:	aeda ba19      	macl %a1,%a3,<<,%a2@\+,%sp,%acc2
    615c:	a21a ba29      	macl %a1,%a3,<<,%a2@\+&,%d1,%acc1
    6160:	a29a ba39      	macl %a1,%a3,<<,%a2@\+&,%d1,%acc2
    6164:	a65a ba29      	macl %a1,%a3,<<,%a2@\+&,%a3,%acc1
    6168:	a6da ba39      	macl %a1,%a3,<<,%a2@\+&,%a3,%acc2
    616c:	a41a ba29      	macl %a1,%a3,<<,%a2@\+&,%d2,%acc1
    6170:	a49a ba39      	macl %a1,%a3,<<,%a2@\+&,%d2,%acc2
    6174:	ae5a ba29      	macl %a1,%a3,<<,%a2@\+&,%sp,%acc1
    6178:	aeda ba39      	macl %a1,%a3,<<,%a2@\+&,%sp,%acc2
    617c:	a22e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1,%acc1
    6182:	a2ae ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1,%acc2
    6188:	a66e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3,%acc1
    618e:	a6ee ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3,%acc2
    6194:	a42e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2,%acc1
    619a:	a4ae ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2,%acc2
    61a0:	ae6e ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp,%acc1
    61a6:	aeee ba19 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp,%acc2
    61ac:	a22e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1,%acc1
    61b2:	a2ae ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1,%acc2
    61b8:	a66e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3,%acc1
    61be:	a6ee ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3,%acc2
    61c4:	a42e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2,%acc1
    61ca:	a4ae ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2,%acc2
    61d0:	ae6e ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp,%acc1
    61d6:	aeee ba39 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp,%acc2
    61dc:	a221 ba09      	macl %a1,%a3,<<,%a1@-,%d1,%acc1
    61e0:	a2a1 ba19      	macl %a1,%a3,<<,%a1@-,%d1,%acc2
    61e4:	a661 ba09      	macl %a1,%a3,<<,%a1@-,%a3,%acc1
    61e8:	a6e1 ba19      	macl %a1,%a3,<<,%a1@-,%a3,%acc2
    61ec:	a421 ba09      	macl %a1,%a3,<<,%a1@-,%d2,%acc1
    61f0:	a4a1 ba19      	macl %a1,%a3,<<,%a1@-,%d2,%acc2
    61f4:	ae61 ba09      	macl %a1,%a3,<<,%a1@-,%sp,%acc1
    61f8:	aee1 ba19      	macl %a1,%a3,<<,%a1@-,%sp,%acc2
    61fc:	a221 ba29      	macl %a1,%a3,<<,%a1@-&,%d1,%acc1
    6200:	a2a1 ba39      	macl %a1,%a3,<<,%a1@-&,%d1,%acc2
    6204:	a661 ba29      	macl %a1,%a3,<<,%a1@-&,%a3,%acc1
    6208:	a6e1 ba39      	macl %a1,%a3,<<,%a1@-&,%a3,%acc2
    620c:	a421 ba29      	macl %a1,%a3,<<,%a1@-&,%d2,%acc1
    6210:	a4a1 ba39      	macl %a1,%a3,<<,%a1@-&,%d2,%acc2
    6214:	ae61 ba29      	macl %a1,%a3,<<,%a1@-&,%sp,%acc1
    6218:	aee1 ba39      	macl %a1,%a3,<<,%a1@-&,%sp,%acc2
    621c:	a213 be09      	macl %a1,%a3,>>,%a3@,%d1,%acc1
    6220:	a293 be19      	macl %a1,%a3,>>,%a3@,%d1,%acc2
    6224:	a653 be09      	macl %a1,%a3,>>,%a3@,%a3,%acc1
    6228:	a6d3 be19      	macl %a1,%a3,>>,%a3@,%a3,%acc2
    622c:	a413 be09      	macl %a1,%a3,>>,%a3@,%d2,%acc1
    6230:	a493 be19      	macl %a1,%a3,>>,%a3@,%d2,%acc2
    6234:	ae53 be09      	macl %a1,%a3,>>,%a3@,%sp,%acc1
    6238:	aed3 be19      	macl %a1,%a3,>>,%a3@,%sp,%acc2
    623c:	a213 be29      	macl %a1,%a3,>>,%a3@&,%d1,%acc1
    6240:	a293 be39      	macl %a1,%a3,>>,%a3@&,%d1,%acc2
    6244:	a653 be29      	macl %a1,%a3,>>,%a3@&,%a3,%acc1
    6248:	a6d3 be39      	macl %a1,%a3,>>,%a3@&,%a3,%acc2
    624c:	a413 be29      	macl %a1,%a3,>>,%a3@&,%d2,%acc1
    6250:	a493 be39      	macl %a1,%a3,>>,%a3@&,%d2,%acc2
    6254:	ae53 be29      	macl %a1,%a3,>>,%a3@&,%sp,%acc1
    6258:	aed3 be39      	macl %a1,%a3,>>,%a3@&,%sp,%acc2
    625c:	a21a be09      	macl %a1,%a3,>>,%a2@\+,%d1,%acc1
    6260:	a29a be19      	macl %a1,%a3,>>,%a2@\+,%d1,%acc2
    6264:	a65a be09      	macl %a1,%a3,>>,%a2@\+,%a3,%acc1
    6268:	a6da be19      	macl %a1,%a3,>>,%a2@\+,%a3,%acc2
    626c:	a41a be09      	macl %a1,%a3,>>,%a2@\+,%d2,%acc1
    6270:	a49a be19      	macl %a1,%a3,>>,%a2@\+,%d2,%acc2
    6274:	ae5a be09      	macl %a1,%a3,>>,%a2@\+,%sp,%acc1
    6278:	aeda be19      	macl %a1,%a3,>>,%a2@\+,%sp,%acc2
    627c:	a21a be29      	macl %a1,%a3,>>,%a2@\+&,%d1,%acc1
    6280:	a29a be39      	macl %a1,%a3,>>,%a2@\+&,%d1,%acc2
    6284:	a65a be29      	macl %a1,%a3,>>,%a2@\+&,%a3,%acc1
    6288:	a6da be39      	macl %a1,%a3,>>,%a2@\+&,%a3,%acc2
    628c:	a41a be29      	macl %a1,%a3,>>,%a2@\+&,%d2,%acc1
    6290:	a49a be39      	macl %a1,%a3,>>,%a2@\+&,%d2,%acc2
    6294:	ae5a be29      	macl %a1,%a3,>>,%a2@\+&,%sp,%acc1
    6298:	aeda be39      	macl %a1,%a3,>>,%a2@\+&,%sp,%acc2
    629c:	a22e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1,%acc1
    62a2:	a2ae be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1,%acc2
    62a8:	a66e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3,%acc1
    62ae:	a6ee be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3,%acc2
    62b4:	a42e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2,%acc1
    62ba:	a4ae be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2,%acc2
    62c0:	ae6e be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp,%acc1
    62c6:	aeee be19 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp,%acc2
    62cc:	a22e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1,%acc1
    62d2:	a2ae be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1,%acc2
    62d8:	a66e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3,%acc1
    62de:	a6ee be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3,%acc2
    62e4:	a42e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2,%acc1
    62ea:	a4ae be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2,%acc2
    62f0:	ae6e be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp,%acc1
    62f6:	aeee be39 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp,%acc2
    62fc:	a221 be09      	macl %a1,%a3,>>,%a1@-,%d1,%acc1
    6300:	a2a1 be19      	macl %a1,%a3,>>,%a1@-,%d1,%acc2
    6304:	a661 be09      	macl %a1,%a3,>>,%a1@-,%a3,%acc1
    6308:	a6e1 be19      	macl %a1,%a3,>>,%a1@-,%a3,%acc2
    630c:	a421 be09      	macl %a1,%a3,>>,%a1@-,%d2,%acc1
    6310:	a4a1 be19      	macl %a1,%a3,>>,%a1@-,%d2,%acc2
    6314:	ae61 be09      	macl %a1,%a3,>>,%a1@-,%sp,%acc1
    6318:	aee1 be19      	macl %a1,%a3,>>,%a1@-,%sp,%acc2
    631c:	a221 be29      	macl %a1,%a3,>>,%a1@-&,%d1,%acc1
    6320:	a2a1 be39      	macl %a1,%a3,>>,%a1@-&,%d1,%acc2
    6324:	a661 be29      	macl %a1,%a3,>>,%a1@-&,%a3,%acc1
    6328:	a6e1 be39      	macl %a1,%a3,>>,%a1@-&,%a3,%acc2
    632c:	a421 be29      	macl %a1,%a3,>>,%a1@-&,%d2,%acc1
    6330:	a4a1 be39      	macl %a1,%a3,>>,%a1@-&,%d2,%acc2
    6334:	ae61 be29      	macl %a1,%a3,>>,%a1@-&,%sp,%acc1
    6338:	aee1 be39      	macl %a1,%a3,>>,%a1@-&,%sp,%acc2
    633c:	a213 4809      	macl %a1,%d4,%a3@,%d1,%acc1
    6340:	a293 4819      	macl %a1,%d4,%a3@,%d1,%acc2
    6344:	a653 4809      	macl %a1,%d4,%a3@,%a3,%acc1
    6348:	a6d3 4819      	macl %a1,%d4,%a3@,%a3,%acc2
    634c:	a413 4809      	macl %a1,%d4,%a3@,%d2,%acc1
    6350:	a493 4819      	macl %a1,%d4,%a3@,%d2,%acc2
    6354:	ae53 4809      	macl %a1,%d4,%a3@,%sp,%acc1
    6358:	aed3 4819      	macl %a1,%d4,%a3@,%sp,%acc2
    635c:	a213 4829      	macl %a1,%d4,%a3@&,%d1,%acc1
    6360:	a293 4839      	macl %a1,%d4,%a3@&,%d1,%acc2
    6364:	a653 4829      	macl %a1,%d4,%a3@&,%a3,%acc1
    6368:	a6d3 4839      	macl %a1,%d4,%a3@&,%a3,%acc2
    636c:	a413 4829      	macl %a1,%d4,%a3@&,%d2,%acc1
    6370:	a493 4839      	macl %a1,%d4,%a3@&,%d2,%acc2
    6374:	ae53 4829      	macl %a1,%d4,%a3@&,%sp,%acc1
    6378:	aed3 4839      	macl %a1,%d4,%a3@&,%sp,%acc2
    637c:	a21a 4809      	macl %a1,%d4,%a2@\+,%d1,%acc1
    6380:	a29a 4819      	macl %a1,%d4,%a2@\+,%d1,%acc2
    6384:	a65a 4809      	macl %a1,%d4,%a2@\+,%a3,%acc1
    6388:	a6da 4819      	macl %a1,%d4,%a2@\+,%a3,%acc2
    638c:	a41a 4809      	macl %a1,%d4,%a2@\+,%d2,%acc1
    6390:	a49a 4819      	macl %a1,%d4,%a2@\+,%d2,%acc2
    6394:	ae5a 4809      	macl %a1,%d4,%a2@\+,%sp,%acc1
    6398:	aeda 4819      	macl %a1,%d4,%a2@\+,%sp,%acc2
    639c:	a21a 4829      	macl %a1,%d4,%a2@\+&,%d1,%acc1
    63a0:	a29a 4839      	macl %a1,%d4,%a2@\+&,%d1,%acc2
    63a4:	a65a 4829      	macl %a1,%d4,%a2@\+&,%a3,%acc1
    63a8:	a6da 4839      	macl %a1,%d4,%a2@\+&,%a3,%acc2
    63ac:	a41a 4829      	macl %a1,%d4,%a2@\+&,%d2,%acc1
    63b0:	a49a 4839      	macl %a1,%d4,%a2@\+&,%d2,%acc2
    63b4:	ae5a 4829      	macl %a1,%d4,%a2@\+&,%sp,%acc1
    63b8:	aeda 4839      	macl %a1,%d4,%a2@\+&,%sp,%acc2
    63bc:	a22e 4809 000a 	macl %a1,%d4,%fp@\(10\),%d1,%acc1
    63c2:	a2ae 4819 000a 	macl %a1,%d4,%fp@\(10\),%d1,%acc2
    63c8:	a66e 4809 000a 	macl %a1,%d4,%fp@\(10\),%a3,%acc1
    63ce:	a6ee 4819 000a 	macl %a1,%d4,%fp@\(10\),%a3,%acc2
    63d4:	a42e 4809 000a 	macl %a1,%d4,%fp@\(10\),%d2,%acc1
    63da:	a4ae 4819 000a 	macl %a1,%d4,%fp@\(10\),%d2,%acc2
    63e0:	ae6e 4809 000a 	macl %a1,%d4,%fp@\(10\),%sp,%acc1
    63e6:	aeee 4819 000a 	macl %a1,%d4,%fp@\(10\),%sp,%acc2
    63ec:	a22e 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%d1,%acc1
    63f2:	a2ae 4839 000a 	macl %a1,%d4,%fp@\(10\)&,%d1,%acc2
    63f8:	a66e 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%a3,%acc1
    63fe:	a6ee 4839 000a 	macl %a1,%d4,%fp@\(10\)&,%a3,%acc2
    6404:	a42e 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%d2,%acc1
    640a:	a4ae 4839 000a 	macl %a1,%d4,%fp@\(10\)&,%d2,%acc2
    6410:	ae6e 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%sp,%acc1
    6416:	aeee 4839 000a 	macl %a1,%d4,%fp@\(10\)&,%sp,%acc2
    641c:	a221 4809      	macl %a1,%d4,%a1@-,%d1,%acc1
    6420:	a2a1 4819      	macl %a1,%d4,%a1@-,%d1,%acc2
    6424:	a661 4809      	macl %a1,%d4,%a1@-,%a3,%acc1
    6428:	a6e1 4819      	macl %a1,%d4,%a1@-,%a3,%acc2
    642c:	a421 4809      	macl %a1,%d4,%a1@-,%d2,%acc1
    6430:	a4a1 4819      	macl %a1,%d4,%a1@-,%d2,%acc2
    6434:	ae61 4809      	macl %a1,%d4,%a1@-,%sp,%acc1
    6438:	aee1 4819      	macl %a1,%d4,%a1@-,%sp,%acc2
    643c:	a221 4829      	macl %a1,%d4,%a1@-&,%d1,%acc1
    6440:	a2a1 4839      	macl %a1,%d4,%a1@-&,%d1,%acc2
    6444:	a661 4829      	macl %a1,%d4,%a1@-&,%a3,%acc1
    6448:	a6e1 4839      	macl %a1,%d4,%a1@-&,%a3,%acc2
    644c:	a421 4829      	macl %a1,%d4,%a1@-&,%d2,%acc1
    6450:	a4a1 4839      	macl %a1,%d4,%a1@-&,%d2,%acc2
    6454:	ae61 4829      	macl %a1,%d4,%a1@-&,%sp,%acc1
    6458:	aee1 4839      	macl %a1,%d4,%a1@-&,%sp,%acc2
    645c:	a213 4a09      	macl %a1,%d4,<<,%a3@,%d1,%acc1
    6460:	a293 4a19      	macl %a1,%d4,<<,%a3@,%d1,%acc2
    6464:	a653 4a09      	macl %a1,%d4,<<,%a3@,%a3,%acc1
    6468:	a6d3 4a19      	macl %a1,%d4,<<,%a3@,%a3,%acc2
    646c:	a413 4a09      	macl %a1,%d4,<<,%a3@,%d2,%acc1
    6470:	a493 4a19      	macl %a1,%d4,<<,%a3@,%d2,%acc2
    6474:	ae53 4a09      	macl %a1,%d4,<<,%a3@,%sp,%acc1
    6478:	aed3 4a19      	macl %a1,%d4,<<,%a3@,%sp,%acc2
    647c:	a213 4a29      	macl %a1,%d4,<<,%a3@&,%d1,%acc1
    6480:	a293 4a39      	macl %a1,%d4,<<,%a3@&,%d1,%acc2
    6484:	a653 4a29      	macl %a1,%d4,<<,%a3@&,%a3,%acc1
    6488:	a6d3 4a39      	macl %a1,%d4,<<,%a3@&,%a3,%acc2
    648c:	a413 4a29      	macl %a1,%d4,<<,%a3@&,%d2,%acc1
    6490:	a493 4a39      	macl %a1,%d4,<<,%a3@&,%d2,%acc2
    6494:	ae53 4a29      	macl %a1,%d4,<<,%a3@&,%sp,%acc1
    6498:	aed3 4a39      	macl %a1,%d4,<<,%a3@&,%sp,%acc2
    649c:	a21a 4a09      	macl %a1,%d4,<<,%a2@\+,%d1,%acc1
    64a0:	a29a 4a19      	macl %a1,%d4,<<,%a2@\+,%d1,%acc2
    64a4:	a65a 4a09      	macl %a1,%d4,<<,%a2@\+,%a3,%acc1
    64a8:	a6da 4a19      	macl %a1,%d4,<<,%a2@\+,%a3,%acc2
    64ac:	a41a 4a09      	macl %a1,%d4,<<,%a2@\+,%d2,%acc1
    64b0:	a49a 4a19      	macl %a1,%d4,<<,%a2@\+,%d2,%acc2
    64b4:	ae5a 4a09      	macl %a1,%d4,<<,%a2@\+,%sp,%acc1
    64b8:	aeda 4a19      	macl %a1,%d4,<<,%a2@\+,%sp,%acc2
    64bc:	a21a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d1,%acc1
    64c0:	a29a 4a39      	macl %a1,%d4,<<,%a2@\+&,%d1,%acc2
    64c4:	a65a 4a29      	macl %a1,%d4,<<,%a2@\+&,%a3,%acc1
    64c8:	a6da 4a39      	macl %a1,%d4,<<,%a2@\+&,%a3,%acc2
    64cc:	a41a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d2,%acc1
    64d0:	a49a 4a39      	macl %a1,%d4,<<,%a2@\+&,%d2,%acc2
    64d4:	ae5a 4a29      	macl %a1,%d4,<<,%a2@\+&,%sp,%acc1
    64d8:	aeda 4a39      	macl %a1,%d4,<<,%a2@\+&,%sp,%acc2
    64dc:	a22e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1,%acc1
    64e2:	a2ae 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1,%acc2
    64e8:	a66e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3,%acc1
    64ee:	a6ee 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3,%acc2
    64f4:	a42e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2,%acc1
    64fa:	a4ae 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2,%acc2
    6500:	ae6e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp,%acc1
    6506:	aeee 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp,%acc2
    650c:	a22e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1,%acc1
    6512:	a2ae 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1,%acc2
    6518:	a66e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3,%acc1
    651e:	a6ee 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3,%acc2
    6524:	a42e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2,%acc1
    652a:	a4ae 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2,%acc2
    6530:	ae6e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp,%acc1
    6536:	aeee 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp,%acc2
    653c:	a221 4a09      	macl %a1,%d4,<<,%a1@-,%d1,%acc1
    6540:	a2a1 4a19      	macl %a1,%d4,<<,%a1@-,%d1,%acc2
    6544:	a661 4a09      	macl %a1,%d4,<<,%a1@-,%a3,%acc1
    6548:	a6e1 4a19      	macl %a1,%d4,<<,%a1@-,%a3,%acc2
    654c:	a421 4a09      	macl %a1,%d4,<<,%a1@-,%d2,%acc1
    6550:	a4a1 4a19      	macl %a1,%d4,<<,%a1@-,%d2,%acc2
    6554:	ae61 4a09      	macl %a1,%d4,<<,%a1@-,%sp,%acc1
    6558:	aee1 4a19      	macl %a1,%d4,<<,%a1@-,%sp,%acc2
    655c:	a221 4a29      	macl %a1,%d4,<<,%a1@-&,%d1,%acc1
    6560:	a2a1 4a39      	macl %a1,%d4,<<,%a1@-&,%d1,%acc2
    6564:	a661 4a29      	macl %a1,%d4,<<,%a1@-&,%a3,%acc1
    6568:	a6e1 4a39      	macl %a1,%d4,<<,%a1@-&,%a3,%acc2
    656c:	a421 4a29      	macl %a1,%d4,<<,%a1@-&,%d2,%acc1
    6570:	a4a1 4a39      	macl %a1,%d4,<<,%a1@-&,%d2,%acc2
    6574:	ae61 4a29      	macl %a1,%d4,<<,%a1@-&,%sp,%acc1
    6578:	aee1 4a39      	macl %a1,%d4,<<,%a1@-&,%sp,%acc2
    657c:	a213 4e09      	macl %a1,%d4,>>,%a3@,%d1,%acc1
    6580:	a293 4e19      	macl %a1,%d4,>>,%a3@,%d1,%acc2
    6584:	a653 4e09      	macl %a1,%d4,>>,%a3@,%a3,%acc1
    6588:	a6d3 4e19      	macl %a1,%d4,>>,%a3@,%a3,%acc2
    658c:	a413 4e09      	macl %a1,%d4,>>,%a3@,%d2,%acc1
    6590:	a493 4e19      	macl %a1,%d4,>>,%a3@,%d2,%acc2
    6594:	ae53 4e09      	macl %a1,%d4,>>,%a3@,%sp,%acc1
    6598:	aed3 4e19      	macl %a1,%d4,>>,%a3@,%sp,%acc2
    659c:	a213 4e29      	macl %a1,%d4,>>,%a3@&,%d1,%acc1
    65a0:	a293 4e39      	macl %a1,%d4,>>,%a3@&,%d1,%acc2
    65a4:	a653 4e29      	macl %a1,%d4,>>,%a3@&,%a3,%acc1
    65a8:	a6d3 4e39      	macl %a1,%d4,>>,%a3@&,%a3,%acc2
    65ac:	a413 4e29      	macl %a1,%d4,>>,%a3@&,%d2,%acc1
    65b0:	a493 4e39      	macl %a1,%d4,>>,%a3@&,%d2,%acc2
    65b4:	ae53 4e29      	macl %a1,%d4,>>,%a3@&,%sp,%acc1
    65b8:	aed3 4e39      	macl %a1,%d4,>>,%a3@&,%sp,%acc2
    65bc:	a21a 4e09      	macl %a1,%d4,>>,%a2@\+,%d1,%acc1
    65c0:	a29a 4e19      	macl %a1,%d4,>>,%a2@\+,%d1,%acc2
    65c4:	a65a 4e09      	macl %a1,%d4,>>,%a2@\+,%a3,%acc1
    65c8:	a6da 4e19      	macl %a1,%d4,>>,%a2@\+,%a3,%acc2
    65cc:	a41a 4e09      	macl %a1,%d4,>>,%a2@\+,%d2,%acc1
    65d0:	a49a 4e19      	macl %a1,%d4,>>,%a2@\+,%d2,%acc2
    65d4:	ae5a 4e09      	macl %a1,%d4,>>,%a2@\+,%sp,%acc1
    65d8:	aeda 4e19      	macl %a1,%d4,>>,%a2@\+,%sp,%acc2
    65dc:	a21a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d1,%acc1
    65e0:	a29a 4e39      	macl %a1,%d4,>>,%a2@\+&,%d1,%acc2
    65e4:	a65a 4e29      	macl %a1,%d4,>>,%a2@\+&,%a3,%acc1
    65e8:	a6da 4e39      	macl %a1,%d4,>>,%a2@\+&,%a3,%acc2
    65ec:	a41a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d2,%acc1
    65f0:	a49a 4e39      	macl %a1,%d4,>>,%a2@\+&,%d2,%acc2
    65f4:	ae5a 4e29      	macl %a1,%d4,>>,%a2@\+&,%sp,%acc1
    65f8:	aeda 4e39      	macl %a1,%d4,>>,%a2@\+&,%sp,%acc2
    65fc:	a22e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1,%acc1
    6602:	a2ae 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1,%acc2
    6608:	a66e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3,%acc1
    660e:	a6ee 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3,%acc2
    6614:	a42e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2,%acc1
    661a:	a4ae 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2,%acc2
    6620:	ae6e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp,%acc1
    6626:	aeee 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp,%acc2
    662c:	a22e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1,%acc1
    6632:	a2ae 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1,%acc2
    6638:	a66e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3,%acc1
    663e:	a6ee 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3,%acc2
    6644:	a42e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2,%acc1
    664a:	a4ae 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2,%acc2
    6650:	ae6e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp,%acc1
    6656:	aeee 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp,%acc2
    665c:	a221 4e09      	macl %a1,%d4,>>,%a1@-,%d1,%acc1
    6660:	a2a1 4e19      	macl %a1,%d4,>>,%a1@-,%d1,%acc2
    6664:	a661 4e09      	macl %a1,%d4,>>,%a1@-,%a3,%acc1
    6668:	a6e1 4e19      	macl %a1,%d4,>>,%a1@-,%a3,%acc2
    666c:	a421 4e09      	macl %a1,%d4,>>,%a1@-,%d2,%acc1
    6670:	a4a1 4e19      	macl %a1,%d4,>>,%a1@-,%d2,%acc2
    6674:	ae61 4e09      	macl %a1,%d4,>>,%a1@-,%sp,%acc1
    6678:	aee1 4e19      	macl %a1,%d4,>>,%a1@-,%sp,%acc2
    667c:	a221 4e29      	macl %a1,%d4,>>,%a1@-&,%d1,%acc1
    6680:	a2a1 4e39      	macl %a1,%d4,>>,%a1@-&,%d1,%acc2
    6684:	a661 4e29      	macl %a1,%d4,>>,%a1@-&,%a3,%acc1
    6688:	a6e1 4e39      	macl %a1,%d4,>>,%a1@-&,%a3,%acc2
    668c:	a421 4e29      	macl %a1,%d4,>>,%a1@-&,%d2,%acc1
    6690:	a4a1 4e39      	macl %a1,%d4,>>,%a1@-&,%d2,%acc2
    6694:	ae61 4e29      	macl %a1,%d4,>>,%a1@-&,%sp,%acc1
    6698:	aee1 4e39      	macl %a1,%d4,>>,%a1@-&,%sp,%acc2
    669c:	a213 4a09      	macl %a1,%d4,<<,%a3@,%d1,%acc1
    66a0:	a293 4a19      	macl %a1,%d4,<<,%a3@,%d1,%acc2
    66a4:	a653 4a09      	macl %a1,%d4,<<,%a3@,%a3,%acc1
    66a8:	a6d3 4a19      	macl %a1,%d4,<<,%a3@,%a3,%acc2
    66ac:	a413 4a09      	macl %a1,%d4,<<,%a3@,%d2,%acc1
    66b0:	a493 4a19      	macl %a1,%d4,<<,%a3@,%d2,%acc2
    66b4:	ae53 4a09      	macl %a1,%d4,<<,%a3@,%sp,%acc1
    66b8:	aed3 4a19      	macl %a1,%d4,<<,%a3@,%sp,%acc2
    66bc:	a213 4a29      	macl %a1,%d4,<<,%a3@&,%d1,%acc1
    66c0:	a293 4a39      	macl %a1,%d4,<<,%a3@&,%d1,%acc2
    66c4:	a653 4a29      	macl %a1,%d4,<<,%a3@&,%a3,%acc1
    66c8:	a6d3 4a39      	macl %a1,%d4,<<,%a3@&,%a3,%acc2
    66cc:	a413 4a29      	macl %a1,%d4,<<,%a3@&,%d2,%acc1
    66d0:	a493 4a39      	macl %a1,%d4,<<,%a3@&,%d2,%acc2
    66d4:	ae53 4a29      	macl %a1,%d4,<<,%a3@&,%sp,%acc1
    66d8:	aed3 4a39      	macl %a1,%d4,<<,%a3@&,%sp,%acc2
    66dc:	a21a 4a09      	macl %a1,%d4,<<,%a2@\+,%d1,%acc1
    66e0:	a29a 4a19      	macl %a1,%d4,<<,%a2@\+,%d1,%acc2
    66e4:	a65a 4a09      	macl %a1,%d4,<<,%a2@\+,%a3,%acc1
    66e8:	a6da 4a19      	macl %a1,%d4,<<,%a2@\+,%a3,%acc2
    66ec:	a41a 4a09      	macl %a1,%d4,<<,%a2@\+,%d2,%acc1
    66f0:	a49a 4a19      	macl %a1,%d4,<<,%a2@\+,%d2,%acc2
    66f4:	ae5a 4a09      	macl %a1,%d4,<<,%a2@\+,%sp,%acc1
    66f8:	aeda 4a19      	macl %a1,%d4,<<,%a2@\+,%sp,%acc2
    66fc:	a21a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d1,%acc1
    6700:	a29a 4a39      	macl %a1,%d4,<<,%a2@\+&,%d1,%acc2
    6704:	a65a 4a29      	macl %a1,%d4,<<,%a2@\+&,%a3,%acc1
    6708:	a6da 4a39      	macl %a1,%d4,<<,%a2@\+&,%a3,%acc2
    670c:	a41a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d2,%acc1
    6710:	a49a 4a39      	macl %a1,%d4,<<,%a2@\+&,%d2,%acc2
    6714:	ae5a 4a29      	macl %a1,%d4,<<,%a2@\+&,%sp,%acc1
    6718:	aeda 4a39      	macl %a1,%d4,<<,%a2@\+&,%sp,%acc2
    671c:	a22e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1,%acc1
    6722:	a2ae 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1,%acc2
    6728:	a66e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3,%acc1
    672e:	a6ee 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3,%acc2
    6734:	a42e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2,%acc1
    673a:	a4ae 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2,%acc2
    6740:	ae6e 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp,%acc1
    6746:	aeee 4a19 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp,%acc2
    674c:	a22e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1,%acc1
    6752:	a2ae 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1,%acc2
    6758:	a66e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3,%acc1
    675e:	a6ee 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3,%acc2
    6764:	a42e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2,%acc1
    676a:	a4ae 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2,%acc2
    6770:	ae6e 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp,%acc1
    6776:	aeee 4a39 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp,%acc2
    677c:	a221 4a09      	macl %a1,%d4,<<,%a1@-,%d1,%acc1
    6780:	a2a1 4a19      	macl %a1,%d4,<<,%a1@-,%d1,%acc2
    6784:	a661 4a09      	macl %a1,%d4,<<,%a1@-,%a3,%acc1
    6788:	a6e1 4a19      	macl %a1,%d4,<<,%a1@-,%a3,%acc2
    678c:	a421 4a09      	macl %a1,%d4,<<,%a1@-,%d2,%acc1
    6790:	a4a1 4a19      	macl %a1,%d4,<<,%a1@-,%d2,%acc2
    6794:	ae61 4a09      	macl %a1,%d4,<<,%a1@-,%sp,%acc1
    6798:	aee1 4a19      	macl %a1,%d4,<<,%a1@-,%sp,%acc2
    679c:	a221 4a29      	macl %a1,%d4,<<,%a1@-&,%d1,%acc1
    67a0:	a2a1 4a39      	macl %a1,%d4,<<,%a1@-&,%d1,%acc2
    67a4:	a661 4a29      	macl %a1,%d4,<<,%a1@-&,%a3,%acc1
    67a8:	a6e1 4a39      	macl %a1,%d4,<<,%a1@-&,%a3,%acc2
    67ac:	a421 4a29      	macl %a1,%d4,<<,%a1@-&,%d2,%acc1
    67b0:	a4a1 4a39      	macl %a1,%d4,<<,%a1@-&,%d2,%acc2
    67b4:	ae61 4a29      	macl %a1,%d4,<<,%a1@-&,%sp,%acc1
    67b8:	aee1 4a39      	macl %a1,%d4,<<,%a1@-&,%sp,%acc2
    67bc:	a213 4e09      	macl %a1,%d4,>>,%a3@,%d1,%acc1
    67c0:	a293 4e19      	macl %a1,%d4,>>,%a3@,%d1,%acc2
    67c4:	a653 4e09      	macl %a1,%d4,>>,%a3@,%a3,%acc1
    67c8:	a6d3 4e19      	macl %a1,%d4,>>,%a3@,%a3,%acc2
    67cc:	a413 4e09      	macl %a1,%d4,>>,%a3@,%d2,%acc1
    67d0:	a493 4e19      	macl %a1,%d4,>>,%a3@,%d2,%acc2
    67d4:	ae53 4e09      	macl %a1,%d4,>>,%a3@,%sp,%acc1
    67d8:	aed3 4e19      	macl %a1,%d4,>>,%a3@,%sp,%acc2
    67dc:	a213 4e29      	macl %a1,%d4,>>,%a3@&,%d1,%acc1
    67e0:	a293 4e39      	macl %a1,%d4,>>,%a3@&,%d1,%acc2
    67e4:	a653 4e29      	macl %a1,%d4,>>,%a3@&,%a3,%acc1
    67e8:	a6d3 4e39      	macl %a1,%d4,>>,%a3@&,%a3,%acc2
    67ec:	a413 4e29      	macl %a1,%d4,>>,%a3@&,%d2,%acc1
    67f0:	a493 4e39      	macl %a1,%d4,>>,%a3@&,%d2,%acc2
    67f4:	ae53 4e29      	macl %a1,%d4,>>,%a3@&,%sp,%acc1
    67f8:	aed3 4e39      	macl %a1,%d4,>>,%a3@&,%sp,%acc2
    67fc:	a21a 4e09      	macl %a1,%d4,>>,%a2@\+,%d1,%acc1
    6800:	a29a 4e19      	macl %a1,%d4,>>,%a2@\+,%d1,%acc2
    6804:	a65a 4e09      	macl %a1,%d4,>>,%a2@\+,%a3,%acc1
    6808:	a6da 4e19      	macl %a1,%d4,>>,%a2@\+,%a3,%acc2
    680c:	a41a 4e09      	macl %a1,%d4,>>,%a2@\+,%d2,%acc1
    6810:	a49a 4e19      	macl %a1,%d4,>>,%a2@\+,%d2,%acc2
    6814:	ae5a 4e09      	macl %a1,%d4,>>,%a2@\+,%sp,%acc1
    6818:	aeda 4e19      	macl %a1,%d4,>>,%a2@\+,%sp,%acc2
    681c:	a21a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d1,%acc1
    6820:	a29a 4e39      	macl %a1,%d4,>>,%a2@\+&,%d1,%acc2
    6824:	a65a 4e29      	macl %a1,%d4,>>,%a2@\+&,%a3,%acc1
    6828:	a6da 4e39      	macl %a1,%d4,>>,%a2@\+&,%a3,%acc2
    682c:	a41a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d2,%acc1
    6830:	a49a 4e39      	macl %a1,%d4,>>,%a2@\+&,%d2,%acc2
    6834:	ae5a 4e29      	macl %a1,%d4,>>,%a2@\+&,%sp,%acc1
    6838:	aeda 4e39      	macl %a1,%d4,>>,%a2@\+&,%sp,%acc2
    683c:	a22e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1,%acc1
    6842:	a2ae 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1,%acc2
    6848:	a66e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3,%acc1
    684e:	a6ee 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3,%acc2
    6854:	a42e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2,%acc1
    685a:	a4ae 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2,%acc2
    6860:	ae6e 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp,%acc1
    6866:	aeee 4e19 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp,%acc2
    686c:	a22e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1,%acc1
    6872:	a2ae 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1,%acc2
    6878:	a66e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3,%acc1
    687e:	a6ee 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3,%acc2
    6884:	a42e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2,%acc1
    688a:	a4ae 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2,%acc2
    6890:	ae6e 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp,%acc1
    6896:	aeee 4e39 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp,%acc2
    689c:	a221 4e09      	macl %a1,%d4,>>,%a1@-,%d1,%acc1
    68a0:	a2a1 4e19      	macl %a1,%d4,>>,%a1@-,%d1,%acc2
    68a4:	a661 4e09      	macl %a1,%d4,>>,%a1@-,%a3,%acc1
    68a8:	a6e1 4e19      	macl %a1,%d4,>>,%a1@-,%a3,%acc2
    68ac:	a421 4e09      	macl %a1,%d4,>>,%a1@-,%d2,%acc1
    68b0:	a4a1 4e19      	macl %a1,%d4,>>,%a1@-,%d2,%acc2
    68b4:	ae61 4e09      	macl %a1,%d4,>>,%a1@-,%sp,%acc1
    68b8:	aee1 4e19      	macl %a1,%d4,>>,%a1@-,%sp,%acc2
    68bc:	a221 4e29      	macl %a1,%d4,>>,%a1@-&,%d1,%acc1
    68c0:	a2a1 4e39      	macl %a1,%d4,>>,%a1@-&,%d1,%acc2
    68c4:	a661 4e29      	macl %a1,%d4,>>,%a1@-&,%a3,%acc1
    68c8:	a6e1 4e39      	macl %a1,%d4,>>,%a1@-&,%a3,%acc2
    68cc:	a421 4e29      	macl %a1,%d4,>>,%a1@-&,%d2,%acc1
    68d0:	a4a1 4e39      	macl %a1,%d4,>>,%a1@-&,%d2,%acc2
    68d4:	ae61 4e29      	macl %a1,%d4,>>,%a1@-&,%sp,%acc1
    68d8:	aee1 4e39      	macl %a1,%d4,>>,%a1@-&,%sp,%acc2
    68dc:	a213 b806      	macl %d6,%a3,%a3@,%d1,%acc1
    68e0:	a293 b816      	macl %d6,%a3,%a3@,%d1,%acc2
    68e4:	a653 b806      	macl %d6,%a3,%a3@,%a3,%acc1
    68e8:	a6d3 b816      	macl %d6,%a3,%a3@,%a3,%acc2
    68ec:	a413 b806      	macl %d6,%a3,%a3@,%d2,%acc1
    68f0:	a493 b816      	macl %d6,%a3,%a3@,%d2,%acc2
    68f4:	ae53 b806      	macl %d6,%a3,%a3@,%sp,%acc1
    68f8:	aed3 b816      	macl %d6,%a3,%a3@,%sp,%acc2
    68fc:	a213 b826      	macl %d6,%a3,%a3@&,%d1,%acc1
    6900:	a293 b836      	macl %d6,%a3,%a3@&,%d1,%acc2
    6904:	a653 b826      	macl %d6,%a3,%a3@&,%a3,%acc1
    6908:	a6d3 b836      	macl %d6,%a3,%a3@&,%a3,%acc2
    690c:	a413 b826      	macl %d6,%a3,%a3@&,%d2,%acc1
    6910:	a493 b836      	macl %d6,%a3,%a3@&,%d2,%acc2
    6914:	ae53 b826      	macl %d6,%a3,%a3@&,%sp,%acc1
    6918:	aed3 b836      	macl %d6,%a3,%a3@&,%sp,%acc2
    691c:	a21a b806      	macl %d6,%a3,%a2@\+,%d1,%acc1
    6920:	a29a b816      	macl %d6,%a3,%a2@\+,%d1,%acc2
    6924:	a65a b806      	macl %d6,%a3,%a2@\+,%a3,%acc1
    6928:	a6da b816      	macl %d6,%a3,%a2@\+,%a3,%acc2
    692c:	a41a b806      	macl %d6,%a3,%a2@\+,%d2,%acc1
    6930:	a49a b816      	macl %d6,%a3,%a2@\+,%d2,%acc2
    6934:	ae5a b806      	macl %d6,%a3,%a2@\+,%sp,%acc1
    6938:	aeda b816      	macl %d6,%a3,%a2@\+,%sp,%acc2
    693c:	a21a b826      	macl %d6,%a3,%a2@\+&,%d1,%acc1
    6940:	a29a b836      	macl %d6,%a3,%a2@\+&,%d1,%acc2
    6944:	a65a b826      	macl %d6,%a3,%a2@\+&,%a3,%acc1
    6948:	a6da b836      	macl %d6,%a3,%a2@\+&,%a3,%acc2
    694c:	a41a b826      	macl %d6,%a3,%a2@\+&,%d2,%acc1
    6950:	a49a b836      	macl %d6,%a3,%a2@\+&,%d2,%acc2
    6954:	ae5a b826      	macl %d6,%a3,%a2@\+&,%sp,%acc1
    6958:	aeda b836      	macl %d6,%a3,%a2@\+&,%sp,%acc2
    695c:	a22e b806 000a 	macl %d6,%a3,%fp@\(10\),%d1,%acc1
    6962:	a2ae b816 000a 	macl %d6,%a3,%fp@\(10\),%d1,%acc2
    6968:	a66e b806 000a 	macl %d6,%a3,%fp@\(10\),%a3,%acc1
    696e:	a6ee b816 000a 	macl %d6,%a3,%fp@\(10\),%a3,%acc2
    6974:	a42e b806 000a 	macl %d6,%a3,%fp@\(10\),%d2,%acc1
    697a:	a4ae b816 000a 	macl %d6,%a3,%fp@\(10\),%d2,%acc2
    6980:	ae6e b806 000a 	macl %d6,%a3,%fp@\(10\),%sp,%acc1
    6986:	aeee b816 000a 	macl %d6,%a3,%fp@\(10\),%sp,%acc2
    698c:	a22e b826 000a 	macl %d6,%a3,%fp@\(10\)&,%d1,%acc1
    6992:	a2ae b836 000a 	macl %d6,%a3,%fp@\(10\)&,%d1,%acc2
    6998:	a66e b826 000a 	macl %d6,%a3,%fp@\(10\)&,%a3,%acc1
    699e:	a6ee b836 000a 	macl %d6,%a3,%fp@\(10\)&,%a3,%acc2
    69a4:	a42e b826 000a 	macl %d6,%a3,%fp@\(10\)&,%d2,%acc1
    69aa:	a4ae b836 000a 	macl %d6,%a3,%fp@\(10\)&,%d2,%acc2
    69b0:	ae6e b826 000a 	macl %d6,%a3,%fp@\(10\)&,%sp,%acc1
    69b6:	aeee b836 000a 	macl %d6,%a3,%fp@\(10\)&,%sp,%acc2
    69bc:	a221 b806      	macl %d6,%a3,%a1@-,%d1,%acc1
    69c0:	a2a1 b816      	macl %d6,%a3,%a1@-,%d1,%acc2
    69c4:	a661 b806      	macl %d6,%a3,%a1@-,%a3,%acc1
    69c8:	a6e1 b816      	macl %d6,%a3,%a1@-,%a3,%acc2
    69cc:	a421 b806      	macl %d6,%a3,%a1@-,%d2,%acc1
    69d0:	a4a1 b816      	macl %d6,%a3,%a1@-,%d2,%acc2
    69d4:	ae61 b806      	macl %d6,%a3,%a1@-,%sp,%acc1
    69d8:	aee1 b816      	macl %d6,%a3,%a1@-,%sp,%acc2
    69dc:	a221 b826      	macl %d6,%a3,%a1@-&,%d1,%acc1
    69e0:	a2a1 b836      	macl %d6,%a3,%a1@-&,%d1,%acc2
    69e4:	a661 b826      	macl %d6,%a3,%a1@-&,%a3,%acc1
    69e8:	a6e1 b836      	macl %d6,%a3,%a1@-&,%a3,%acc2
    69ec:	a421 b826      	macl %d6,%a3,%a1@-&,%d2,%acc1
    69f0:	a4a1 b836      	macl %d6,%a3,%a1@-&,%d2,%acc2
    69f4:	ae61 b826      	macl %d6,%a3,%a1@-&,%sp,%acc1
    69f8:	aee1 b836      	macl %d6,%a3,%a1@-&,%sp,%acc2
    69fc:	a213 ba06      	macl %d6,%a3,<<,%a3@,%d1,%acc1
    6a00:	a293 ba16      	macl %d6,%a3,<<,%a3@,%d1,%acc2
    6a04:	a653 ba06      	macl %d6,%a3,<<,%a3@,%a3,%acc1
    6a08:	a6d3 ba16      	macl %d6,%a3,<<,%a3@,%a3,%acc2
    6a0c:	a413 ba06      	macl %d6,%a3,<<,%a3@,%d2,%acc1
    6a10:	a493 ba16      	macl %d6,%a3,<<,%a3@,%d2,%acc2
    6a14:	ae53 ba06      	macl %d6,%a3,<<,%a3@,%sp,%acc1
    6a18:	aed3 ba16      	macl %d6,%a3,<<,%a3@,%sp,%acc2
    6a1c:	a213 ba26      	macl %d6,%a3,<<,%a3@&,%d1,%acc1
    6a20:	a293 ba36      	macl %d6,%a3,<<,%a3@&,%d1,%acc2
    6a24:	a653 ba26      	macl %d6,%a3,<<,%a3@&,%a3,%acc1
    6a28:	a6d3 ba36      	macl %d6,%a3,<<,%a3@&,%a3,%acc2
    6a2c:	a413 ba26      	macl %d6,%a3,<<,%a3@&,%d2,%acc1
    6a30:	a493 ba36      	macl %d6,%a3,<<,%a3@&,%d2,%acc2
    6a34:	ae53 ba26      	macl %d6,%a3,<<,%a3@&,%sp,%acc1
    6a38:	aed3 ba36      	macl %d6,%a3,<<,%a3@&,%sp,%acc2
    6a3c:	a21a ba06      	macl %d6,%a3,<<,%a2@\+,%d1,%acc1
    6a40:	a29a ba16      	macl %d6,%a3,<<,%a2@\+,%d1,%acc2
    6a44:	a65a ba06      	macl %d6,%a3,<<,%a2@\+,%a3,%acc1
    6a48:	a6da ba16      	macl %d6,%a3,<<,%a2@\+,%a3,%acc2
    6a4c:	a41a ba06      	macl %d6,%a3,<<,%a2@\+,%d2,%acc1
    6a50:	a49a ba16      	macl %d6,%a3,<<,%a2@\+,%d2,%acc2
    6a54:	ae5a ba06      	macl %d6,%a3,<<,%a2@\+,%sp,%acc1
    6a58:	aeda ba16      	macl %d6,%a3,<<,%a2@\+,%sp,%acc2
    6a5c:	a21a ba26      	macl %d6,%a3,<<,%a2@\+&,%d1,%acc1
    6a60:	a29a ba36      	macl %d6,%a3,<<,%a2@\+&,%d1,%acc2
    6a64:	a65a ba26      	macl %d6,%a3,<<,%a2@\+&,%a3,%acc1
    6a68:	a6da ba36      	macl %d6,%a3,<<,%a2@\+&,%a3,%acc2
    6a6c:	a41a ba26      	macl %d6,%a3,<<,%a2@\+&,%d2,%acc1
    6a70:	a49a ba36      	macl %d6,%a3,<<,%a2@\+&,%d2,%acc2
    6a74:	ae5a ba26      	macl %d6,%a3,<<,%a2@\+&,%sp,%acc1
    6a78:	aeda ba36      	macl %d6,%a3,<<,%a2@\+&,%sp,%acc2
    6a7c:	a22e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1,%acc1
    6a82:	a2ae ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1,%acc2
    6a88:	a66e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3,%acc1
    6a8e:	a6ee ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3,%acc2
    6a94:	a42e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2,%acc1
    6a9a:	a4ae ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2,%acc2
    6aa0:	ae6e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp,%acc1
    6aa6:	aeee ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp,%acc2
    6aac:	a22e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1,%acc1
    6ab2:	a2ae ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1,%acc2
    6ab8:	a66e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3,%acc1
    6abe:	a6ee ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3,%acc2
    6ac4:	a42e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2,%acc1
    6aca:	a4ae ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2,%acc2
    6ad0:	ae6e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp,%acc1
    6ad6:	aeee ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp,%acc2
    6adc:	a221 ba06      	macl %d6,%a3,<<,%a1@-,%d1,%acc1
    6ae0:	a2a1 ba16      	macl %d6,%a3,<<,%a1@-,%d1,%acc2
    6ae4:	a661 ba06      	macl %d6,%a3,<<,%a1@-,%a3,%acc1
    6ae8:	a6e1 ba16      	macl %d6,%a3,<<,%a1@-,%a3,%acc2
    6aec:	a421 ba06      	macl %d6,%a3,<<,%a1@-,%d2,%acc1
    6af0:	a4a1 ba16      	macl %d6,%a3,<<,%a1@-,%d2,%acc2
    6af4:	ae61 ba06      	macl %d6,%a3,<<,%a1@-,%sp,%acc1
    6af8:	aee1 ba16      	macl %d6,%a3,<<,%a1@-,%sp,%acc2
    6afc:	a221 ba26      	macl %d6,%a3,<<,%a1@-&,%d1,%acc1
    6b00:	a2a1 ba36      	macl %d6,%a3,<<,%a1@-&,%d1,%acc2
    6b04:	a661 ba26      	macl %d6,%a3,<<,%a1@-&,%a3,%acc1
    6b08:	a6e1 ba36      	macl %d6,%a3,<<,%a1@-&,%a3,%acc2
    6b0c:	a421 ba26      	macl %d6,%a3,<<,%a1@-&,%d2,%acc1
    6b10:	a4a1 ba36      	macl %d6,%a3,<<,%a1@-&,%d2,%acc2
    6b14:	ae61 ba26      	macl %d6,%a3,<<,%a1@-&,%sp,%acc1
    6b18:	aee1 ba36      	macl %d6,%a3,<<,%a1@-&,%sp,%acc2
    6b1c:	a213 be06      	macl %d6,%a3,>>,%a3@,%d1,%acc1
    6b20:	a293 be16      	macl %d6,%a3,>>,%a3@,%d1,%acc2
    6b24:	a653 be06      	macl %d6,%a3,>>,%a3@,%a3,%acc1
    6b28:	a6d3 be16      	macl %d6,%a3,>>,%a3@,%a3,%acc2
    6b2c:	a413 be06      	macl %d6,%a3,>>,%a3@,%d2,%acc1
    6b30:	a493 be16      	macl %d6,%a3,>>,%a3@,%d2,%acc2
    6b34:	ae53 be06      	macl %d6,%a3,>>,%a3@,%sp,%acc1
    6b38:	aed3 be16      	macl %d6,%a3,>>,%a3@,%sp,%acc2
    6b3c:	a213 be26      	macl %d6,%a3,>>,%a3@&,%d1,%acc1
    6b40:	a293 be36      	macl %d6,%a3,>>,%a3@&,%d1,%acc2
    6b44:	a653 be26      	macl %d6,%a3,>>,%a3@&,%a3,%acc1
    6b48:	a6d3 be36      	macl %d6,%a3,>>,%a3@&,%a3,%acc2
    6b4c:	a413 be26      	macl %d6,%a3,>>,%a3@&,%d2,%acc1
    6b50:	a493 be36      	macl %d6,%a3,>>,%a3@&,%d2,%acc2
    6b54:	ae53 be26      	macl %d6,%a3,>>,%a3@&,%sp,%acc1
    6b58:	aed3 be36      	macl %d6,%a3,>>,%a3@&,%sp,%acc2
    6b5c:	a21a be06      	macl %d6,%a3,>>,%a2@\+,%d1,%acc1
    6b60:	a29a be16      	macl %d6,%a3,>>,%a2@\+,%d1,%acc2
    6b64:	a65a be06      	macl %d6,%a3,>>,%a2@\+,%a3,%acc1
    6b68:	a6da be16      	macl %d6,%a3,>>,%a2@\+,%a3,%acc2
    6b6c:	a41a be06      	macl %d6,%a3,>>,%a2@\+,%d2,%acc1
    6b70:	a49a be16      	macl %d6,%a3,>>,%a2@\+,%d2,%acc2
    6b74:	ae5a be06      	macl %d6,%a3,>>,%a2@\+,%sp,%acc1
    6b78:	aeda be16      	macl %d6,%a3,>>,%a2@\+,%sp,%acc2
    6b7c:	a21a be26      	macl %d6,%a3,>>,%a2@\+&,%d1,%acc1
    6b80:	a29a be36      	macl %d6,%a3,>>,%a2@\+&,%d1,%acc2
    6b84:	a65a be26      	macl %d6,%a3,>>,%a2@\+&,%a3,%acc1
    6b88:	a6da be36      	macl %d6,%a3,>>,%a2@\+&,%a3,%acc2
    6b8c:	a41a be26      	macl %d6,%a3,>>,%a2@\+&,%d2,%acc1
    6b90:	a49a be36      	macl %d6,%a3,>>,%a2@\+&,%d2,%acc2
    6b94:	ae5a be26      	macl %d6,%a3,>>,%a2@\+&,%sp,%acc1
    6b98:	aeda be36      	macl %d6,%a3,>>,%a2@\+&,%sp,%acc2
    6b9c:	a22e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1,%acc1
    6ba2:	a2ae be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1,%acc2
    6ba8:	a66e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3,%acc1
    6bae:	a6ee be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3,%acc2
    6bb4:	a42e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2,%acc1
    6bba:	a4ae be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2,%acc2
    6bc0:	ae6e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp,%acc1
    6bc6:	aeee be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp,%acc2
    6bcc:	a22e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1,%acc1
    6bd2:	a2ae be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1,%acc2
    6bd8:	a66e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3,%acc1
    6bde:	a6ee be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3,%acc2
    6be4:	a42e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2,%acc1
    6bea:	a4ae be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2,%acc2
    6bf0:	ae6e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp,%acc1
    6bf6:	aeee be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp,%acc2
    6bfc:	a221 be06      	macl %d6,%a3,>>,%a1@-,%d1,%acc1
    6c00:	a2a1 be16      	macl %d6,%a3,>>,%a1@-,%d1,%acc2
    6c04:	a661 be06      	macl %d6,%a3,>>,%a1@-,%a3,%acc1
    6c08:	a6e1 be16      	macl %d6,%a3,>>,%a1@-,%a3,%acc2
    6c0c:	a421 be06      	macl %d6,%a3,>>,%a1@-,%d2,%acc1
    6c10:	a4a1 be16      	macl %d6,%a3,>>,%a1@-,%d2,%acc2
    6c14:	ae61 be06      	macl %d6,%a3,>>,%a1@-,%sp,%acc1
    6c18:	aee1 be16      	macl %d6,%a3,>>,%a1@-,%sp,%acc2
    6c1c:	a221 be26      	macl %d6,%a3,>>,%a1@-&,%d1,%acc1
    6c20:	a2a1 be36      	macl %d6,%a3,>>,%a1@-&,%d1,%acc2
    6c24:	a661 be26      	macl %d6,%a3,>>,%a1@-&,%a3,%acc1
    6c28:	a6e1 be36      	macl %d6,%a3,>>,%a1@-&,%a3,%acc2
    6c2c:	a421 be26      	macl %d6,%a3,>>,%a1@-&,%d2,%acc1
    6c30:	a4a1 be36      	macl %d6,%a3,>>,%a1@-&,%d2,%acc2
    6c34:	ae61 be26      	macl %d6,%a3,>>,%a1@-&,%sp,%acc1
    6c38:	aee1 be36      	macl %d6,%a3,>>,%a1@-&,%sp,%acc2
    6c3c:	a213 ba06      	macl %d6,%a3,<<,%a3@,%d1,%acc1
    6c40:	a293 ba16      	macl %d6,%a3,<<,%a3@,%d1,%acc2
    6c44:	a653 ba06      	macl %d6,%a3,<<,%a3@,%a3,%acc1
    6c48:	a6d3 ba16      	macl %d6,%a3,<<,%a3@,%a3,%acc2
    6c4c:	a413 ba06      	macl %d6,%a3,<<,%a3@,%d2,%acc1
    6c50:	a493 ba16      	macl %d6,%a3,<<,%a3@,%d2,%acc2
    6c54:	ae53 ba06      	macl %d6,%a3,<<,%a3@,%sp,%acc1
    6c58:	aed3 ba16      	macl %d6,%a3,<<,%a3@,%sp,%acc2
    6c5c:	a213 ba26      	macl %d6,%a3,<<,%a3@&,%d1,%acc1
    6c60:	a293 ba36      	macl %d6,%a3,<<,%a3@&,%d1,%acc2
    6c64:	a653 ba26      	macl %d6,%a3,<<,%a3@&,%a3,%acc1
    6c68:	a6d3 ba36      	macl %d6,%a3,<<,%a3@&,%a3,%acc2
    6c6c:	a413 ba26      	macl %d6,%a3,<<,%a3@&,%d2,%acc1
    6c70:	a493 ba36      	macl %d6,%a3,<<,%a3@&,%d2,%acc2
    6c74:	ae53 ba26      	macl %d6,%a3,<<,%a3@&,%sp,%acc1
    6c78:	aed3 ba36      	macl %d6,%a3,<<,%a3@&,%sp,%acc2
    6c7c:	a21a ba06      	macl %d6,%a3,<<,%a2@\+,%d1,%acc1
    6c80:	a29a ba16      	macl %d6,%a3,<<,%a2@\+,%d1,%acc2
    6c84:	a65a ba06      	macl %d6,%a3,<<,%a2@\+,%a3,%acc1
    6c88:	a6da ba16      	macl %d6,%a3,<<,%a2@\+,%a3,%acc2
    6c8c:	a41a ba06      	macl %d6,%a3,<<,%a2@\+,%d2,%acc1
    6c90:	a49a ba16      	macl %d6,%a3,<<,%a2@\+,%d2,%acc2
    6c94:	ae5a ba06      	macl %d6,%a3,<<,%a2@\+,%sp,%acc1
    6c98:	aeda ba16      	macl %d6,%a3,<<,%a2@\+,%sp,%acc2
    6c9c:	a21a ba26      	macl %d6,%a3,<<,%a2@\+&,%d1,%acc1
    6ca0:	a29a ba36      	macl %d6,%a3,<<,%a2@\+&,%d1,%acc2
    6ca4:	a65a ba26      	macl %d6,%a3,<<,%a2@\+&,%a3,%acc1
    6ca8:	a6da ba36      	macl %d6,%a3,<<,%a2@\+&,%a3,%acc2
    6cac:	a41a ba26      	macl %d6,%a3,<<,%a2@\+&,%d2,%acc1
    6cb0:	a49a ba36      	macl %d6,%a3,<<,%a2@\+&,%d2,%acc2
    6cb4:	ae5a ba26      	macl %d6,%a3,<<,%a2@\+&,%sp,%acc1
    6cb8:	aeda ba36      	macl %d6,%a3,<<,%a2@\+&,%sp,%acc2
    6cbc:	a22e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1,%acc1
    6cc2:	a2ae ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1,%acc2
    6cc8:	a66e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3,%acc1
    6cce:	a6ee ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3,%acc2
    6cd4:	a42e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2,%acc1
    6cda:	a4ae ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2,%acc2
    6ce0:	ae6e ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp,%acc1
    6ce6:	aeee ba16 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp,%acc2
    6cec:	a22e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1,%acc1
    6cf2:	a2ae ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1,%acc2
    6cf8:	a66e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3,%acc1
    6cfe:	a6ee ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3,%acc2
    6d04:	a42e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2,%acc1
    6d0a:	a4ae ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2,%acc2
    6d10:	ae6e ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp,%acc1
    6d16:	aeee ba36 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp,%acc2
    6d1c:	a221 ba06      	macl %d6,%a3,<<,%a1@-,%d1,%acc1
    6d20:	a2a1 ba16      	macl %d6,%a3,<<,%a1@-,%d1,%acc2
    6d24:	a661 ba06      	macl %d6,%a3,<<,%a1@-,%a3,%acc1
    6d28:	a6e1 ba16      	macl %d6,%a3,<<,%a1@-,%a3,%acc2
    6d2c:	a421 ba06      	macl %d6,%a3,<<,%a1@-,%d2,%acc1
    6d30:	a4a1 ba16      	macl %d6,%a3,<<,%a1@-,%d2,%acc2
    6d34:	ae61 ba06      	macl %d6,%a3,<<,%a1@-,%sp,%acc1
    6d38:	aee1 ba16      	macl %d6,%a3,<<,%a1@-,%sp,%acc2
    6d3c:	a221 ba26      	macl %d6,%a3,<<,%a1@-&,%d1,%acc1
    6d40:	a2a1 ba36      	macl %d6,%a3,<<,%a1@-&,%d1,%acc2
    6d44:	a661 ba26      	macl %d6,%a3,<<,%a1@-&,%a3,%acc1
    6d48:	a6e1 ba36      	macl %d6,%a3,<<,%a1@-&,%a3,%acc2
    6d4c:	a421 ba26      	macl %d6,%a3,<<,%a1@-&,%d2,%acc1
    6d50:	a4a1 ba36      	macl %d6,%a3,<<,%a1@-&,%d2,%acc2
    6d54:	ae61 ba26      	macl %d6,%a3,<<,%a1@-&,%sp,%acc1
    6d58:	aee1 ba36      	macl %d6,%a3,<<,%a1@-&,%sp,%acc2
    6d5c:	a213 be06      	macl %d6,%a3,>>,%a3@,%d1,%acc1
    6d60:	a293 be16      	macl %d6,%a3,>>,%a3@,%d1,%acc2
    6d64:	a653 be06      	macl %d6,%a3,>>,%a3@,%a3,%acc1
    6d68:	a6d3 be16      	macl %d6,%a3,>>,%a3@,%a3,%acc2
    6d6c:	a413 be06      	macl %d6,%a3,>>,%a3@,%d2,%acc1
    6d70:	a493 be16      	macl %d6,%a3,>>,%a3@,%d2,%acc2
    6d74:	ae53 be06      	macl %d6,%a3,>>,%a3@,%sp,%acc1
    6d78:	aed3 be16      	macl %d6,%a3,>>,%a3@,%sp,%acc2
    6d7c:	a213 be26      	macl %d6,%a3,>>,%a3@&,%d1,%acc1
    6d80:	a293 be36      	macl %d6,%a3,>>,%a3@&,%d1,%acc2
    6d84:	a653 be26      	macl %d6,%a3,>>,%a3@&,%a3,%acc1
    6d88:	a6d3 be36      	macl %d6,%a3,>>,%a3@&,%a3,%acc2
    6d8c:	a413 be26      	macl %d6,%a3,>>,%a3@&,%d2,%acc1
    6d90:	a493 be36      	macl %d6,%a3,>>,%a3@&,%d2,%acc2
    6d94:	ae53 be26      	macl %d6,%a3,>>,%a3@&,%sp,%acc1
    6d98:	aed3 be36      	macl %d6,%a3,>>,%a3@&,%sp,%acc2
    6d9c:	a21a be06      	macl %d6,%a3,>>,%a2@\+,%d1,%acc1
    6da0:	a29a be16      	macl %d6,%a3,>>,%a2@\+,%d1,%acc2
    6da4:	a65a be06      	macl %d6,%a3,>>,%a2@\+,%a3,%acc1
    6da8:	a6da be16      	macl %d6,%a3,>>,%a2@\+,%a3,%acc2
    6dac:	a41a be06      	macl %d6,%a3,>>,%a2@\+,%d2,%acc1
    6db0:	a49a be16      	macl %d6,%a3,>>,%a2@\+,%d2,%acc2
    6db4:	ae5a be06      	macl %d6,%a3,>>,%a2@\+,%sp,%acc1
    6db8:	aeda be16      	macl %d6,%a3,>>,%a2@\+,%sp,%acc2
    6dbc:	a21a be26      	macl %d6,%a3,>>,%a2@\+&,%d1,%acc1
    6dc0:	a29a be36      	macl %d6,%a3,>>,%a2@\+&,%d1,%acc2
    6dc4:	a65a be26      	macl %d6,%a3,>>,%a2@\+&,%a3,%acc1
    6dc8:	a6da be36      	macl %d6,%a3,>>,%a2@\+&,%a3,%acc2
    6dcc:	a41a be26      	macl %d6,%a3,>>,%a2@\+&,%d2,%acc1
    6dd0:	a49a be36      	macl %d6,%a3,>>,%a2@\+&,%d2,%acc2
    6dd4:	ae5a be26      	macl %d6,%a3,>>,%a2@\+&,%sp,%acc1
    6dd8:	aeda be36      	macl %d6,%a3,>>,%a2@\+&,%sp,%acc2
    6ddc:	a22e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1,%acc1
    6de2:	a2ae be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1,%acc2
    6de8:	a66e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3,%acc1
    6dee:	a6ee be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3,%acc2
    6df4:	a42e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2,%acc1
    6dfa:	a4ae be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2,%acc2
    6e00:	ae6e be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp,%acc1
    6e06:	aeee be16 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp,%acc2
    6e0c:	a22e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1,%acc1
    6e12:	a2ae be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1,%acc2
    6e18:	a66e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3,%acc1
    6e1e:	a6ee be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3,%acc2
    6e24:	a42e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2,%acc1
    6e2a:	a4ae be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2,%acc2
    6e30:	ae6e be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp,%acc1
    6e36:	aeee be36 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp,%acc2
    6e3c:	a221 be06      	macl %d6,%a3,>>,%a1@-,%d1,%acc1
    6e40:	a2a1 be16      	macl %d6,%a3,>>,%a1@-,%d1,%acc2
    6e44:	a661 be06      	macl %d6,%a3,>>,%a1@-,%a3,%acc1
    6e48:	a6e1 be16      	macl %d6,%a3,>>,%a1@-,%a3,%acc2
    6e4c:	a421 be06      	macl %d6,%a3,>>,%a1@-,%d2,%acc1
    6e50:	a4a1 be16      	macl %d6,%a3,>>,%a1@-,%d2,%acc2
    6e54:	ae61 be06      	macl %d6,%a3,>>,%a1@-,%sp,%acc1
    6e58:	aee1 be16      	macl %d6,%a3,>>,%a1@-,%sp,%acc2
    6e5c:	a221 be26      	macl %d6,%a3,>>,%a1@-&,%d1,%acc1
    6e60:	a2a1 be36      	macl %d6,%a3,>>,%a1@-&,%d1,%acc2
    6e64:	a661 be26      	macl %d6,%a3,>>,%a1@-&,%a3,%acc1
    6e68:	a6e1 be36      	macl %d6,%a3,>>,%a1@-&,%a3,%acc2
    6e6c:	a421 be26      	macl %d6,%a3,>>,%a1@-&,%d2,%acc1
    6e70:	a4a1 be36      	macl %d6,%a3,>>,%a1@-&,%d2,%acc2
    6e74:	ae61 be26      	macl %d6,%a3,>>,%a1@-&,%sp,%acc1
    6e78:	aee1 be36      	macl %d6,%a3,>>,%a1@-&,%sp,%acc2
    6e7c:	a213 4806      	macl %d6,%d4,%a3@,%d1,%acc1
    6e80:	a293 4816      	macl %d6,%d4,%a3@,%d1,%acc2
    6e84:	a653 4806      	macl %d6,%d4,%a3@,%a3,%acc1
    6e88:	a6d3 4816      	macl %d6,%d4,%a3@,%a3,%acc2
    6e8c:	a413 4806      	macl %d6,%d4,%a3@,%d2,%acc1
    6e90:	a493 4816      	macl %d6,%d4,%a3@,%d2,%acc2
    6e94:	ae53 4806      	macl %d6,%d4,%a3@,%sp,%acc1
    6e98:	aed3 4816      	macl %d6,%d4,%a3@,%sp,%acc2
    6e9c:	a213 4826      	macl %d6,%d4,%a3@&,%d1,%acc1
    6ea0:	a293 4836      	macl %d6,%d4,%a3@&,%d1,%acc2
    6ea4:	a653 4826      	macl %d6,%d4,%a3@&,%a3,%acc1
    6ea8:	a6d3 4836      	macl %d6,%d4,%a3@&,%a3,%acc2
    6eac:	a413 4826      	macl %d6,%d4,%a3@&,%d2,%acc1
    6eb0:	a493 4836      	macl %d6,%d4,%a3@&,%d2,%acc2
    6eb4:	ae53 4826      	macl %d6,%d4,%a3@&,%sp,%acc1
    6eb8:	aed3 4836      	macl %d6,%d4,%a3@&,%sp,%acc2
    6ebc:	a21a 4806      	macl %d6,%d4,%a2@\+,%d1,%acc1
    6ec0:	a29a 4816      	macl %d6,%d4,%a2@\+,%d1,%acc2
    6ec4:	a65a 4806      	macl %d6,%d4,%a2@\+,%a3,%acc1
    6ec8:	a6da 4816      	macl %d6,%d4,%a2@\+,%a3,%acc2
    6ecc:	a41a 4806      	macl %d6,%d4,%a2@\+,%d2,%acc1
    6ed0:	a49a 4816      	macl %d6,%d4,%a2@\+,%d2,%acc2
    6ed4:	ae5a 4806      	macl %d6,%d4,%a2@\+,%sp,%acc1
    6ed8:	aeda 4816      	macl %d6,%d4,%a2@\+,%sp,%acc2
    6edc:	a21a 4826      	macl %d6,%d4,%a2@\+&,%d1,%acc1
    6ee0:	a29a 4836      	macl %d6,%d4,%a2@\+&,%d1,%acc2
    6ee4:	a65a 4826      	macl %d6,%d4,%a2@\+&,%a3,%acc1
    6ee8:	a6da 4836      	macl %d6,%d4,%a2@\+&,%a3,%acc2
    6eec:	a41a 4826      	macl %d6,%d4,%a2@\+&,%d2,%acc1
    6ef0:	a49a 4836      	macl %d6,%d4,%a2@\+&,%d2,%acc2
    6ef4:	ae5a 4826      	macl %d6,%d4,%a2@\+&,%sp,%acc1
    6ef8:	aeda 4836      	macl %d6,%d4,%a2@\+&,%sp,%acc2
    6efc:	a22e 4806 000a 	macl %d6,%d4,%fp@\(10\),%d1,%acc1
    6f02:	a2ae 4816 000a 	macl %d6,%d4,%fp@\(10\),%d1,%acc2
    6f08:	a66e 4806 000a 	macl %d6,%d4,%fp@\(10\),%a3,%acc1
    6f0e:	a6ee 4816 000a 	macl %d6,%d4,%fp@\(10\),%a3,%acc2
    6f14:	a42e 4806 000a 	macl %d6,%d4,%fp@\(10\),%d2,%acc1
    6f1a:	a4ae 4816 000a 	macl %d6,%d4,%fp@\(10\),%d2,%acc2
    6f20:	ae6e 4806 000a 	macl %d6,%d4,%fp@\(10\),%sp,%acc1
    6f26:	aeee 4816 000a 	macl %d6,%d4,%fp@\(10\),%sp,%acc2
    6f2c:	a22e 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%d1,%acc1
    6f32:	a2ae 4836 000a 	macl %d6,%d4,%fp@\(10\)&,%d1,%acc2
    6f38:	a66e 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%a3,%acc1
    6f3e:	a6ee 4836 000a 	macl %d6,%d4,%fp@\(10\)&,%a3,%acc2
    6f44:	a42e 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%d2,%acc1
    6f4a:	a4ae 4836 000a 	macl %d6,%d4,%fp@\(10\)&,%d2,%acc2
    6f50:	ae6e 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%sp,%acc1
    6f56:	aeee 4836 000a 	macl %d6,%d4,%fp@\(10\)&,%sp,%acc2
    6f5c:	a221 4806      	macl %d6,%d4,%a1@-,%d1,%acc1
    6f60:	a2a1 4816      	macl %d6,%d4,%a1@-,%d1,%acc2
    6f64:	a661 4806      	macl %d6,%d4,%a1@-,%a3,%acc1
    6f68:	a6e1 4816      	macl %d6,%d4,%a1@-,%a3,%acc2
    6f6c:	a421 4806      	macl %d6,%d4,%a1@-,%d2,%acc1
    6f70:	a4a1 4816      	macl %d6,%d4,%a1@-,%d2,%acc2
    6f74:	ae61 4806      	macl %d6,%d4,%a1@-,%sp,%acc1
    6f78:	aee1 4816      	macl %d6,%d4,%a1@-,%sp,%acc2
    6f7c:	a221 4826      	macl %d6,%d4,%a1@-&,%d1,%acc1
    6f80:	a2a1 4836      	macl %d6,%d4,%a1@-&,%d1,%acc2
    6f84:	a661 4826      	macl %d6,%d4,%a1@-&,%a3,%acc1
    6f88:	a6e1 4836      	macl %d6,%d4,%a1@-&,%a3,%acc2
    6f8c:	a421 4826      	macl %d6,%d4,%a1@-&,%d2,%acc1
    6f90:	a4a1 4836      	macl %d6,%d4,%a1@-&,%d2,%acc2
    6f94:	ae61 4826      	macl %d6,%d4,%a1@-&,%sp,%acc1
    6f98:	aee1 4836      	macl %d6,%d4,%a1@-&,%sp,%acc2
    6f9c:	a213 4a06      	macl %d6,%d4,<<,%a3@,%d1,%acc1
    6fa0:	a293 4a16      	macl %d6,%d4,<<,%a3@,%d1,%acc2
    6fa4:	a653 4a06      	macl %d6,%d4,<<,%a3@,%a3,%acc1
    6fa8:	a6d3 4a16      	macl %d6,%d4,<<,%a3@,%a3,%acc2
    6fac:	a413 4a06      	macl %d6,%d4,<<,%a3@,%d2,%acc1
    6fb0:	a493 4a16      	macl %d6,%d4,<<,%a3@,%d2,%acc2
    6fb4:	ae53 4a06      	macl %d6,%d4,<<,%a3@,%sp,%acc1
    6fb8:	aed3 4a16      	macl %d6,%d4,<<,%a3@,%sp,%acc2
    6fbc:	a213 4a26      	macl %d6,%d4,<<,%a3@&,%d1,%acc1
    6fc0:	a293 4a36      	macl %d6,%d4,<<,%a3@&,%d1,%acc2
    6fc4:	a653 4a26      	macl %d6,%d4,<<,%a3@&,%a3,%acc1
    6fc8:	a6d3 4a36      	macl %d6,%d4,<<,%a3@&,%a3,%acc2
    6fcc:	a413 4a26      	macl %d6,%d4,<<,%a3@&,%d2,%acc1
    6fd0:	a493 4a36      	macl %d6,%d4,<<,%a3@&,%d2,%acc2
    6fd4:	ae53 4a26      	macl %d6,%d4,<<,%a3@&,%sp,%acc1
    6fd8:	aed3 4a36      	macl %d6,%d4,<<,%a3@&,%sp,%acc2
    6fdc:	a21a 4a06      	macl %d6,%d4,<<,%a2@\+,%d1,%acc1
    6fe0:	a29a 4a16      	macl %d6,%d4,<<,%a2@\+,%d1,%acc2
    6fe4:	a65a 4a06      	macl %d6,%d4,<<,%a2@\+,%a3,%acc1
    6fe8:	a6da 4a16      	macl %d6,%d4,<<,%a2@\+,%a3,%acc2
    6fec:	a41a 4a06      	macl %d6,%d4,<<,%a2@\+,%d2,%acc1
    6ff0:	a49a 4a16      	macl %d6,%d4,<<,%a2@\+,%d2,%acc2
    6ff4:	ae5a 4a06      	macl %d6,%d4,<<,%a2@\+,%sp,%acc1
    6ff8:	aeda 4a16      	macl %d6,%d4,<<,%a2@\+,%sp,%acc2
    6ffc:	a21a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d1,%acc1
    7000:	a29a 4a36      	macl %d6,%d4,<<,%a2@\+&,%d1,%acc2
    7004:	a65a 4a26      	macl %d6,%d4,<<,%a2@\+&,%a3,%acc1
    7008:	a6da 4a36      	macl %d6,%d4,<<,%a2@\+&,%a3,%acc2
    700c:	a41a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d2,%acc1
    7010:	a49a 4a36      	macl %d6,%d4,<<,%a2@\+&,%d2,%acc2
    7014:	ae5a 4a26      	macl %d6,%d4,<<,%a2@\+&,%sp,%acc1
    7018:	aeda 4a36      	macl %d6,%d4,<<,%a2@\+&,%sp,%acc2
    701c:	a22e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1,%acc1
    7022:	a2ae 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1,%acc2
    7028:	a66e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3,%acc1
    702e:	a6ee 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3,%acc2
    7034:	a42e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2,%acc1
    703a:	a4ae 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2,%acc2
    7040:	ae6e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp,%acc1
    7046:	aeee 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp,%acc2
    704c:	a22e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1,%acc1
    7052:	a2ae 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1,%acc2
    7058:	a66e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3,%acc1
    705e:	a6ee 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3,%acc2
    7064:	a42e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2,%acc1
    706a:	a4ae 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2,%acc2
    7070:	ae6e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp,%acc1
    7076:	aeee 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp,%acc2
    707c:	a221 4a06      	macl %d6,%d4,<<,%a1@-,%d1,%acc1
    7080:	a2a1 4a16      	macl %d6,%d4,<<,%a1@-,%d1,%acc2
    7084:	a661 4a06      	macl %d6,%d4,<<,%a1@-,%a3,%acc1
    7088:	a6e1 4a16      	macl %d6,%d4,<<,%a1@-,%a3,%acc2
    708c:	a421 4a06      	macl %d6,%d4,<<,%a1@-,%d2,%acc1
    7090:	a4a1 4a16      	macl %d6,%d4,<<,%a1@-,%d2,%acc2
    7094:	ae61 4a06      	macl %d6,%d4,<<,%a1@-,%sp,%acc1
    7098:	aee1 4a16      	macl %d6,%d4,<<,%a1@-,%sp,%acc2
    709c:	a221 4a26      	macl %d6,%d4,<<,%a1@-&,%d1,%acc1
    70a0:	a2a1 4a36      	macl %d6,%d4,<<,%a1@-&,%d1,%acc2
    70a4:	a661 4a26      	macl %d6,%d4,<<,%a1@-&,%a3,%acc1
    70a8:	a6e1 4a36      	macl %d6,%d4,<<,%a1@-&,%a3,%acc2
    70ac:	a421 4a26      	macl %d6,%d4,<<,%a1@-&,%d2,%acc1
    70b0:	a4a1 4a36      	macl %d6,%d4,<<,%a1@-&,%d2,%acc2
    70b4:	ae61 4a26      	macl %d6,%d4,<<,%a1@-&,%sp,%acc1
    70b8:	aee1 4a36      	macl %d6,%d4,<<,%a1@-&,%sp,%acc2
    70bc:	a213 4e06      	macl %d6,%d4,>>,%a3@,%d1,%acc1
    70c0:	a293 4e16      	macl %d6,%d4,>>,%a3@,%d1,%acc2
    70c4:	a653 4e06      	macl %d6,%d4,>>,%a3@,%a3,%acc1
    70c8:	a6d3 4e16      	macl %d6,%d4,>>,%a3@,%a3,%acc2
    70cc:	a413 4e06      	macl %d6,%d4,>>,%a3@,%d2,%acc1
    70d0:	a493 4e16      	macl %d6,%d4,>>,%a3@,%d2,%acc2
    70d4:	ae53 4e06      	macl %d6,%d4,>>,%a3@,%sp,%acc1
    70d8:	aed3 4e16      	macl %d6,%d4,>>,%a3@,%sp,%acc2
    70dc:	a213 4e26      	macl %d6,%d4,>>,%a3@&,%d1,%acc1
    70e0:	a293 4e36      	macl %d6,%d4,>>,%a3@&,%d1,%acc2
    70e4:	a653 4e26      	macl %d6,%d4,>>,%a3@&,%a3,%acc1
    70e8:	a6d3 4e36      	macl %d6,%d4,>>,%a3@&,%a3,%acc2
    70ec:	a413 4e26      	macl %d6,%d4,>>,%a3@&,%d2,%acc1
    70f0:	a493 4e36      	macl %d6,%d4,>>,%a3@&,%d2,%acc2
    70f4:	ae53 4e26      	macl %d6,%d4,>>,%a3@&,%sp,%acc1
    70f8:	aed3 4e36      	macl %d6,%d4,>>,%a3@&,%sp,%acc2
    70fc:	a21a 4e06      	macl %d6,%d4,>>,%a2@\+,%d1,%acc1
    7100:	a29a 4e16      	macl %d6,%d4,>>,%a2@\+,%d1,%acc2
    7104:	a65a 4e06      	macl %d6,%d4,>>,%a2@\+,%a3,%acc1
    7108:	a6da 4e16      	macl %d6,%d4,>>,%a2@\+,%a3,%acc2
    710c:	a41a 4e06      	macl %d6,%d4,>>,%a2@\+,%d2,%acc1
    7110:	a49a 4e16      	macl %d6,%d4,>>,%a2@\+,%d2,%acc2
    7114:	ae5a 4e06      	macl %d6,%d4,>>,%a2@\+,%sp,%acc1
    7118:	aeda 4e16      	macl %d6,%d4,>>,%a2@\+,%sp,%acc2
    711c:	a21a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d1,%acc1
    7120:	a29a 4e36      	macl %d6,%d4,>>,%a2@\+&,%d1,%acc2
    7124:	a65a 4e26      	macl %d6,%d4,>>,%a2@\+&,%a3,%acc1
    7128:	a6da 4e36      	macl %d6,%d4,>>,%a2@\+&,%a3,%acc2
    712c:	a41a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d2,%acc1
    7130:	a49a 4e36      	macl %d6,%d4,>>,%a2@\+&,%d2,%acc2
    7134:	ae5a 4e26      	macl %d6,%d4,>>,%a2@\+&,%sp,%acc1
    7138:	aeda 4e36      	macl %d6,%d4,>>,%a2@\+&,%sp,%acc2
    713c:	a22e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1,%acc1
    7142:	a2ae 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1,%acc2
    7148:	a66e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3,%acc1
    714e:	a6ee 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3,%acc2
    7154:	a42e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2,%acc1
    715a:	a4ae 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2,%acc2
    7160:	ae6e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp,%acc1
    7166:	aeee 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp,%acc2
    716c:	a22e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1,%acc1
    7172:	a2ae 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1,%acc2
    7178:	a66e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3,%acc1
    717e:	a6ee 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3,%acc2
    7184:	a42e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2,%acc1
    718a:	a4ae 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2,%acc2
    7190:	ae6e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp,%acc1
    7196:	aeee 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp,%acc2
    719c:	a221 4e06      	macl %d6,%d4,>>,%a1@-,%d1,%acc1
    71a0:	a2a1 4e16      	macl %d6,%d4,>>,%a1@-,%d1,%acc2
    71a4:	a661 4e06      	macl %d6,%d4,>>,%a1@-,%a3,%acc1
    71a8:	a6e1 4e16      	macl %d6,%d4,>>,%a1@-,%a3,%acc2
    71ac:	a421 4e06      	macl %d6,%d4,>>,%a1@-,%d2,%acc1
    71b0:	a4a1 4e16      	macl %d6,%d4,>>,%a1@-,%d2,%acc2
    71b4:	ae61 4e06      	macl %d6,%d4,>>,%a1@-,%sp,%acc1
    71b8:	aee1 4e16      	macl %d6,%d4,>>,%a1@-,%sp,%acc2
    71bc:	a221 4e26      	macl %d6,%d4,>>,%a1@-&,%d1,%acc1
    71c0:	a2a1 4e36      	macl %d6,%d4,>>,%a1@-&,%d1,%acc2
    71c4:	a661 4e26      	macl %d6,%d4,>>,%a1@-&,%a3,%acc1
    71c8:	a6e1 4e36      	macl %d6,%d4,>>,%a1@-&,%a3,%acc2
    71cc:	a421 4e26      	macl %d6,%d4,>>,%a1@-&,%d2,%acc1
    71d0:	a4a1 4e36      	macl %d6,%d4,>>,%a1@-&,%d2,%acc2
    71d4:	ae61 4e26      	macl %d6,%d4,>>,%a1@-&,%sp,%acc1
    71d8:	aee1 4e36      	macl %d6,%d4,>>,%a1@-&,%sp,%acc2
    71dc:	a213 4a06      	macl %d6,%d4,<<,%a3@,%d1,%acc1
    71e0:	a293 4a16      	macl %d6,%d4,<<,%a3@,%d1,%acc2
    71e4:	a653 4a06      	macl %d6,%d4,<<,%a3@,%a3,%acc1
    71e8:	a6d3 4a16      	macl %d6,%d4,<<,%a3@,%a3,%acc2
    71ec:	a413 4a06      	macl %d6,%d4,<<,%a3@,%d2,%acc1
    71f0:	a493 4a16      	macl %d6,%d4,<<,%a3@,%d2,%acc2
    71f4:	ae53 4a06      	macl %d6,%d4,<<,%a3@,%sp,%acc1
    71f8:	aed3 4a16      	macl %d6,%d4,<<,%a3@,%sp,%acc2
    71fc:	a213 4a26      	macl %d6,%d4,<<,%a3@&,%d1,%acc1
    7200:	a293 4a36      	macl %d6,%d4,<<,%a3@&,%d1,%acc2
    7204:	a653 4a26      	macl %d6,%d4,<<,%a3@&,%a3,%acc1
    7208:	a6d3 4a36      	macl %d6,%d4,<<,%a3@&,%a3,%acc2
    720c:	a413 4a26      	macl %d6,%d4,<<,%a3@&,%d2,%acc1
    7210:	a493 4a36      	macl %d6,%d4,<<,%a3@&,%d2,%acc2
    7214:	ae53 4a26      	macl %d6,%d4,<<,%a3@&,%sp,%acc1
    7218:	aed3 4a36      	macl %d6,%d4,<<,%a3@&,%sp,%acc2
    721c:	a21a 4a06      	macl %d6,%d4,<<,%a2@\+,%d1,%acc1
    7220:	a29a 4a16      	macl %d6,%d4,<<,%a2@\+,%d1,%acc2
    7224:	a65a 4a06      	macl %d6,%d4,<<,%a2@\+,%a3,%acc1
    7228:	a6da 4a16      	macl %d6,%d4,<<,%a2@\+,%a3,%acc2
    722c:	a41a 4a06      	macl %d6,%d4,<<,%a2@\+,%d2,%acc1
    7230:	a49a 4a16      	macl %d6,%d4,<<,%a2@\+,%d2,%acc2
    7234:	ae5a 4a06      	macl %d6,%d4,<<,%a2@\+,%sp,%acc1
    7238:	aeda 4a16      	macl %d6,%d4,<<,%a2@\+,%sp,%acc2
    723c:	a21a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d1,%acc1
    7240:	a29a 4a36      	macl %d6,%d4,<<,%a2@\+&,%d1,%acc2
    7244:	a65a 4a26      	macl %d6,%d4,<<,%a2@\+&,%a3,%acc1
    7248:	a6da 4a36      	macl %d6,%d4,<<,%a2@\+&,%a3,%acc2
    724c:	a41a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d2,%acc1
    7250:	a49a 4a36      	macl %d6,%d4,<<,%a2@\+&,%d2,%acc2
    7254:	ae5a 4a26      	macl %d6,%d4,<<,%a2@\+&,%sp,%acc1
    7258:	aeda 4a36      	macl %d6,%d4,<<,%a2@\+&,%sp,%acc2
    725c:	a22e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1,%acc1
    7262:	a2ae 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1,%acc2
    7268:	a66e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3,%acc1
    726e:	a6ee 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3,%acc2
    7274:	a42e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2,%acc1
    727a:	a4ae 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2,%acc2
    7280:	ae6e 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp,%acc1
    7286:	aeee 4a16 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp,%acc2
    728c:	a22e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1,%acc1
    7292:	a2ae 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1,%acc2
    7298:	a66e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3,%acc1
    729e:	a6ee 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3,%acc2
    72a4:	a42e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2,%acc1
    72aa:	a4ae 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2,%acc2
    72b0:	ae6e 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp,%acc1
    72b6:	aeee 4a36 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp,%acc2
    72bc:	a221 4a06      	macl %d6,%d4,<<,%a1@-,%d1,%acc1
    72c0:	a2a1 4a16      	macl %d6,%d4,<<,%a1@-,%d1,%acc2
    72c4:	a661 4a06      	macl %d6,%d4,<<,%a1@-,%a3,%acc1
    72c8:	a6e1 4a16      	macl %d6,%d4,<<,%a1@-,%a3,%acc2
    72cc:	a421 4a06      	macl %d6,%d4,<<,%a1@-,%d2,%acc1
    72d0:	a4a1 4a16      	macl %d6,%d4,<<,%a1@-,%d2,%acc2
    72d4:	ae61 4a06      	macl %d6,%d4,<<,%a1@-,%sp,%acc1
    72d8:	aee1 4a16      	macl %d6,%d4,<<,%a1@-,%sp,%acc2
    72dc:	a221 4a26      	macl %d6,%d4,<<,%a1@-&,%d1,%acc1
    72e0:	a2a1 4a36      	macl %d6,%d4,<<,%a1@-&,%d1,%acc2
    72e4:	a661 4a26      	macl %d6,%d4,<<,%a1@-&,%a3,%acc1
    72e8:	a6e1 4a36      	macl %d6,%d4,<<,%a1@-&,%a3,%acc2
    72ec:	a421 4a26      	macl %d6,%d4,<<,%a1@-&,%d2,%acc1
    72f0:	a4a1 4a36      	macl %d6,%d4,<<,%a1@-&,%d2,%acc2
    72f4:	ae61 4a26      	macl %d6,%d4,<<,%a1@-&,%sp,%acc1
    72f8:	aee1 4a36      	macl %d6,%d4,<<,%a1@-&,%sp,%acc2
    72fc:	a213 4e06      	macl %d6,%d4,>>,%a3@,%d1,%acc1
    7300:	a293 4e16      	macl %d6,%d4,>>,%a3@,%d1,%acc2
    7304:	a653 4e06      	macl %d6,%d4,>>,%a3@,%a3,%acc1
    7308:	a6d3 4e16      	macl %d6,%d4,>>,%a3@,%a3,%acc2
    730c:	a413 4e06      	macl %d6,%d4,>>,%a3@,%d2,%acc1
    7310:	a493 4e16      	macl %d6,%d4,>>,%a3@,%d2,%acc2
    7314:	ae53 4e06      	macl %d6,%d4,>>,%a3@,%sp,%acc1
    7318:	aed3 4e16      	macl %d6,%d4,>>,%a3@,%sp,%acc2
    731c:	a213 4e26      	macl %d6,%d4,>>,%a3@&,%d1,%acc1
    7320:	a293 4e36      	macl %d6,%d4,>>,%a3@&,%d1,%acc2
    7324:	a653 4e26      	macl %d6,%d4,>>,%a3@&,%a3,%acc1
    7328:	a6d3 4e36      	macl %d6,%d4,>>,%a3@&,%a3,%acc2
    732c:	a413 4e26      	macl %d6,%d4,>>,%a3@&,%d2,%acc1
    7330:	a493 4e36      	macl %d6,%d4,>>,%a3@&,%d2,%acc2
    7334:	ae53 4e26      	macl %d6,%d4,>>,%a3@&,%sp,%acc1
    7338:	aed3 4e36      	macl %d6,%d4,>>,%a3@&,%sp,%acc2
    733c:	a21a 4e06      	macl %d6,%d4,>>,%a2@\+,%d1,%acc1
    7340:	a29a 4e16      	macl %d6,%d4,>>,%a2@\+,%d1,%acc2
    7344:	a65a 4e06      	macl %d6,%d4,>>,%a2@\+,%a3,%acc1
    7348:	a6da 4e16      	macl %d6,%d4,>>,%a2@\+,%a3,%acc2
    734c:	a41a 4e06      	macl %d6,%d4,>>,%a2@\+,%d2,%acc1
    7350:	a49a 4e16      	macl %d6,%d4,>>,%a2@\+,%d2,%acc2
    7354:	ae5a 4e06      	macl %d6,%d4,>>,%a2@\+,%sp,%acc1
    7358:	aeda 4e16      	macl %d6,%d4,>>,%a2@\+,%sp,%acc2
    735c:	a21a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d1,%acc1
    7360:	a29a 4e36      	macl %d6,%d4,>>,%a2@\+&,%d1,%acc2
    7364:	a65a 4e26      	macl %d6,%d4,>>,%a2@\+&,%a3,%acc1
    7368:	a6da 4e36      	macl %d6,%d4,>>,%a2@\+&,%a3,%acc2
    736c:	a41a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d2,%acc1
    7370:	a49a 4e36      	macl %d6,%d4,>>,%a2@\+&,%d2,%acc2
    7374:	ae5a 4e26      	macl %d6,%d4,>>,%a2@\+&,%sp,%acc1
    7378:	aeda 4e36      	macl %d6,%d4,>>,%a2@\+&,%sp,%acc2
    737c:	a22e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1,%acc1
    7382:	a2ae 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1,%acc2
    7388:	a66e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3,%acc1
    738e:	a6ee 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3,%acc2
    7394:	a42e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2,%acc1
    739a:	a4ae 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2,%acc2
    73a0:	ae6e 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp,%acc1
    73a6:	aeee 4e16 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp,%acc2
    73ac:	a22e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1,%acc1
    73b2:	a2ae 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1,%acc2
    73b8:	a66e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3,%acc1
    73be:	a6ee 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3,%acc2
    73c4:	a42e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2,%acc1
    73ca:	a4ae 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2,%acc2
    73d0:	ae6e 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp,%acc1
    73d6:	aeee 4e36 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp,%acc2
    73dc:	a221 4e06      	macl %d6,%d4,>>,%a1@-,%d1,%acc1
    73e0:	a2a1 4e16      	macl %d6,%d4,>>,%a1@-,%d1,%acc2
    73e4:	a661 4e06      	macl %d6,%d4,>>,%a1@-,%a3,%acc1
    73e8:	a6e1 4e16      	macl %d6,%d4,>>,%a1@-,%a3,%acc2
    73ec:	a421 4e06      	macl %d6,%d4,>>,%a1@-,%d2,%acc1
    73f0:	a4a1 4e16      	macl %d6,%d4,>>,%a1@-,%d2,%acc2
    73f4:	ae61 4e06      	macl %d6,%d4,>>,%a1@-,%sp,%acc1
    73f8:	aee1 4e16      	macl %d6,%d4,>>,%a1@-,%sp,%acc2
    73fc:	a221 4e26      	macl %d6,%d4,>>,%a1@-&,%d1,%acc1
    7400:	a2a1 4e36      	macl %d6,%d4,>>,%a1@-&,%d1,%acc2
    7404:	a661 4e26      	macl %d6,%d4,>>,%a1@-&,%a3,%acc1
    7408:	a6e1 4e36      	macl %d6,%d4,>>,%a1@-&,%a3,%acc2
    740c:	a421 4e26      	macl %d6,%d4,>>,%a1@-&,%d2,%acc1
    7410:	a4a1 4e36      	macl %d6,%d4,>>,%a1@-&,%d2,%acc2
    7414:	ae61 4e26      	macl %d6,%d4,>>,%a1@-&,%sp,%acc1
    7418:	aee1 4e36      	macl %d6,%d4,>>,%a1@-&,%sp,%acc2
