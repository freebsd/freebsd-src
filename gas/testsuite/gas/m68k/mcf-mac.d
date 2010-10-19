#name: mcf-mac
#objdump: -d --architecture=m68k:5407
#as: -m5407

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
       0:	a182           	movel %acc,%d2
       2:	a189           	movel %acc,%a1
       4:	a989           	movel %macsr,%a1
       6:	a982           	movel %macsr,%d2
       8:	ad89           	movel %mask,%a1
       a:	ad82           	movel %mask,%d2
       c:	a9c0           	movel %macsr,%ccr
       e:	a13c 1234 5678 	movel #305419896,%acc
      14:	a101           	movel %d1,%acc
      16:	a10a           	movel %a2,%acc
      18:	a93c 1234 5678 	movel #305419896,%macsr
      1e:	a901           	movel %d1,%macsr
      20:	a90a           	movel %a2,%macsr
      22:	ad3c 1234 5678 	movel #305419896,%mask
      28:	ad01           	movel %d1,%mask
      2a:	ad0a           	movel %a2,%mask
      2c:	a449 0080      	macw %a1l,%a2u
      30:	a449 0280      	macw %a1l,%a2u,<<
      34:	a449 0680      	macw %a1l,%a2u,>>
      38:	a449 0280      	macw %a1l,%a2u,<<
      3c:	a449 0680      	macw %a1l,%a2u,>>
      40:	a609 0000      	macw %a1l,%d3l
      44:	a609 0200      	macw %a1l,%d3l,<<
      48:	a609 0600      	macw %a1l,%d3l,>>
      4c:	a609 0200      	macw %a1l,%d3l,<<
      50:	a609 0600      	macw %a1l,%d3l,>>
      54:	ae49 0080      	macw %a1l,%a7u
      58:	ae49 0280      	macw %a1l,%a7u,<<
      5c:	ae49 0680      	macw %a1l,%a7u,>>
      60:	ae49 0280      	macw %a1l,%a7u,<<
      64:	ae49 0680      	macw %a1l,%a7u,>>
      68:	a209 0000      	macw %a1l,%d1l
      6c:	a209 0200      	macw %a1l,%d1l,<<
      70:	a209 0600      	macw %a1l,%d1l,>>
      74:	a209 0200      	macw %a1l,%d1l,<<
      78:	a209 0600      	macw %a1l,%d1l,>>
      7c:	a442 00c0      	macw %d2u,%a2u
      80:	a442 02c0      	macw %d2u,%a2u,<<
      84:	a442 06c0      	macw %d2u,%a2u,>>
      88:	a442 02c0      	macw %d2u,%a2u,<<
      8c:	a442 06c0      	macw %d2u,%a2u,>>
      90:	a602 0040      	macw %d2u,%d3l
      94:	a602 0240      	macw %d2u,%d3l,<<
      98:	a602 0640      	macw %d2u,%d3l,>>
      9c:	a602 0240      	macw %d2u,%d3l,<<
      a0:	a602 0640      	macw %d2u,%d3l,>>
      a4:	ae42 00c0      	macw %d2u,%a7u
      a8:	ae42 02c0      	macw %d2u,%a7u,<<
      ac:	ae42 06c0      	macw %d2u,%a7u,>>
      b0:	ae42 02c0      	macw %d2u,%a7u,<<
      b4:	ae42 06c0      	macw %d2u,%a7u,>>
      b8:	a202 0040      	macw %d2u,%d1l
      bc:	a202 0240      	macw %d2u,%d1l,<<
      c0:	a202 0640      	macw %d2u,%d1l,>>
      c4:	a202 0240      	macw %d2u,%d1l,<<
      c8:	a202 0640      	macw %d2u,%d1l,>>
      cc:	a44d 0080      	macw %a5l,%a2u
      d0:	a44d 0280      	macw %a5l,%a2u,<<
      d4:	a44d 0680      	macw %a5l,%a2u,>>
      d8:	a44d 0280      	macw %a5l,%a2u,<<
      dc:	a44d 0680      	macw %a5l,%a2u,>>
      e0:	a60d 0000      	macw %a5l,%d3l
      e4:	a60d 0200      	macw %a5l,%d3l,<<
      e8:	a60d 0600      	macw %a5l,%d3l,>>
      ec:	a60d 0200      	macw %a5l,%d3l,<<
      f0:	a60d 0600      	macw %a5l,%d3l,>>
      f4:	ae4d 0080      	macw %a5l,%a7u
      f8:	ae4d 0280      	macw %a5l,%a7u,<<
      fc:	ae4d 0680      	macw %a5l,%a7u,>>
     100:	ae4d 0280      	macw %a5l,%a7u,<<
     104:	ae4d 0680      	macw %a5l,%a7u,>>
     108:	a20d 0000      	macw %a5l,%d1l
     10c:	a20d 0200      	macw %a5l,%d1l,<<
     110:	a20d 0600      	macw %a5l,%d1l,>>
     114:	a20d 0200      	macw %a5l,%d1l,<<
     118:	a20d 0600      	macw %a5l,%d1l,>>
     11c:	a446 00c0      	macw %d6u,%a2u
     120:	a446 02c0      	macw %d6u,%a2u,<<
     124:	a446 06c0      	macw %d6u,%a2u,>>
     128:	a446 02c0      	macw %d6u,%a2u,<<
     12c:	a446 06c0      	macw %d6u,%a2u,>>
     130:	a606 0040      	macw %d6u,%d3l
     134:	a606 0240      	macw %d6u,%d3l,<<
     138:	a606 0640      	macw %d6u,%d3l,>>
     13c:	a606 0240      	macw %d6u,%d3l,<<
     140:	a606 0640      	macw %d6u,%d3l,>>
     144:	ae46 00c0      	macw %d6u,%a7u
     148:	ae46 02c0      	macw %d6u,%a7u,<<
     14c:	ae46 06c0      	macw %d6u,%a7u,>>
     150:	ae46 02c0      	macw %d6u,%a7u,<<
     154:	ae46 06c0      	macw %d6u,%a7u,>>
     158:	a206 0040      	macw %d6u,%d1l
     15c:	a206 0240      	macw %d6u,%d1l,<<
     160:	a206 0640      	macw %d6u,%d1l,>>
     164:	a206 0240      	macw %d6u,%d1l,<<
     168:	a206 0640      	macw %d6u,%d1l,>>
     16c:	a293 a089      	macw %a1l,%a2u,%a3@,%d1
     170:	a6d3 a089      	macw %a1l,%a2u,%a3@,%a3
     174:	a493 a089      	macw %a1l,%a2u,%a3@,%d2
     178:	aed3 a089      	macw %a1l,%a2u,%a3@,%sp
     17c:	a293 a0a9      	macw %a1l,%a2u,%a3@&,%d1
     180:	a6d3 a0a9      	macw %a1l,%a2u,%a3@&,%a3
     184:	a493 a0a9      	macw %a1l,%a2u,%a3@&,%d2
     188:	aed3 a0a9      	macw %a1l,%a2u,%a3@&,%sp
     18c:	a29a a089      	macw %a1l,%a2u,%a2@\+,%d1
     190:	a6da a089      	macw %a1l,%a2u,%a2@\+,%a3
     194:	a49a a089      	macw %a1l,%a2u,%a2@\+,%d2
     198:	aeda a089      	macw %a1l,%a2u,%a2@\+,%sp
     19c:	a29a a0a9      	macw %a1l,%a2u,%a2@\+&,%d1
     1a0:	a6da a0a9      	macw %a1l,%a2u,%a2@\+&,%a3
     1a4:	a49a a0a9      	macw %a1l,%a2u,%a2@\+&,%d2
     1a8:	aeda a0a9      	macw %a1l,%a2u,%a2@\+&,%sp
     1ac:	a2ae a089 000a 	macw %a1l,%a2u,%fp@\(10\),%d1
     1b2:	a6ee a089 000a 	macw %a1l,%a2u,%fp@\(10\),%a3
     1b8:	a4ae a089 000a 	macw %a1l,%a2u,%fp@\(10\),%d2
     1be:	aeee a089 000a 	macw %a1l,%a2u,%fp@\(10\),%sp
     1c4:	a2ae a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d1
     1ca:	a6ee a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%a3
     1d0:	a4ae a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%d2
     1d6:	aeee a0a9 000a 	macw %a1l,%a2u,%fp@\(10\)&,%sp
     1dc:	a2a1 a089      	macw %a1l,%a2u,%a1@-,%d1
     1e0:	a6e1 a089      	macw %a1l,%a2u,%a1@-,%a3
     1e4:	a4a1 a089      	macw %a1l,%a2u,%a1@-,%d2
     1e8:	aee1 a089      	macw %a1l,%a2u,%a1@-,%sp
     1ec:	a2a1 a0a9      	macw %a1l,%a2u,%a1@-&,%d1
     1f0:	a6e1 a0a9      	macw %a1l,%a2u,%a1@-&,%a3
     1f4:	a4a1 a0a9      	macw %a1l,%a2u,%a1@-&,%d2
     1f8:	aee1 a0a9      	macw %a1l,%a2u,%a1@-&,%sp
     1fc:	a293 a289      	macw %a1l,%a2u,<<,%a3@,%d1
     200:	a6d3 a289      	macw %a1l,%a2u,<<,%a3@,%a3
     204:	a493 a289      	macw %a1l,%a2u,<<,%a3@,%d2
     208:	aed3 a289      	macw %a1l,%a2u,<<,%a3@,%sp
     20c:	a293 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d1
     210:	a6d3 a2a9      	macw %a1l,%a2u,<<,%a3@&,%a3
     214:	a493 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d2
     218:	aed3 a2a9      	macw %a1l,%a2u,<<,%a3@&,%sp
     21c:	a29a a289      	macw %a1l,%a2u,<<,%a2@\+,%d1
     220:	a6da a289      	macw %a1l,%a2u,<<,%a2@\+,%a3
     224:	a49a a289      	macw %a1l,%a2u,<<,%a2@\+,%d2
     228:	aeda a289      	macw %a1l,%a2u,<<,%a2@\+,%sp
     22c:	a29a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d1
     230:	a6da a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%a3
     234:	a49a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d2
     238:	aeda a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%sp
     23c:	a2ae a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1
     242:	a6ee a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3
     248:	a4ae a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2
     24e:	aeee a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp
     254:	a2ae a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1
     25a:	a6ee a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3
     260:	a4ae a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2
     266:	aeee a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp
     26c:	a2a1 a289      	macw %a1l,%a2u,<<,%a1@-,%d1
     270:	a6e1 a289      	macw %a1l,%a2u,<<,%a1@-,%a3
     274:	a4a1 a289      	macw %a1l,%a2u,<<,%a1@-,%d2
     278:	aee1 a289      	macw %a1l,%a2u,<<,%a1@-,%sp
     27c:	a2a1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d1
     280:	a6e1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%a3
     284:	a4a1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d2
     288:	aee1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%sp
     28c:	a293 a689      	macw %a1l,%a2u,>>,%a3@,%d1
     290:	a6d3 a689      	macw %a1l,%a2u,>>,%a3@,%a3
     294:	a493 a689      	macw %a1l,%a2u,>>,%a3@,%d2
     298:	aed3 a689      	macw %a1l,%a2u,>>,%a3@,%sp
     29c:	a293 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d1
     2a0:	a6d3 a6a9      	macw %a1l,%a2u,>>,%a3@&,%a3
     2a4:	a493 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d2
     2a8:	aed3 a6a9      	macw %a1l,%a2u,>>,%a3@&,%sp
     2ac:	a29a a689      	macw %a1l,%a2u,>>,%a2@\+,%d1
     2b0:	a6da a689      	macw %a1l,%a2u,>>,%a2@\+,%a3
     2b4:	a49a a689      	macw %a1l,%a2u,>>,%a2@\+,%d2
     2b8:	aeda a689      	macw %a1l,%a2u,>>,%a2@\+,%sp
     2bc:	a29a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d1
     2c0:	a6da a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%a3
     2c4:	a49a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d2
     2c8:	aeda a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%sp
     2cc:	a2ae a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1
     2d2:	a6ee a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3
     2d8:	a4ae a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2
     2de:	aeee a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp
     2e4:	a2ae a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1
     2ea:	a6ee a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3
     2f0:	a4ae a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2
     2f6:	aeee a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp
     2fc:	a2a1 a689      	macw %a1l,%a2u,>>,%a1@-,%d1
     300:	a6e1 a689      	macw %a1l,%a2u,>>,%a1@-,%a3
     304:	a4a1 a689      	macw %a1l,%a2u,>>,%a1@-,%d2
     308:	aee1 a689      	macw %a1l,%a2u,>>,%a1@-,%sp
     30c:	a2a1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d1
     310:	a6e1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%a3
     314:	a4a1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d2
     318:	aee1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%sp
     31c:	a293 a289      	macw %a1l,%a2u,<<,%a3@,%d1
     320:	a6d3 a289      	macw %a1l,%a2u,<<,%a3@,%a3
     324:	a493 a289      	macw %a1l,%a2u,<<,%a3@,%d2
     328:	aed3 a289      	macw %a1l,%a2u,<<,%a3@,%sp
     32c:	a293 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d1
     330:	a6d3 a2a9      	macw %a1l,%a2u,<<,%a3@&,%a3
     334:	a493 a2a9      	macw %a1l,%a2u,<<,%a3@&,%d2
     338:	aed3 a2a9      	macw %a1l,%a2u,<<,%a3@&,%sp
     33c:	a29a a289      	macw %a1l,%a2u,<<,%a2@\+,%d1
     340:	a6da a289      	macw %a1l,%a2u,<<,%a2@\+,%a3
     344:	a49a a289      	macw %a1l,%a2u,<<,%a2@\+,%d2
     348:	aeda a289      	macw %a1l,%a2u,<<,%a2@\+,%sp
     34c:	a29a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d1
     350:	a6da a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%a3
     354:	a49a a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%d2
     358:	aeda a2a9      	macw %a1l,%a2u,<<,%a2@\+&,%sp
     35c:	a2ae a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d1
     362:	a6ee a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%a3
     368:	a4ae a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%d2
     36e:	aeee a289 000a 	macw %a1l,%a2u,<<,%fp@\(10\),%sp
     374:	a2ae a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d1
     37a:	a6ee a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%a3
     380:	a4ae a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%d2
     386:	aeee a2a9 000a 	macw %a1l,%a2u,<<,%fp@\(10\)&,%sp
     38c:	a2a1 a289      	macw %a1l,%a2u,<<,%a1@-,%d1
     390:	a6e1 a289      	macw %a1l,%a2u,<<,%a1@-,%a3
     394:	a4a1 a289      	macw %a1l,%a2u,<<,%a1@-,%d2
     398:	aee1 a289      	macw %a1l,%a2u,<<,%a1@-,%sp
     39c:	a2a1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d1
     3a0:	a6e1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%a3
     3a4:	a4a1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%d2
     3a8:	aee1 a2a9      	macw %a1l,%a2u,<<,%a1@-&,%sp
     3ac:	a293 a689      	macw %a1l,%a2u,>>,%a3@,%d1
     3b0:	a6d3 a689      	macw %a1l,%a2u,>>,%a3@,%a3
     3b4:	a493 a689      	macw %a1l,%a2u,>>,%a3@,%d2
     3b8:	aed3 a689      	macw %a1l,%a2u,>>,%a3@,%sp
     3bc:	a293 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d1
     3c0:	a6d3 a6a9      	macw %a1l,%a2u,>>,%a3@&,%a3
     3c4:	a493 a6a9      	macw %a1l,%a2u,>>,%a3@&,%d2
     3c8:	aed3 a6a9      	macw %a1l,%a2u,>>,%a3@&,%sp
     3cc:	a29a a689      	macw %a1l,%a2u,>>,%a2@\+,%d1
     3d0:	a6da a689      	macw %a1l,%a2u,>>,%a2@\+,%a3
     3d4:	a49a a689      	macw %a1l,%a2u,>>,%a2@\+,%d2
     3d8:	aeda a689      	macw %a1l,%a2u,>>,%a2@\+,%sp
     3dc:	a29a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d1
     3e0:	a6da a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%a3
     3e4:	a49a a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%d2
     3e8:	aeda a6a9      	macw %a1l,%a2u,>>,%a2@\+&,%sp
     3ec:	a2ae a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d1
     3f2:	a6ee a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%a3
     3f8:	a4ae a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%d2
     3fe:	aeee a689 000a 	macw %a1l,%a2u,>>,%fp@\(10\),%sp
     404:	a2ae a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d1
     40a:	a6ee a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%a3
     410:	a4ae a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%d2
     416:	aeee a6a9 000a 	macw %a1l,%a2u,>>,%fp@\(10\)&,%sp
     41c:	a2a1 a689      	macw %a1l,%a2u,>>,%a1@-,%d1
     420:	a6e1 a689      	macw %a1l,%a2u,>>,%a1@-,%a3
     424:	a4a1 a689      	macw %a1l,%a2u,>>,%a1@-,%d2
     428:	aee1 a689      	macw %a1l,%a2u,>>,%a1@-,%sp
     42c:	a2a1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d1
     430:	a6e1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%a3
     434:	a4a1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%d2
     438:	aee1 a6a9      	macw %a1l,%a2u,>>,%a1@-&,%sp
     43c:	a293 3009      	macw %a1l,%d3l,%a3@,%d1
     440:	a6d3 3009      	macw %a1l,%d3l,%a3@,%a3
     444:	a493 3009      	macw %a1l,%d3l,%a3@,%d2
     448:	aed3 3009      	macw %a1l,%d3l,%a3@,%sp
     44c:	a293 3029      	macw %a1l,%d3l,%a3@&,%d1
     450:	a6d3 3029      	macw %a1l,%d3l,%a3@&,%a3
     454:	a493 3029      	macw %a1l,%d3l,%a3@&,%d2
     458:	aed3 3029      	macw %a1l,%d3l,%a3@&,%sp
     45c:	a29a 3009      	macw %a1l,%d3l,%a2@\+,%d1
     460:	a6da 3009      	macw %a1l,%d3l,%a2@\+,%a3
     464:	a49a 3009      	macw %a1l,%d3l,%a2@\+,%d2
     468:	aeda 3009      	macw %a1l,%d3l,%a2@\+,%sp
     46c:	a29a 3029      	macw %a1l,%d3l,%a2@\+&,%d1
     470:	a6da 3029      	macw %a1l,%d3l,%a2@\+&,%a3
     474:	a49a 3029      	macw %a1l,%d3l,%a2@\+&,%d2
     478:	aeda 3029      	macw %a1l,%d3l,%a2@\+&,%sp
     47c:	a2ae 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%d1
     482:	a6ee 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%a3
     488:	a4ae 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%d2
     48e:	aeee 3009 000a 	macw %a1l,%d3l,%fp@\(10\),%sp
     494:	a2ae 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d1
     49a:	a6ee 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%a3
     4a0:	a4ae 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%d2
     4a6:	aeee 3029 000a 	macw %a1l,%d3l,%fp@\(10\)&,%sp
     4ac:	a2a1 3009      	macw %a1l,%d3l,%a1@-,%d1
     4b0:	a6e1 3009      	macw %a1l,%d3l,%a1@-,%a3
     4b4:	a4a1 3009      	macw %a1l,%d3l,%a1@-,%d2
     4b8:	aee1 3009      	macw %a1l,%d3l,%a1@-,%sp
     4bc:	a2a1 3029      	macw %a1l,%d3l,%a1@-&,%d1
     4c0:	a6e1 3029      	macw %a1l,%d3l,%a1@-&,%a3
     4c4:	a4a1 3029      	macw %a1l,%d3l,%a1@-&,%d2
     4c8:	aee1 3029      	macw %a1l,%d3l,%a1@-&,%sp
     4cc:	a293 3209      	macw %a1l,%d3l,<<,%a3@,%d1
     4d0:	a6d3 3209      	macw %a1l,%d3l,<<,%a3@,%a3
     4d4:	a493 3209      	macw %a1l,%d3l,<<,%a3@,%d2
     4d8:	aed3 3209      	macw %a1l,%d3l,<<,%a3@,%sp
     4dc:	a293 3229      	macw %a1l,%d3l,<<,%a3@&,%d1
     4e0:	a6d3 3229      	macw %a1l,%d3l,<<,%a3@&,%a3
     4e4:	a493 3229      	macw %a1l,%d3l,<<,%a3@&,%d2
     4e8:	aed3 3229      	macw %a1l,%d3l,<<,%a3@&,%sp
     4ec:	a29a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d1
     4f0:	a6da 3209      	macw %a1l,%d3l,<<,%a2@\+,%a3
     4f4:	a49a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d2
     4f8:	aeda 3209      	macw %a1l,%d3l,<<,%a2@\+,%sp
     4fc:	a29a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d1
     500:	a6da 3229      	macw %a1l,%d3l,<<,%a2@\+&,%a3
     504:	a49a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d2
     508:	aeda 3229      	macw %a1l,%d3l,<<,%a2@\+&,%sp
     50c:	a2ae 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1
     512:	a6ee 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3
     518:	a4ae 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2
     51e:	aeee 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp
     524:	a2ae 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1
     52a:	a6ee 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3
     530:	a4ae 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2
     536:	aeee 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp
     53c:	a2a1 3209      	macw %a1l,%d3l,<<,%a1@-,%d1
     540:	a6e1 3209      	macw %a1l,%d3l,<<,%a1@-,%a3
     544:	a4a1 3209      	macw %a1l,%d3l,<<,%a1@-,%d2
     548:	aee1 3209      	macw %a1l,%d3l,<<,%a1@-,%sp
     54c:	a2a1 3229      	macw %a1l,%d3l,<<,%a1@-&,%d1
     550:	a6e1 3229      	macw %a1l,%d3l,<<,%a1@-&,%a3
     554:	a4a1 3229      	macw %a1l,%d3l,<<,%a1@-&,%d2
     558:	aee1 3229      	macw %a1l,%d3l,<<,%a1@-&,%sp
     55c:	a293 3609      	macw %a1l,%d3l,>>,%a3@,%d1
     560:	a6d3 3609      	macw %a1l,%d3l,>>,%a3@,%a3
     564:	a493 3609      	macw %a1l,%d3l,>>,%a3@,%d2
     568:	aed3 3609      	macw %a1l,%d3l,>>,%a3@,%sp
     56c:	a293 3629      	macw %a1l,%d3l,>>,%a3@&,%d1
     570:	a6d3 3629      	macw %a1l,%d3l,>>,%a3@&,%a3
     574:	a493 3629      	macw %a1l,%d3l,>>,%a3@&,%d2
     578:	aed3 3629      	macw %a1l,%d3l,>>,%a3@&,%sp
     57c:	a29a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d1
     580:	a6da 3609      	macw %a1l,%d3l,>>,%a2@\+,%a3
     584:	a49a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d2
     588:	aeda 3609      	macw %a1l,%d3l,>>,%a2@\+,%sp
     58c:	a29a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d1
     590:	a6da 3629      	macw %a1l,%d3l,>>,%a2@\+&,%a3
     594:	a49a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d2
     598:	aeda 3629      	macw %a1l,%d3l,>>,%a2@\+&,%sp
     59c:	a2ae 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1
     5a2:	a6ee 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3
     5a8:	a4ae 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2
     5ae:	aeee 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp
     5b4:	a2ae 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1
     5ba:	a6ee 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3
     5c0:	a4ae 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2
     5c6:	aeee 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp
     5cc:	a2a1 3609      	macw %a1l,%d3l,>>,%a1@-,%d1
     5d0:	a6e1 3609      	macw %a1l,%d3l,>>,%a1@-,%a3
     5d4:	a4a1 3609      	macw %a1l,%d3l,>>,%a1@-,%d2
     5d8:	aee1 3609      	macw %a1l,%d3l,>>,%a1@-,%sp
     5dc:	a2a1 3629      	macw %a1l,%d3l,>>,%a1@-&,%d1
     5e0:	a6e1 3629      	macw %a1l,%d3l,>>,%a1@-&,%a3
     5e4:	a4a1 3629      	macw %a1l,%d3l,>>,%a1@-&,%d2
     5e8:	aee1 3629      	macw %a1l,%d3l,>>,%a1@-&,%sp
     5ec:	a293 3209      	macw %a1l,%d3l,<<,%a3@,%d1
     5f0:	a6d3 3209      	macw %a1l,%d3l,<<,%a3@,%a3
     5f4:	a493 3209      	macw %a1l,%d3l,<<,%a3@,%d2
     5f8:	aed3 3209      	macw %a1l,%d3l,<<,%a3@,%sp
     5fc:	a293 3229      	macw %a1l,%d3l,<<,%a3@&,%d1
     600:	a6d3 3229      	macw %a1l,%d3l,<<,%a3@&,%a3
     604:	a493 3229      	macw %a1l,%d3l,<<,%a3@&,%d2
     608:	aed3 3229      	macw %a1l,%d3l,<<,%a3@&,%sp
     60c:	a29a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d1
     610:	a6da 3209      	macw %a1l,%d3l,<<,%a2@\+,%a3
     614:	a49a 3209      	macw %a1l,%d3l,<<,%a2@\+,%d2
     618:	aeda 3209      	macw %a1l,%d3l,<<,%a2@\+,%sp
     61c:	a29a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d1
     620:	a6da 3229      	macw %a1l,%d3l,<<,%a2@\+&,%a3
     624:	a49a 3229      	macw %a1l,%d3l,<<,%a2@\+&,%d2
     628:	aeda 3229      	macw %a1l,%d3l,<<,%a2@\+&,%sp
     62c:	a2ae 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d1
     632:	a6ee 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%a3
     638:	a4ae 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%d2
     63e:	aeee 3209 000a 	macw %a1l,%d3l,<<,%fp@\(10\),%sp
     644:	a2ae 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d1
     64a:	a6ee 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%a3
     650:	a4ae 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%d2
     656:	aeee 3229 000a 	macw %a1l,%d3l,<<,%fp@\(10\)&,%sp
     65c:	a2a1 3209      	macw %a1l,%d3l,<<,%a1@-,%d1
     660:	a6e1 3209      	macw %a1l,%d3l,<<,%a1@-,%a3
     664:	a4a1 3209      	macw %a1l,%d3l,<<,%a1@-,%d2
     668:	aee1 3209      	macw %a1l,%d3l,<<,%a1@-,%sp
     66c:	a2a1 3229      	macw %a1l,%d3l,<<,%a1@-&,%d1
     670:	a6e1 3229      	macw %a1l,%d3l,<<,%a1@-&,%a3
     674:	a4a1 3229      	macw %a1l,%d3l,<<,%a1@-&,%d2
     678:	aee1 3229      	macw %a1l,%d3l,<<,%a1@-&,%sp
     67c:	a293 3609      	macw %a1l,%d3l,>>,%a3@,%d1
     680:	a6d3 3609      	macw %a1l,%d3l,>>,%a3@,%a3
     684:	a493 3609      	macw %a1l,%d3l,>>,%a3@,%d2
     688:	aed3 3609      	macw %a1l,%d3l,>>,%a3@,%sp
     68c:	a293 3629      	macw %a1l,%d3l,>>,%a3@&,%d1
     690:	a6d3 3629      	macw %a1l,%d3l,>>,%a3@&,%a3
     694:	a493 3629      	macw %a1l,%d3l,>>,%a3@&,%d2
     698:	aed3 3629      	macw %a1l,%d3l,>>,%a3@&,%sp
     69c:	a29a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d1
     6a0:	a6da 3609      	macw %a1l,%d3l,>>,%a2@\+,%a3
     6a4:	a49a 3609      	macw %a1l,%d3l,>>,%a2@\+,%d2
     6a8:	aeda 3609      	macw %a1l,%d3l,>>,%a2@\+,%sp
     6ac:	a29a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d1
     6b0:	a6da 3629      	macw %a1l,%d3l,>>,%a2@\+&,%a3
     6b4:	a49a 3629      	macw %a1l,%d3l,>>,%a2@\+&,%d2
     6b8:	aeda 3629      	macw %a1l,%d3l,>>,%a2@\+&,%sp
     6bc:	a2ae 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d1
     6c2:	a6ee 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%a3
     6c8:	a4ae 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%d2
     6ce:	aeee 3609 000a 	macw %a1l,%d3l,>>,%fp@\(10\),%sp
     6d4:	a2ae 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d1
     6da:	a6ee 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%a3
     6e0:	a4ae 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%d2
     6e6:	aeee 3629 000a 	macw %a1l,%d3l,>>,%fp@\(10\)&,%sp
     6ec:	a2a1 3609      	macw %a1l,%d3l,>>,%a1@-,%d1
     6f0:	a6e1 3609      	macw %a1l,%d3l,>>,%a1@-,%a3
     6f4:	a4a1 3609      	macw %a1l,%d3l,>>,%a1@-,%d2
     6f8:	aee1 3609      	macw %a1l,%d3l,>>,%a1@-,%sp
     6fc:	a2a1 3629      	macw %a1l,%d3l,>>,%a1@-&,%d1
     700:	a6e1 3629      	macw %a1l,%d3l,>>,%a1@-&,%a3
     704:	a4a1 3629      	macw %a1l,%d3l,>>,%a1@-&,%d2
     708:	aee1 3629      	macw %a1l,%d3l,>>,%a1@-&,%sp
     70c:	a293 f089      	macw %a1l,%a7u,%a3@,%d1
     710:	a6d3 f089      	macw %a1l,%a7u,%a3@,%a3
     714:	a493 f089      	macw %a1l,%a7u,%a3@,%d2
     718:	aed3 f089      	macw %a1l,%a7u,%a3@,%sp
     71c:	a293 f0a9      	macw %a1l,%a7u,%a3@&,%d1
     720:	a6d3 f0a9      	macw %a1l,%a7u,%a3@&,%a3
     724:	a493 f0a9      	macw %a1l,%a7u,%a3@&,%d2
     728:	aed3 f0a9      	macw %a1l,%a7u,%a3@&,%sp
     72c:	a29a f089      	macw %a1l,%a7u,%a2@\+,%d1
     730:	a6da f089      	macw %a1l,%a7u,%a2@\+,%a3
     734:	a49a f089      	macw %a1l,%a7u,%a2@\+,%d2
     738:	aeda f089      	macw %a1l,%a7u,%a2@\+,%sp
     73c:	a29a f0a9      	macw %a1l,%a7u,%a2@\+&,%d1
     740:	a6da f0a9      	macw %a1l,%a7u,%a2@\+&,%a3
     744:	a49a f0a9      	macw %a1l,%a7u,%a2@\+&,%d2
     748:	aeda f0a9      	macw %a1l,%a7u,%a2@\+&,%sp
     74c:	a2ae f089 000a 	macw %a1l,%a7u,%fp@\(10\),%d1
     752:	a6ee f089 000a 	macw %a1l,%a7u,%fp@\(10\),%a3
     758:	a4ae f089 000a 	macw %a1l,%a7u,%fp@\(10\),%d2
     75e:	aeee f089 000a 	macw %a1l,%a7u,%fp@\(10\),%sp
     764:	a2ae f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d1
     76a:	a6ee f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%a3
     770:	a4ae f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%d2
     776:	aeee f0a9 000a 	macw %a1l,%a7u,%fp@\(10\)&,%sp
     77c:	a2a1 f089      	macw %a1l,%a7u,%a1@-,%d1
     780:	a6e1 f089      	macw %a1l,%a7u,%a1@-,%a3
     784:	a4a1 f089      	macw %a1l,%a7u,%a1@-,%d2
     788:	aee1 f089      	macw %a1l,%a7u,%a1@-,%sp
     78c:	a2a1 f0a9      	macw %a1l,%a7u,%a1@-&,%d1
     790:	a6e1 f0a9      	macw %a1l,%a7u,%a1@-&,%a3
     794:	a4a1 f0a9      	macw %a1l,%a7u,%a1@-&,%d2
     798:	aee1 f0a9      	macw %a1l,%a7u,%a1@-&,%sp
     79c:	a293 f289      	macw %a1l,%a7u,<<,%a3@,%d1
     7a0:	a6d3 f289      	macw %a1l,%a7u,<<,%a3@,%a3
     7a4:	a493 f289      	macw %a1l,%a7u,<<,%a3@,%d2
     7a8:	aed3 f289      	macw %a1l,%a7u,<<,%a3@,%sp
     7ac:	a293 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d1
     7b0:	a6d3 f2a9      	macw %a1l,%a7u,<<,%a3@&,%a3
     7b4:	a493 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d2
     7b8:	aed3 f2a9      	macw %a1l,%a7u,<<,%a3@&,%sp
     7bc:	a29a f289      	macw %a1l,%a7u,<<,%a2@\+,%d1
     7c0:	a6da f289      	macw %a1l,%a7u,<<,%a2@\+,%a3
     7c4:	a49a f289      	macw %a1l,%a7u,<<,%a2@\+,%d2
     7c8:	aeda f289      	macw %a1l,%a7u,<<,%a2@\+,%sp
     7cc:	a29a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d1
     7d0:	a6da f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%a3
     7d4:	a49a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d2
     7d8:	aeda f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%sp
     7dc:	a2ae f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1
     7e2:	a6ee f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3
     7e8:	a4ae f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2
     7ee:	aeee f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp
     7f4:	a2ae f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1
     7fa:	a6ee f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3
     800:	a4ae f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2
     806:	aeee f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp
     80c:	a2a1 f289      	macw %a1l,%a7u,<<,%a1@-,%d1
     810:	a6e1 f289      	macw %a1l,%a7u,<<,%a1@-,%a3
     814:	a4a1 f289      	macw %a1l,%a7u,<<,%a1@-,%d2
     818:	aee1 f289      	macw %a1l,%a7u,<<,%a1@-,%sp
     81c:	a2a1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d1
     820:	a6e1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%a3
     824:	a4a1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d2
     828:	aee1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%sp
     82c:	a293 f689      	macw %a1l,%a7u,>>,%a3@,%d1
     830:	a6d3 f689      	macw %a1l,%a7u,>>,%a3@,%a3
     834:	a493 f689      	macw %a1l,%a7u,>>,%a3@,%d2
     838:	aed3 f689      	macw %a1l,%a7u,>>,%a3@,%sp
     83c:	a293 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d1
     840:	a6d3 f6a9      	macw %a1l,%a7u,>>,%a3@&,%a3
     844:	a493 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d2
     848:	aed3 f6a9      	macw %a1l,%a7u,>>,%a3@&,%sp
     84c:	a29a f689      	macw %a1l,%a7u,>>,%a2@\+,%d1
     850:	a6da f689      	macw %a1l,%a7u,>>,%a2@\+,%a3
     854:	a49a f689      	macw %a1l,%a7u,>>,%a2@\+,%d2
     858:	aeda f689      	macw %a1l,%a7u,>>,%a2@\+,%sp
     85c:	a29a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d1
     860:	a6da f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%a3
     864:	a49a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d2
     868:	aeda f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%sp
     86c:	a2ae f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1
     872:	a6ee f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3
     878:	a4ae f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2
     87e:	aeee f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp
     884:	a2ae f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1
     88a:	a6ee f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3
     890:	a4ae f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2
     896:	aeee f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp
     89c:	a2a1 f689      	macw %a1l,%a7u,>>,%a1@-,%d1
     8a0:	a6e1 f689      	macw %a1l,%a7u,>>,%a1@-,%a3
     8a4:	a4a1 f689      	macw %a1l,%a7u,>>,%a1@-,%d2
     8a8:	aee1 f689      	macw %a1l,%a7u,>>,%a1@-,%sp
     8ac:	a2a1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d1
     8b0:	a6e1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%a3
     8b4:	a4a1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d2
     8b8:	aee1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%sp
     8bc:	a293 f289      	macw %a1l,%a7u,<<,%a3@,%d1
     8c0:	a6d3 f289      	macw %a1l,%a7u,<<,%a3@,%a3
     8c4:	a493 f289      	macw %a1l,%a7u,<<,%a3@,%d2
     8c8:	aed3 f289      	macw %a1l,%a7u,<<,%a3@,%sp
     8cc:	a293 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d1
     8d0:	a6d3 f2a9      	macw %a1l,%a7u,<<,%a3@&,%a3
     8d4:	a493 f2a9      	macw %a1l,%a7u,<<,%a3@&,%d2
     8d8:	aed3 f2a9      	macw %a1l,%a7u,<<,%a3@&,%sp
     8dc:	a29a f289      	macw %a1l,%a7u,<<,%a2@\+,%d1
     8e0:	a6da f289      	macw %a1l,%a7u,<<,%a2@\+,%a3
     8e4:	a49a f289      	macw %a1l,%a7u,<<,%a2@\+,%d2
     8e8:	aeda f289      	macw %a1l,%a7u,<<,%a2@\+,%sp
     8ec:	a29a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d1
     8f0:	a6da f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%a3
     8f4:	a49a f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%d2
     8f8:	aeda f2a9      	macw %a1l,%a7u,<<,%a2@\+&,%sp
     8fc:	a2ae f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d1
     902:	a6ee f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%a3
     908:	a4ae f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%d2
     90e:	aeee f289 000a 	macw %a1l,%a7u,<<,%fp@\(10\),%sp
     914:	a2ae f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d1
     91a:	a6ee f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%a3
     920:	a4ae f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%d2
     926:	aeee f2a9 000a 	macw %a1l,%a7u,<<,%fp@\(10\)&,%sp
     92c:	a2a1 f289      	macw %a1l,%a7u,<<,%a1@-,%d1
     930:	a6e1 f289      	macw %a1l,%a7u,<<,%a1@-,%a3
     934:	a4a1 f289      	macw %a1l,%a7u,<<,%a1@-,%d2
     938:	aee1 f289      	macw %a1l,%a7u,<<,%a1@-,%sp
     93c:	a2a1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d1
     940:	a6e1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%a3
     944:	a4a1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%d2
     948:	aee1 f2a9      	macw %a1l,%a7u,<<,%a1@-&,%sp
     94c:	a293 f689      	macw %a1l,%a7u,>>,%a3@,%d1
     950:	a6d3 f689      	macw %a1l,%a7u,>>,%a3@,%a3
     954:	a493 f689      	macw %a1l,%a7u,>>,%a3@,%d2
     958:	aed3 f689      	macw %a1l,%a7u,>>,%a3@,%sp
     95c:	a293 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d1
     960:	a6d3 f6a9      	macw %a1l,%a7u,>>,%a3@&,%a3
     964:	a493 f6a9      	macw %a1l,%a7u,>>,%a3@&,%d2
     968:	aed3 f6a9      	macw %a1l,%a7u,>>,%a3@&,%sp
     96c:	a29a f689      	macw %a1l,%a7u,>>,%a2@\+,%d1
     970:	a6da f689      	macw %a1l,%a7u,>>,%a2@\+,%a3
     974:	a49a f689      	macw %a1l,%a7u,>>,%a2@\+,%d2
     978:	aeda f689      	macw %a1l,%a7u,>>,%a2@\+,%sp
     97c:	a29a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d1
     980:	a6da f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%a3
     984:	a49a f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%d2
     988:	aeda f6a9      	macw %a1l,%a7u,>>,%a2@\+&,%sp
     98c:	a2ae f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d1
     992:	a6ee f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%a3
     998:	a4ae f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%d2
     99e:	aeee f689 000a 	macw %a1l,%a7u,>>,%fp@\(10\),%sp
     9a4:	a2ae f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d1
     9aa:	a6ee f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%a3
     9b0:	a4ae f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%d2
     9b6:	aeee f6a9 000a 	macw %a1l,%a7u,>>,%fp@\(10\)&,%sp
     9bc:	a2a1 f689      	macw %a1l,%a7u,>>,%a1@-,%d1
     9c0:	a6e1 f689      	macw %a1l,%a7u,>>,%a1@-,%a3
     9c4:	a4a1 f689      	macw %a1l,%a7u,>>,%a1@-,%d2
     9c8:	aee1 f689      	macw %a1l,%a7u,>>,%a1@-,%sp
     9cc:	a2a1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d1
     9d0:	a6e1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%a3
     9d4:	a4a1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%d2
     9d8:	aee1 f6a9      	macw %a1l,%a7u,>>,%a1@-&,%sp
     9dc:	a293 1009      	macw %a1l,%d1l,%a3@,%d1
     9e0:	a6d3 1009      	macw %a1l,%d1l,%a3@,%a3
     9e4:	a493 1009      	macw %a1l,%d1l,%a3@,%d2
     9e8:	aed3 1009      	macw %a1l,%d1l,%a3@,%sp
     9ec:	a293 1029      	macw %a1l,%d1l,%a3@&,%d1
     9f0:	a6d3 1029      	macw %a1l,%d1l,%a3@&,%a3
     9f4:	a493 1029      	macw %a1l,%d1l,%a3@&,%d2
     9f8:	aed3 1029      	macw %a1l,%d1l,%a3@&,%sp
     9fc:	a29a 1009      	macw %a1l,%d1l,%a2@\+,%d1
     a00:	a6da 1009      	macw %a1l,%d1l,%a2@\+,%a3
     a04:	a49a 1009      	macw %a1l,%d1l,%a2@\+,%d2
     a08:	aeda 1009      	macw %a1l,%d1l,%a2@\+,%sp
     a0c:	a29a 1029      	macw %a1l,%d1l,%a2@\+&,%d1
     a10:	a6da 1029      	macw %a1l,%d1l,%a2@\+&,%a3
     a14:	a49a 1029      	macw %a1l,%d1l,%a2@\+&,%d2
     a18:	aeda 1029      	macw %a1l,%d1l,%a2@\+&,%sp
     a1c:	a2ae 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%d1
     a22:	a6ee 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%a3
     a28:	a4ae 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%d2
     a2e:	aeee 1009 000a 	macw %a1l,%d1l,%fp@\(10\),%sp
     a34:	a2ae 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d1
     a3a:	a6ee 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%a3
     a40:	a4ae 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%d2
     a46:	aeee 1029 000a 	macw %a1l,%d1l,%fp@\(10\)&,%sp
     a4c:	a2a1 1009      	macw %a1l,%d1l,%a1@-,%d1
     a50:	a6e1 1009      	macw %a1l,%d1l,%a1@-,%a3
     a54:	a4a1 1009      	macw %a1l,%d1l,%a1@-,%d2
     a58:	aee1 1009      	macw %a1l,%d1l,%a1@-,%sp
     a5c:	a2a1 1029      	macw %a1l,%d1l,%a1@-&,%d1
     a60:	a6e1 1029      	macw %a1l,%d1l,%a1@-&,%a3
     a64:	a4a1 1029      	macw %a1l,%d1l,%a1@-&,%d2
     a68:	aee1 1029      	macw %a1l,%d1l,%a1@-&,%sp
     a6c:	a293 1209      	macw %a1l,%d1l,<<,%a3@,%d1
     a70:	a6d3 1209      	macw %a1l,%d1l,<<,%a3@,%a3
     a74:	a493 1209      	macw %a1l,%d1l,<<,%a3@,%d2
     a78:	aed3 1209      	macw %a1l,%d1l,<<,%a3@,%sp
     a7c:	a293 1229      	macw %a1l,%d1l,<<,%a3@&,%d1
     a80:	a6d3 1229      	macw %a1l,%d1l,<<,%a3@&,%a3
     a84:	a493 1229      	macw %a1l,%d1l,<<,%a3@&,%d2
     a88:	aed3 1229      	macw %a1l,%d1l,<<,%a3@&,%sp
     a8c:	a29a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d1
     a90:	a6da 1209      	macw %a1l,%d1l,<<,%a2@\+,%a3
     a94:	a49a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d2
     a98:	aeda 1209      	macw %a1l,%d1l,<<,%a2@\+,%sp
     a9c:	a29a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d1
     aa0:	a6da 1229      	macw %a1l,%d1l,<<,%a2@\+&,%a3
     aa4:	a49a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d2
     aa8:	aeda 1229      	macw %a1l,%d1l,<<,%a2@\+&,%sp
     aac:	a2ae 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1
     ab2:	a6ee 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3
     ab8:	a4ae 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2
     abe:	aeee 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp
     ac4:	a2ae 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1
     aca:	a6ee 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3
     ad0:	a4ae 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2
     ad6:	aeee 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp
     adc:	a2a1 1209      	macw %a1l,%d1l,<<,%a1@-,%d1
     ae0:	a6e1 1209      	macw %a1l,%d1l,<<,%a1@-,%a3
     ae4:	a4a1 1209      	macw %a1l,%d1l,<<,%a1@-,%d2
     ae8:	aee1 1209      	macw %a1l,%d1l,<<,%a1@-,%sp
     aec:	a2a1 1229      	macw %a1l,%d1l,<<,%a1@-&,%d1
     af0:	a6e1 1229      	macw %a1l,%d1l,<<,%a1@-&,%a3
     af4:	a4a1 1229      	macw %a1l,%d1l,<<,%a1@-&,%d2
     af8:	aee1 1229      	macw %a1l,%d1l,<<,%a1@-&,%sp
     afc:	a293 1609      	macw %a1l,%d1l,>>,%a3@,%d1
     b00:	a6d3 1609      	macw %a1l,%d1l,>>,%a3@,%a3
     b04:	a493 1609      	macw %a1l,%d1l,>>,%a3@,%d2
     b08:	aed3 1609      	macw %a1l,%d1l,>>,%a3@,%sp
     b0c:	a293 1629      	macw %a1l,%d1l,>>,%a3@&,%d1
     b10:	a6d3 1629      	macw %a1l,%d1l,>>,%a3@&,%a3
     b14:	a493 1629      	macw %a1l,%d1l,>>,%a3@&,%d2
     b18:	aed3 1629      	macw %a1l,%d1l,>>,%a3@&,%sp
     b1c:	a29a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d1
     b20:	a6da 1609      	macw %a1l,%d1l,>>,%a2@\+,%a3
     b24:	a49a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d2
     b28:	aeda 1609      	macw %a1l,%d1l,>>,%a2@\+,%sp
     b2c:	a29a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d1
     b30:	a6da 1629      	macw %a1l,%d1l,>>,%a2@\+&,%a3
     b34:	a49a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d2
     b38:	aeda 1629      	macw %a1l,%d1l,>>,%a2@\+&,%sp
     b3c:	a2ae 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1
     b42:	a6ee 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3
     b48:	a4ae 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2
     b4e:	aeee 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp
     b54:	a2ae 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1
     b5a:	a6ee 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3
     b60:	a4ae 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2
     b66:	aeee 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp
     b6c:	a2a1 1609      	macw %a1l,%d1l,>>,%a1@-,%d1
     b70:	a6e1 1609      	macw %a1l,%d1l,>>,%a1@-,%a3
     b74:	a4a1 1609      	macw %a1l,%d1l,>>,%a1@-,%d2
     b78:	aee1 1609      	macw %a1l,%d1l,>>,%a1@-,%sp
     b7c:	a2a1 1629      	macw %a1l,%d1l,>>,%a1@-&,%d1
     b80:	a6e1 1629      	macw %a1l,%d1l,>>,%a1@-&,%a3
     b84:	a4a1 1629      	macw %a1l,%d1l,>>,%a1@-&,%d2
     b88:	aee1 1629      	macw %a1l,%d1l,>>,%a1@-&,%sp
     b8c:	a293 1209      	macw %a1l,%d1l,<<,%a3@,%d1
     b90:	a6d3 1209      	macw %a1l,%d1l,<<,%a3@,%a3
     b94:	a493 1209      	macw %a1l,%d1l,<<,%a3@,%d2
     b98:	aed3 1209      	macw %a1l,%d1l,<<,%a3@,%sp
     b9c:	a293 1229      	macw %a1l,%d1l,<<,%a3@&,%d1
     ba0:	a6d3 1229      	macw %a1l,%d1l,<<,%a3@&,%a3
     ba4:	a493 1229      	macw %a1l,%d1l,<<,%a3@&,%d2
     ba8:	aed3 1229      	macw %a1l,%d1l,<<,%a3@&,%sp
     bac:	a29a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d1
     bb0:	a6da 1209      	macw %a1l,%d1l,<<,%a2@\+,%a3
     bb4:	a49a 1209      	macw %a1l,%d1l,<<,%a2@\+,%d2
     bb8:	aeda 1209      	macw %a1l,%d1l,<<,%a2@\+,%sp
     bbc:	a29a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d1
     bc0:	a6da 1229      	macw %a1l,%d1l,<<,%a2@\+&,%a3
     bc4:	a49a 1229      	macw %a1l,%d1l,<<,%a2@\+&,%d2
     bc8:	aeda 1229      	macw %a1l,%d1l,<<,%a2@\+&,%sp
     bcc:	a2ae 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d1
     bd2:	a6ee 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%a3
     bd8:	a4ae 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%d2
     bde:	aeee 1209 000a 	macw %a1l,%d1l,<<,%fp@\(10\),%sp
     be4:	a2ae 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d1
     bea:	a6ee 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%a3
     bf0:	a4ae 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%d2
     bf6:	aeee 1229 000a 	macw %a1l,%d1l,<<,%fp@\(10\)&,%sp
     bfc:	a2a1 1209      	macw %a1l,%d1l,<<,%a1@-,%d1
     c00:	a6e1 1209      	macw %a1l,%d1l,<<,%a1@-,%a3
     c04:	a4a1 1209      	macw %a1l,%d1l,<<,%a1@-,%d2
     c08:	aee1 1209      	macw %a1l,%d1l,<<,%a1@-,%sp
     c0c:	a2a1 1229      	macw %a1l,%d1l,<<,%a1@-&,%d1
     c10:	a6e1 1229      	macw %a1l,%d1l,<<,%a1@-&,%a3
     c14:	a4a1 1229      	macw %a1l,%d1l,<<,%a1@-&,%d2
     c18:	aee1 1229      	macw %a1l,%d1l,<<,%a1@-&,%sp
     c1c:	a293 1609      	macw %a1l,%d1l,>>,%a3@,%d1
     c20:	a6d3 1609      	macw %a1l,%d1l,>>,%a3@,%a3
     c24:	a493 1609      	macw %a1l,%d1l,>>,%a3@,%d2
     c28:	aed3 1609      	macw %a1l,%d1l,>>,%a3@,%sp
     c2c:	a293 1629      	macw %a1l,%d1l,>>,%a3@&,%d1
     c30:	a6d3 1629      	macw %a1l,%d1l,>>,%a3@&,%a3
     c34:	a493 1629      	macw %a1l,%d1l,>>,%a3@&,%d2
     c38:	aed3 1629      	macw %a1l,%d1l,>>,%a3@&,%sp
     c3c:	a29a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d1
     c40:	a6da 1609      	macw %a1l,%d1l,>>,%a2@\+,%a3
     c44:	a49a 1609      	macw %a1l,%d1l,>>,%a2@\+,%d2
     c48:	aeda 1609      	macw %a1l,%d1l,>>,%a2@\+,%sp
     c4c:	a29a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d1
     c50:	a6da 1629      	macw %a1l,%d1l,>>,%a2@\+&,%a3
     c54:	a49a 1629      	macw %a1l,%d1l,>>,%a2@\+&,%d2
     c58:	aeda 1629      	macw %a1l,%d1l,>>,%a2@\+&,%sp
     c5c:	a2ae 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d1
     c62:	a6ee 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%a3
     c68:	a4ae 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%d2
     c6e:	aeee 1609 000a 	macw %a1l,%d1l,>>,%fp@\(10\),%sp
     c74:	a2ae 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d1
     c7a:	a6ee 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%a3
     c80:	a4ae 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%d2
     c86:	aeee 1629 000a 	macw %a1l,%d1l,>>,%fp@\(10\)&,%sp
     c8c:	a2a1 1609      	macw %a1l,%d1l,>>,%a1@-,%d1
     c90:	a6e1 1609      	macw %a1l,%d1l,>>,%a1@-,%a3
     c94:	a4a1 1609      	macw %a1l,%d1l,>>,%a1@-,%d2
     c98:	aee1 1609      	macw %a1l,%d1l,>>,%a1@-,%sp
     c9c:	a2a1 1629      	macw %a1l,%d1l,>>,%a1@-&,%d1
     ca0:	a6e1 1629      	macw %a1l,%d1l,>>,%a1@-&,%a3
     ca4:	a4a1 1629      	macw %a1l,%d1l,>>,%a1@-&,%d2
     ca8:	aee1 1629      	macw %a1l,%d1l,>>,%a1@-&,%sp
     cac:	a293 a0c2      	macw %d2u,%a2u,%a3@,%d1
     cb0:	a6d3 a0c2      	macw %d2u,%a2u,%a3@,%a3
     cb4:	a493 a0c2      	macw %d2u,%a2u,%a3@,%d2
     cb8:	aed3 a0c2      	macw %d2u,%a2u,%a3@,%sp
     cbc:	a293 a0e2      	macw %d2u,%a2u,%a3@&,%d1
     cc0:	a6d3 a0e2      	macw %d2u,%a2u,%a3@&,%a3
     cc4:	a493 a0e2      	macw %d2u,%a2u,%a3@&,%d2
     cc8:	aed3 a0e2      	macw %d2u,%a2u,%a3@&,%sp
     ccc:	a29a a0c2      	macw %d2u,%a2u,%a2@\+,%d1
     cd0:	a6da a0c2      	macw %d2u,%a2u,%a2@\+,%a3
     cd4:	a49a a0c2      	macw %d2u,%a2u,%a2@\+,%d2
     cd8:	aeda a0c2      	macw %d2u,%a2u,%a2@\+,%sp
     cdc:	a29a a0e2      	macw %d2u,%a2u,%a2@\+&,%d1
     ce0:	a6da a0e2      	macw %d2u,%a2u,%a2@\+&,%a3
     ce4:	a49a a0e2      	macw %d2u,%a2u,%a2@\+&,%d2
     ce8:	aeda a0e2      	macw %d2u,%a2u,%a2@\+&,%sp
     cec:	a2ae a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%d1
     cf2:	a6ee a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%a3
     cf8:	a4ae a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%d2
     cfe:	aeee a0c2 000a 	macw %d2u,%a2u,%fp@\(10\),%sp
     d04:	a2ae a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d1
     d0a:	a6ee a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%a3
     d10:	a4ae a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%d2
     d16:	aeee a0e2 000a 	macw %d2u,%a2u,%fp@\(10\)&,%sp
     d1c:	a2a1 a0c2      	macw %d2u,%a2u,%a1@-,%d1
     d20:	a6e1 a0c2      	macw %d2u,%a2u,%a1@-,%a3
     d24:	a4a1 a0c2      	macw %d2u,%a2u,%a1@-,%d2
     d28:	aee1 a0c2      	macw %d2u,%a2u,%a1@-,%sp
     d2c:	a2a1 a0e2      	macw %d2u,%a2u,%a1@-&,%d1
     d30:	a6e1 a0e2      	macw %d2u,%a2u,%a1@-&,%a3
     d34:	a4a1 a0e2      	macw %d2u,%a2u,%a1@-&,%d2
     d38:	aee1 a0e2      	macw %d2u,%a2u,%a1@-&,%sp
     d3c:	a293 a2c2      	macw %d2u,%a2u,<<,%a3@,%d1
     d40:	a6d3 a2c2      	macw %d2u,%a2u,<<,%a3@,%a3
     d44:	a493 a2c2      	macw %d2u,%a2u,<<,%a3@,%d2
     d48:	aed3 a2c2      	macw %d2u,%a2u,<<,%a3@,%sp
     d4c:	a293 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d1
     d50:	a6d3 a2e2      	macw %d2u,%a2u,<<,%a3@&,%a3
     d54:	a493 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d2
     d58:	aed3 a2e2      	macw %d2u,%a2u,<<,%a3@&,%sp
     d5c:	a29a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d1
     d60:	a6da a2c2      	macw %d2u,%a2u,<<,%a2@\+,%a3
     d64:	a49a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d2
     d68:	aeda a2c2      	macw %d2u,%a2u,<<,%a2@\+,%sp
     d6c:	a29a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d1
     d70:	a6da a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%a3
     d74:	a49a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d2
     d78:	aeda a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%sp
     d7c:	a2ae a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1
     d82:	a6ee a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3
     d88:	a4ae a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2
     d8e:	aeee a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp
     d94:	a2ae a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1
     d9a:	a6ee a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3
     da0:	a4ae a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2
     da6:	aeee a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp
     dac:	a2a1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d1
     db0:	a6e1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%a3
     db4:	a4a1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d2
     db8:	aee1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%sp
     dbc:	a2a1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d1
     dc0:	a6e1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%a3
     dc4:	a4a1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d2
     dc8:	aee1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%sp
     dcc:	a293 a6c2      	macw %d2u,%a2u,>>,%a3@,%d1
     dd0:	a6d3 a6c2      	macw %d2u,%a2u,>>,%a3@,%a3
     dd4:	a493 a6c2      	macw %d2u,%a2u,>>,%a3@,%d2
     dd8:	aed3 a6c2      	macw %d2u,%a2u,>>,%a3@,%sp
     ddc:	a293 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d1
     de0:	a6d3 a6e2      	macw %d2u,%a2u,>>,%a3@&,%a3
     de4:	a493 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d2
     de8:	aed3 a6e2      	macw %d2u,%a2u,>>,%a3@&,%sp
     dec:	a29a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d1
     df0:	a6da a6c2      	macw %d2u,%a2u,>>,%a2@\+,%a3
     df4:	a49a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d2
     df8:	aeda a6c2      	macw %d2u,%a2u,>>,%a2@\+,%sp
     dfc:	a29a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d1
     e00:	a6da a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%a3
     e04:	a49a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d2
     e08:	aeda a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%sp
     e0c:	a2ae a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1
     e12:	a6ee a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3
     e18:	a4ae a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2
     e1e:	aeee a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp
     e24:	a2ae a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1
     e2a:	a6ee a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3
     e30:	a4ae a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2
     e36:	aeee a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp
     e3c:	a2a1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d1
     e40:	a6e1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%a3
     e44:	a4a1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d2
     e48:	aee1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%sp
     e4c:	a2a1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d1
     e50:	a6e1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%a3
     e54:	a4a1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d2
     e58:	aee1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%sp
     e5c:	a293 a2c2      	macw %d2u,%a2u,<<,%a3@,%d1
     e60:	a6d3 a2c2      	macw %d2u,%a2u,<<,%a3@,%a3
     e64:	a493 a2c2      	macw %d2u,%a2u,<<,%a3@,%d2
     e68:	aed3 a2c2      	macw %d2u,%a2u,<<,%a3@,%sp
     e6c:	a293 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d1
     e70:	a6d3 a2e2      	macw %d2u,%a2u,<<,%a3@&,%a3
     e74:	a493 a2e2      	macw %d2u,%a2u,<<,%a3@&,%d2
     e78:	aed3 a2e2      	macw %d2u,%a2u,<<,%a3@&,%sp
     e7c:	a29a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d1
     e80:	a6da a2c2      	macw %d2u,%a2u,<<,%a2@\+,%a3
     e84:	a49a a2c2      	macw %d2u,%a2u,<<,%a2@\+,%d2
     e88:	aeda a2c2      	macw %d2u,%a2u,<<,%a2@\+,%sp
     e8c:	a29a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d1
     e90:	a6da a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%a3
     e94:	a49a a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%d2
     e98:	aeda a2e2      	macw %d2u,%a2u,<<,%a2@\+&,%sp
     e9c:	a2ae a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d1
     ea2:	a6ee a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%a3
     ea8:	a4ae a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%d2
     eae:	aeee a2c2 000a 	macw %d2u,%a2u,<<,%fp@\(10\),%sp
     eb4:	a2ae a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d1
     eba:	a6ee a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%a3
     ec0:	a4ae a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%d2
     ec6:	aeee a2e2 000a 	macw %d2u,%a2u,<<,%fp@\(10\)&,%sp
     ecc:	a2a1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d1
     ed0:	a6e1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%a3
     ed4:	a4a1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%d2
     ed8:	aee1 a2c2      	macw %d2u,%a2u,<<,%a1@-,%sp
     edc:	a2a1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d1
     ee0:	a6e1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%a3
     ee4:	a4a1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%d2
     ee8:	aee1 a2e2      	macw %d2u,%a2u,<<,%a1@-&,%sp
     eec:	a293 a6c2      	macw %d2u,%a2u,>>,%a3@,%d1
     ef0:	a6d3 a6c2      	macw %d2u,%a2u,>>,%a3@,%a3
     ef4:	a493 a6c2      	macw %d2u,%a2u,>>,%a3@,%d2
     ef8:	aed3 a6c2      	macw %d2u,%a2u,>>,%a3@,%sp
     efc:	a293 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d1
     f00:	a6d3 a6e2      	macw %d2u,%a2u,>>,%a3@&,%a3
     f04:	a493 a6e2      	macw %d2u,%a2u,>>,%a3@&,%d2
     f08:	aed3 a6e2      	macw %d2u,%a2u,>>,%a3@&,%sp
     f0c:	a29a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d1
     f10:	a6da a6c2      	macw %d2u,%a2u,>>,%a2@\+,%a3
     f14:	a49a a6c2      	macw %d2u,%a2u,>>,%a2@\+,%d2
     f18:	aeda a6c2      	macw %d2u,%a2u,>>,%a2@\+,%sp
     f1c:	a29a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d1
     f20:	a6da a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%a3
     f24:	a49a a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%d2
     f28:	aeda a6e2      	macw %d2u,%a2u,>>,%a2@\+&,%sp
     f2c:	a2ae a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d1
     f32:	a6ee a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%a3
     f38:	a4ae a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%d2
     f3e:	aeee a6c2 000a 	macw %d2u,%a2u,>>,%fp@\(10\),%sp
     f44:	a2ae a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d1
     f4a:	a6ee a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%a3
     f50:	a4ae a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%d2
     f56:	aeee a6e2 000a 	macw %d2u,%a2u,>>,%fp@\(10\)&,%sp
     f5c:	a2a1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d1
     f60:	a6e1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%a3
     f64:	a4a1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%d2
     f68:	aee1 a6c2      	macw %d2u,%a2u,>>,%a1@-,%sp
     f6c:	a2a1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d1
     f70:	a6e1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%a3
     f74:	a4a1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%d2
     f78:	aee1 a6e2      	macw %d2u,%a2u,>>,%a1@-&,%sp
     f7c:	a293 3042      	macw %d2u,%d3l,%a3@,%d1
     f80:	a6d3 3042      	macw %d2u,%d3l,%a3@,%a3
     f84:	a493 3042      	macw %d2u,%d3l,%a3@,%d2
     f88:	aed3 3042      	macw %d2u,%d3l,%a3@,%sp
     f8c:	a293 3062      	macw %d2u,%d3l,%a3@&,%d1
     f90:	a6d3 3062      	macw %d2u,%d3l,%a3@&,%a3
     f94:	a493 3062      	macw %d2u,%d3l,%a3@&,%d2
     f98:	aed3 3062      	macw %d2u,%d3l,%a3@&,%sp
     f9c:	a29a 3042      	macw %d2u,%d3l,%a2@\+,%d1
     fa0:	a6da 3042      	macw %d2u,%d3l,%a2@\+,%a3
     fa4:	a49a 3042      	macw %d2u,%d3l,%a2@\+,%d2
     fa8:	aeda 3042      	macw %d2u,%d3l,%a2@\+,%sp
     fac:	a29a 3062      	macw %d2u,%d3l,%a2@\+&,%d1
     fb0:	a6da 3062      	macw %d2u,%d3l,%a2@\+&,%a3
     fb4:	a49a 3062      	macw %d2u,%d3l,%a2@\+&,%d2
     fb8:	aeda 3062      	macw %d2u,%d3l,%a2@\+&,%sp
     fbc:	a2ae 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%d1
     fc2:	a6ee 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%a3
     fc8:	a4ae 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%d2
     fce:	aeee 3042 000a 	macw %d2u,%d3l,%fp@\(10\),%sp
     fd4:	a2ae 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d1
     fda:	a6ee 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%a3
     fe0:	a4ae 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%d2
     fe6:	aeee 3062 000a 	macw %d2u,%d3l,%fp@\(10\)&,%sp
     fec:	a2a1 3042      	macw %d2u,%d3l,%a1@-,%d1
     ff0:	a6e1 3042      	macw %d2u,%d3l,%a1@-,%a3
     ff4:	a4a1 3042      	macw %d2u,%d3l,%a1@-,%d2
     ff8:	aee1 3042      	macw %d2u,%d3l,%a1@-,%sp
     ffc:	a2a1 3062      	macw %d2u,%d3l,%a1@-&,%d1
    1000:	a6e1 3062      	macw %d2u,%d3l,%a1@-&,%a3
    1004:	a4a1 3062      	macw %d2u,%d3l,%a1@-&,%d2
    1008:	aee1 3062      	macw %d2u,%d3l,%a1@-&,%sp
    100c:	a293 3242      	macw %d2u,%d3l,<<,%a3@,%d1
    1010:	a6d3 3242      	macw %d2u,%d3l,<<,%a3@,%a3
    1014:	a493 3242      	macw %d2u,%d3l,<<,%a3@,%d2
    1018:	aed3 3242      	macw %d2u,%d3l,<<,%a3@,%sp
    101c:	a293 3262      	macw %d2u,%d3l,<<,%a3@&,%d1
    1020:	a6d3 3262      	macw %d2u,%d3l,<<,%a3@&,%a3
    1024:	a493 3262      	macw %d2u,%d3l,<<,%a3@&,%d2
    1028:	aed3 3262      	macw %d2u,%d3l,<<,%a3@&,%sp
    102c:	a29a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d1
    1030:	a6da 3242      	macw %d2u,%d3l,<<,%a2@\+,%a3
    1034:	a49a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d2
    1038:	aeda 3242      	macw %d2u,%d3l,<<,%a2@\+,%sp
    103c:	a29a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d1
    1040:	a6da 3262      	macw %d2u,%d3l,<<,%a2@\+&,%a3
    1044:	a49a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d2
    1048:	aeda 3262      	macw %d2u,%d3l,<<,%a2@\+&,%sp
    104c:	a2ae 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1
    1052:	a6ee 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3
    1058:	a4ae 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2
    105e:	aeee 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp
    1064:	a2ae 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1
    106a:	a6ee 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3
    1070:	a4ae 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2
    1076:	aeee 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp
    107c:	a2a1 3242      	macw %d2u,%d3l,<<,%a1@-,%d1
    1080:	a6e1 3242      	macw %d2u,%d3l,<<,%a1@-,%a3
    1084:	a4a1 3242      	macw %d2u,%d3l,<<,%a1@-,%d2
    1088:	aee1 3242      	macw %d2u,%d3l,<<,%a1@-,%sp
    108c:	a2a1 3262      	macw %d2u,%d3l,<<,%a1@-&,%d1
    1090:	a6e1 3262      	macw %d2u,%d3l,<<,%a1@-&,%a3
    1094:	a4a1 3262      	macw %d2u,%d3l,<<,%a1@-&,%d2
    1098:	aee1 3262      	macw %d2u,%d3l,<<,%a1@-&,%sp
    109c:	a293 3642      	macw %d2u,%d3l,>>,%a3@,%d1
    10a0:	a6d3 3642      	macw %d2u,%d3l,>>,%a3@,%a3
    10a4:	a493 3642      	macw %d2u,%d3l,>>,%a3@,%d2
    10a8:	aed3 3642      	macw %d2u,%d3l,>>,%a3@,%sp
    10ac:	a293 3662      	macw %d2u,%d3l,>>,%a3@&,%d1
    10b0:	a6d3 3662      	macw %d2u,%d3l,>>,%a3@&,%a3
    10b4:	a493 3662      	macw %d2u,%d3l,>>,%a3@&,%d2
    10b8:	aed3 3662      	macw %d2u,%d3l,>>,%a3@&,%sp
    10bc:	a29a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d1
    10c0:	a6da 3642      	macw %d2u,%d3l,>>,%a2@\+,%a3
    10c4:	a49a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d2
    10c8:	aeda 3642      	macw %d2u,%d3l,>>,%a2@\+,%sp
    10cc:	a29a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d1
    10d0:	a6da 3662      	macw %d2u,%d3l,>>,%a2@\+&,%a3
    10d4:	a49a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d2
    10d8:	aeda 3662      	macw %d2u,%d3l,>>,%a2@\+&,%sp
    10dc:	a2ae 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1
    10e2:	a6ee 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3
    10e8:	a4ae 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2
    10ee:	aeee 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp
    10f4:	a2ae 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1
    10fa:	a6ee 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3
    1100:	a4ae 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2
    1106:	aeee 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp
    110c:	a2a1 3642      	macw %d2u,%d3l,>>,%a1@-,%d1
    1110:	a6e1 3642      	macw %d2u,%d3l,>>,%a1@-,%a3
    1114:	a4a1 3642      	macw %d2u,%d3l,>>,%a1@-,%d2
    1118:	aee1 3642      	macw %d2u,%d3l,>>,%a1@-,%sp
    111c:	a2a1 3662      	macw %d2u,%d3l,>>,%a1@-&,%d1
    1120:	a6e1 3662      	macw %d2u,%d3l,>>,%a1@-&,%a3
    1124:	a4a1 3662      	macw %d2u,%d3l,>>,%a1@-&,%d2
    1128:	aee1 3662      	macw %d2u,%d3l,>>,%a1@-&,%sp
    112c:	a293 3242      	macw %d2u,%d3l,<<,%a3@,%d1
    1130:	a6d3 3242      	macw %d2u,%d3l,<<,%a3@,%a3
    1134:	a493 3242      	macw %d2u,%d3l,<<,%a3@,%d2
    1138:	aed3 3242      	macw %d2u,%d3l,<<,%a3@,%sp
    113c:	a293 3262      	macw %d2u,%d3l,<<,%a3@&,%d1
    1140:	a6d3 3262      	macw %d2u,%d3l,<<,%a3@&,%a3
    1144:	a493 3262      	macw %d2u,%d3l,<<,%a3@&,%d2
    1148:	aed3 3262      	macw %d2u,%d3l,<<,%a3@&,%sp
    114c:	a29a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d1
    1150:	a6da 3242      	macw %d2u,%d3l,<<,%a2@\+,%a3
    1154:	a49a 3242      	macw %d2u,%d3l,<<,%a2@\+,%d2
    1158:	aeda 3242      	macw %d2u,%d3l,<<,%a2@\+,%sp
    115c:	a29a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d1
    1160:	a6da 3262      	macw %d2u,%d3l,<<,%a2@\+&,%a3
    1164:	a49a 3262      	macw %d2u,%d3l,<<,%a2@\+&,%d2
    1168:	aeda 3262      	macw %d2u,%d3l,<<,%a2@\+&,%sp
    116c:	a2ae 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d1
    1172:	a6ee 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%a3
    1178:	a4ae 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%d2
    117e:	aeee 3242 000a 	macw %d2u,%d3l,<<,%fp@\(10\),%sp
    1184:	a2ae 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d1
    118a:	a6ee 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%a3
    1190:	a4ae 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%d2
    1196:	aeee 3262 000a 	macw %d2u,%d3l,<<,%fp@\(10\)&,%sp
    119c:	a2a1 3242      	macw %d2u,%d3l,<<,%a1@-,%d1
    11a0:	a6e1 3242      	macw %d2u,%d3l,<<,%a1@-,%a3
    11a4:	a4a1 3242      	macw %d2u,%d3l,<<,%a1@-,%d2
    11a8:	aee1 3242      	macw %d2u,%d3l,<<,%a1@-,%sp
    11ac:	a2a1 3262      	macw %d2u,%d3l,<<,%a1@-&,%d1
    11b0:	a6e1 3262      	macw %d2u,%d3l,<<,%a1@-&,%a3
    11b4:	a4a1 3262      	macw %d2u,%d3l,<<,%a1@-&,%d2
    11b8:	aee1 3262      	macw %d2u,%d3l,<<,%a1@-&,%sp
    11bc:	a293 3642      	macw %d2u,%d3l,>>,%a3@,%d1
    11c0:	a6d3 3642      	macw %d2u,%d3l,>>,%a3@,%a3
    11c4:	a493 3642      	macw %d2u,%d3l,>>,%a3@,%d2
    11c8:	aed3 3642      	macw %d2u,%d3l,>>,%a3@,%sp
    11cc:	a293 3662      	macw %d2u,%d3l,>>,%a3@&,%d1
    11d0:	a6d3 3662      	macw %d2u,%d3l,>>,%a3@&,%a3
    11d4:	a493 3662      	macw %d2u,%d3l,>>,%a3@&,%d2
    11d8:	aed3 3662      	macw %d2u,%d3l,>>,%a3@&,%sp
    11dc:	a29a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d1
    11e0:	a6da 3642      	macw %d2u,%d3l,>>,%a2@\+,%a3
    11e4:	a49a 3642      	macw %d2u,%d3l,>>,%a2@\+,%d2
    11e8:	aeda 3642      	macw %d2u,%d3l,>>,%a2@\+,%sp
    11ec:	a29a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d1
    11f0:	a6da 3662      	macw %d2u,%d3l,>>,%a2@\+&,%a3
    11f4:	a49a 3662      	macw %d2u,%d3l,>>,%a2@\+&,%d2
    11f8:	aeda 3662      	macw %d2u,%d3l,>>,%a2@\+&,%sp
    11fc:	a2ae 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d1
    1202:	a6ee 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%a3
    1208:	a4ae 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%d2
    120e:	aeee 3642 000a 	macw %d2u,%d3l,>>,%fp@\(10\),%sp
    1214:	a2ae 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d1
    121a:	a6ee 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%a3
    1220:	a4ae 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%d2
    1226:	aeee 3662 000a 	macw %d2u,%d3l,>>,%fp@\(10\)&,%sp
    122c:	a2a1 3642      	macw %d2u,%d3l,>>,%a1@-,%d1
    1230:	a6e1 3642      	macw %d2u,%d3l,>>,%a1@-,%a3
    1234:	a4a1 3642      	macw %d2u,%d3l,>>,%a1@-,%d2
    1238:	aee1 3642      	macw %d2u,%d3l,>>,%a1@-,%sp
    123c:	a2a1 3662      	macw %d2u,%d3l,>>,%a1@-&,%d1
    1240:	a6e1 3662      	macw %d2u,%d3l,>>,%a1@-&,%a3
    1244:	a4a1 3662      	macw %d2u,%d3l,>>,%a1@-&,%d2
    1248:	aee1 3662      	macw %d2u,%d3l,>>,%a1@-&,%sp
    124c:	a293 f0c2      	macw %d2u,%a7u,%a3@,%d1
    1250:	a6d3 f0c2      	macw %d2u,%a7u,%a3@,%a3
    1254:	a493 f0c2      	macw %d2u,%a7u,%a3@,%d2
    1258:	aed3 f0c2      	macw %d2u,%a7u,%a3@,%sp
    125c:	a293 f0e2      	macw %d2u,%a7u,%a3@&,%d1
    1260:	a6d3 f0e2      	macw %d2u,%a7u,%a3@&,%a3
    1264:	a493 f0e2      	macw %d2u,%a7u,%a3@&,%d2
    1268:	aed3 f0e2      	macw %d2u,%a7u,%a3@&,%sp
    126c:	a29a f0c2      	macw %d2u,%a7u,%a2@\+,%d1
    1270:	a6da f0c2      	macw %d2u,%a7u,%a2@\+,%a3
    1274:	a49a f0c2      	macw %d2u,%a7u,%a2@\+,%d2
    1278:	aeda f0c2      	macw %d2u,%a7u,%a2@\+,%sp
    127c:	a29a f0e2      	macw %d2u,%a7u,%a2@\+&,%d1
    1280:	a6da f0e2      	macw %d2u,%a7u,%a2@\+&,%a3
    1284:	a49a f0e2      	macw %d2u,%a7u,%a2@\+&,%d2
    1288:	aeda f0e2      	macw %d2u,%a7u,%a2@\+&,%sp
    128c:	a2ae f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%d1
    1292:	a6ee f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%a3
    1298:	a4ae f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%d2
    129e:	aeee f0c2 000a 	macw %d2u,%a7u,%fp@\(10\),%sp
    12a4:	a2ae f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d1
    12aa:	a6ee f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%a3
    12b0:	a4ae f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%d2
    12b6:	aeee f0e2 000a 	macw %d2u,%a7u,%fp@\(10\)&,%sp
    12bc:	a2a1 f0c2      	macw %d2u,%a7u,%a1@-,%d1
    12c0:	a6e1 f0c2      	macw %d2u,%a7u,%a1@-,%a3
    12c4:	a4a1 f0c2      	macw %d2u,%a7u,%a1@-,%d2
    12c8:	aee1 f0c2      	macw %d2u,%a7u,%a1@-,%sp
    12cc:	a2a1 f0e2      	macw %d2u,%a7u,%a1@-&,%d1
    12d0:	a6e1 f0e2      	macw %d2u,%a7u,%a1@-&,%a3
    12d4:	a4a1 f0e2      	macw %d2u,%a7u,%a1@-&,%d2
    12d8:	aee1 f0e2      	macw %d2u,%a7u,%a1@-&,%sp
    12dc:	a293 f2c2      	macw %d2u,%a7u,<<,%a3@,%d1
    12e0:	a6d3 f2c2      	macw %d2u,%a7u,<<,%a3@,%a3
    12e4:	a493 f2c2      	macw %d2u,%a7u,<<,%a3@,%d2
    12e8:	aed3 f2c2      	macw %d2u,%a7u,<<,%a3@,%sp
    12ec:	a293 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d1
    12f0:	a6d3 f2e2      	macw %d2u,%a7u,<<,%a3@&,%a3
    12f4:	a493 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d2
    12f8:	aed3 f2e2      	macw %d2u,%a7u,<<,%a3@&,%sp
    12fc:	a29a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d1
    1300:	a6da f2c2      	macw %d2u,%a7u,<<,%a2@\+,%a3
    1304:	a49a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d2
    1308:	aeda f2c2      	macw %d2u,%a7u,<<,%a2@\+,%sp
    130c:	a29a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d1
    1310:	a6da f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%a3
    1314:	a49a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d2
    1318:	aeda f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%sp
    131c:	a2ae f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1
    1322:	a6ee f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3
    1328:	a4ae f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2
    132e:	aeee f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp
    1334:	a2ae f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1
    133a:	a6ee f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3
    1340:	a4ae f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2
    1346:	aeee f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp
    134c:	a2a1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d1
    1350:	a6e1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%a3
    1354:	a4a1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d2
    1358:	aee1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%sp
    135c:	a2a1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d1
    1360:	a6e1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%a3
    1364:	a4a1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d2
    1368:	aee1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%sp
    136c:	a293 f6c2      	macw %d2u,%a7u,>>,%a3@,%d1
    1370:	a6d3 f6c2      	macw %d2u,%a7u,>>,%a3@,%a3
    1374:	a493 f6c2      	macw %d2u,%a7u,>>,%a3@,%d2
    1378:	aed3 f6c2      	macw %d2u,%a7u,>>,%a3@,%sp
    137c:	a293 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d1
    1380:	a6d3 f6e2      	macw %d2u,%a7u,>>,%a3@&,%a3
    1384:	a493 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d2
    1388:	aed3 f6e2      	macw %d2u,%a7u,>>,%a3@&,%sp
    138c:	a29a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d1
    1390:	a6da f6c2      	macw %d2u,%a7u,>>,%a2@\+,%a3
    1394:	a49a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d2
    1398:	aeda f6c2      	macw %d2u,%a7u,>>,%a2@\+,%sp
    139c:	a29a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d1
    13a0:	a6da f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%a3
    13a4:	a49a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d2
    13a8:	aeda f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%sp
    13ac:	a2ae f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1
    13b2:	a6ee f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3
    13b8:	a4ae f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2
    13be:	aeee f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp
    13c4:	a2ae f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1
    13ca:	a6ee f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3
    13d0:	a4ae f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2
    13d6:	aeee f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp
    13dc:	a2a1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d1
    13e0:	a6e1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%a3
    13e4:	a4a1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d2
    13e8:	aee1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%sp
    13ec:	a2a1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d1
    13f0:	a6e1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%a3
    13f4:	a4a1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d2
    13f8:	aee1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%sp
    13fc:	a293 f2c2      	macw %d2u,%a7u,<<,%a3@,%d1
    1400:	a6d3 f2c2      	macw %d2u,%a7u,<<,%a3@,%a3
    1404:	a493 f2c2      	macw %d2u,%a7u,<<,%a3@,%d2
    1408:	aed3 f2c2      	macw %d2u,%a7u,<<,%a3@,%sp
    140c:	a293 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d1
    1410:	a6d3 f2e2      	macw %d2u,%a7u,<<,%a3@&,%a3
    1414:	a493 f2e2      	macw %d2u,%a7u,<<,%a3@&,%d2
    1418:	aed3 f2e2      	macw %d2u,%a7u,<<,%a3@&,%sp
    141c:	a29a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d1
    1420:	a6da f2c2      	macw %d2u,%a7u,<<,%a2@\+,%a3
    1424:	a49a f2c2      	macw %d2u,%a7u,<<,%a2@\+,%d2
    1428:	aeda f2c2      	macw %d2u,%a7u,<<,%a2@\+,%sp
    142c:	a29a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d1
    1430:	a6da f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%a3
    1434:	a49a f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%d2
    1438:	aeda f2e2      	macw %d2u,%a7u,<<,%a2@\+&,%sp
    143c:	a2ae f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d1
    1442:	a6ee f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%a3
    1448:	a4ae f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%d2
    144e:	aeee f2c2 000a 	macw %d2u,%a7u,<<,%fp@\(10\),%sp
    1454:	a2ae f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d1
    145a:	a6ee f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%a3
    1460:	a4ae f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%d2
    1466:	aeee f2e2 000a 	macw %d2u,%a7u,<<,%fp@\(10\)&,%sp
    146c:	a2a1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d1
    1470:	a6e1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%a3
    1474:	a4a1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%d2
    1478:	aee1 f2c2      	macw %d2u,%a7u,<<,%a1@-,%sp
    147c:	a2a1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d1
    1480:	a6e1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%a3
    1484:	a4a1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%d2
    1488:	aee1 f2e2      	macw %d2u,%a7u,<<,%a1@-&,%sp
    148c:	a293 f6c2      	macw %d2u,%a7u,>>,%a3@,%d1
    1490:	a6d3 f6c2      	macw %d2u,%a7u,>>,%a3@,%a3
    1494:	a493 f6c2      	macw %d2u,%a7u,>>,%a3@,%d2
    1498:	aed3 f6c2      	macw %d2u,%a7u,>>,%a3@,%sp
    149c:	a293 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d1
    14a0:	a6d3 f6e2      	macw %d2u,%a7u,>>,%a3@&,%a3
    14a4:	a493 f6e2      	macw %d2u,%a7u,>>,%a3@&,%d2
    14a8:	aed3 f6e2      	macw %d2u,%a7u,>>,%a3@&,%sp
    14ac:	a29a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d1
    14b0:	a6da f6c2      	macw %d2u,%a7u,>>,%a2@\+,%a3
    14b4:	a49a f6c2      	macw %d2u,%a7u,>>,%a2@\+,%d2
    14b8:	aeda f6c2      	macw %d2u,%a7u,>>,%a2@\+,%sp
    14bc:	a29a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d1
    14c0:	a6da f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%a3
    14c4:	a49a f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%d2
    14c8:	aeda f6e2      	macw %d2u,%a7u,>>,%a2@\+&,%sp
    14cc:	a2ae f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d1
    14d2:	a6ee f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%a3
    14d8:	a4ae f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%d2
    14de:	aeee f6c2 000a 	macw %d2u,%a7u,>>,%fp@\(10\),%sp
    14e4:	a2ae f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d1
    14ea:	a6ee f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%a3
    14f0:	a4ae f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%d2
    14f6:	aeee f6e2 000a 	macw %d2u,%a7u,>>,%fp@\(10\)&,%sp
    14fc:	a2a1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d1
    1500:	a6e1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%a3
    1504:	a4a1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%d2
    1508:	aee1 f6c2      	macw %d2u,%a7u,>>,%a1@-,%sp
    150c:	a2a1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d1
    1510:	a6e1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%a3
    1514:	a4a1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%d2
    1518:	aee1 f6e2      	macw %d2u,%a7u,>>,%a1@-&,%sp
    151c:	a293 1042      	macw %d2u,%d1l,%a3@,%d1
    1520:	a6d3 1042      	macw %d2u,%d1l,%a3@,%a3
    1524:	a493 1042      	macw %d2u,%d1l,%a3@,%d2
    1528:	aed3 1042      	macw %d2u,%d1l,%a3@,%sp
    152c:	a293 1062      	macw %d2u,%d1l,%a3@&,%d1
    1530:	a6d3 1062      	macw %d2u,%d1l,%a3@&,%a3
    1534:	a493 1062      	macw %d2u,%d1l,%a3@&,%d2
    1538:	aed3 1062      	macw %d2u,%d1l,%a3@&,%sp
    153c:	a29a 1042      	macw %d2u,%d1l,%a2@\+,%d1
    1540:	a6da 1042      	macw %d2u,%d1l,%a2@\+,%a3
    1544:	a49a 1042      	macw %d2u,%d1l,%a2@\+,%d2
    1548:	aeda 1042      	macw %d2u,%d1l,%a2@\+,%sp
    154c:	a29a 1062      	macw %d2u,%d1l,%a2@\+&,%d1
    1550:	a6da 1062      	macw %d2u,%d1l,%a2@\+&,%a3
    1554:	a49a 1062      	macw %d2u,%d1l,%a2@\+&,%d2
    1558:	aeda 1062      	macw %d2u,%d1l,%a2@\+&,%sp
    155c:	a2ae 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%d1
    1562:	a6ee 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%a3
    1568:	a4ae 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%d2
    156e:	aeee 1042 000a 	macw %d2u,%d1l,%fp@\(10\),%sp
    1574:	a2ae 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d1
    157a:	a6ee 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%a3
    1580:	a4ae 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%d2
    1586:	aeee 1062 000a 	macw %d2u,%d1l,%fp@\(10\)&,%sp
    158c:	a2a1 1042      	macw %d2u,%d1l,%a1@-,%d1
    1590:	a6e1 1042      	macw %d2u,%d1l,%a1@-,%a3
    1594:	a4a1 1042      	macw %d2u,%d1l,%a1@-,%d2
    1598:	aee1 1042      	macw %d2u,%d1l,%a1@-,%sp
    159c:	a2a1 1062      	macw %d2u,%d1l,%a1@-&,%d1
    15a0:	a6e1 1062      	macw %d2u,%d1l,%a1@-&,%a3
    15a4:	a4a1 1062      	macw %d2u,%d1l,%a1@-&,%d2
    15a8:	aee1 1062      	macw %d2u,%d1l,%a1@-&,%sp
    15ac:	a293 1242      	macw %d2u,%d1l,<<,%a3@,%d1
    15b0:	a6d3 1242      	macw %d2u,%d1l,<<,%a3@,%a3
    15b4:	a493 1242      	macw %d2u,%d1l,<<,%a3@,%d2
    15b8:	aed3 1242      	macw %d2u,%d1l,<<,%a3@,%sp
    15bc:	a293 1262      	macw %d2u,%d1l,<<,%a3@&,%d1
    15c0:	a6d3 1262      	macw %d2u,%d1l,<<,%a3@&,%a3
    15c4:	a493 1262      	macw %d2u,%d1l,<<,%a3@&,%d2
    15c8:	aed3 1262      	macw %d2u,%d1l,<<,%a3@&,%sp
    15cc:	a29a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d1
    15d0:	a6da 1242      	macw %d2u,%d1l,<<,%a2@\+,%a3
    15d4:	a49a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d2
    15d8:	aeda 1242      	macw %d2u,%d1l,<<,%a2@\+,%sp
    15dc:	a29a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d1
    15e0:	a6da 1262      	macw %d2u,%d1l,<<,%a2@\+&,%a3
    15e4:	a49a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d2
    15e8:	aeda 1262      	macw %d2u,%d1l,<<,%a2@\+&,%sp
    15ec:	a2ae 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1
    15f2:	a6ee 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3
    15f8:	a4ae 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2
    15fe:	aeee 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp
    1604:	a2ae 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1
    160a:	a6ee 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3
    1610:	a4ae 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2
    1616:	aeee 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp
    161c:	a2a1 1242      	macw %d2u,%d1l,<<,%a1@-,%d1
    1620:	a6e1 1242      	macw %d2u,%d1l,<<,%a1@-,%a3
    1624:	a4a1 1242      	macw %d2u,%d1l,<<,%a1@-,%d2
    1628:	aee1 1242      	macw %d2u,%d1l,<<,%a1@-,%sp
    162c:	a2a1 1262      	macw %d2u,%d1l,<<,%a1@-&,%d1
    1630:	a6e1 1262      	macw %d2u,%d1l,<<,%a1@-&,%a3
    1634:	a4a1 1262      	macw %d2u,%d1l,<<,%a1@-&,%d2
    1638:	aee1 1262      	macw %d2u,%d1l,<<,%a1@-&,%sp
    163c:	a293 1642      	macw %d2u,%d1l,>>,%a3@,%d1
    1640:	a6d3 1642      	macw %d2u,%d1l,>>,%a3@,%a3
    1644:	a493 1642      	macw %d2u,%d1l,>>,%a3@,%d2
    1648:	aed3 1642      	macw %d2u,%d1l,>>,%a3@,%sp
    164c:	a293 1662      	macw %d2u,%d1l,>>,%a3@&,%d1
    1650:	a6d3 1662      	macw %d2u,%d1l,>>,%a3@&,%a3
    1654:	a493 1662      	macw %d2u,%d1l,>>,%a3@&,%d2
    1658:	aed3 1662      	macw %d2u,%d1l,>>,%a3@&,%sp
    165c:	a29a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d1
    1660:	a6da 1642      	macw %d2u,%d1l,>>,%a2@\+,%a3
    1664:	a49a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d2
    1668:	aeda 1642      	macw %d2u,%d1l,>>,%a2@\+,%sp
    166c:	a29a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d1
    1670:	a6da 1662      	macw %d2u,%d1l,>>,%a2@\+&,%a3
    1674:	a49a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d2
    1678:	aeda 1662      	macw %d2u,%d1l,>>,%a2@\+&,%sp
    167c:	a2ae 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1
    1682:	a6ee 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3
    1688:	a4ae 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2
    168e:	aeee 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp
    1694:	a2ae 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1
    169a:	a6ee 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3
    16a0:	a4ae 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2
    16a6:	aeee 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp
    16ac:	a2a1 1642      	macw %d2u,%d1l,>>,%a1@-,%d1
    16b0:	a6e1 1642      	macw %d2u,%d1l,>>,%a1@-,%a3
    16b4:	a4a1 1642      	macw %d2u,%d1l,>>,%a1@-,%d2
    16b8:	aee1 1642      	macw %d2u,%d1l,>>,%a1@-,%sp
    16bc:	a2a1 1662      	macw %d2u,%d1l,>>,%a1@-&,%d1
    16c0:	a6e1 1662      	macw %d2u,%d1l,>>,%a1@-&,%a3
    16c4:	a4a1 1662      	macw %d2u,%d1l,>>,%a1@-&,%d2
    16c8:	aee1 1662      	macw %d2u,%d1l,>>,%a1@-&,%sp
    16cc:	a293 1242      	macw %d2u,%d1l,<<,%a3@,%d1
    16d0:	a6d3 1242      	macw %d2u,%d1l,<<,%a3@,%a3
    16d4:	a493 1242      	macw %d2u,%d1l,<<,%a3@,%d2
    16d8:	aed3 1242      	macw %d2u,%d1l,<<,%a3@,%sp
    16dc:	a293 1262      	macw %d2u,%d1l,<<,%a3@&,%d1
    16e0:	a6d3 1262      	macw %d2u,%d1l,<<,%a3@&,%a3
    16e4:	a493 1262      	macw %d2u,%d1l,<<,%a3@&,%d2
    16e8:	aed3 1262      	macw %d2u,%d1l,<<,%a3@&,%sp
    16ec:	a29a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d1
    16f0:	a6da 1242      	macw %d2u,%d1l,<<,%a2@\+,%a3
    16f4:	a49a 1242      	macw %d2u,%d1l,<<,%a2@\+,%d2
    16f8:	aeda 1242      	macw %d2u,%d1l,<<,%a2@\+,%sp
    16fc:	a29a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d1
    1700:	a6da 1262      	macw %d2u,%d1l,<<,%a2@\+&,%a3
    1704:	a49a 1262      	macw %d2u,%d1l,<<,%a2@\+&,%d2
    1708:	aeda 1262      	macw %d2u,%d1l,<<,%a2@\+&,%sp
    170c:	a2ae 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d1
    1712:	a6ee 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%a3
    1718:	a4ae 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%d2
    171e:	aeee 1242 000a 	macw %d2u,%d1l,<<,%fp@\(10\),%sp
    1724:	a2ae 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d1
    172a:	a6ee 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%a3
    1730:	a4ae 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%d2
    1736:	aeee 1262 000a 	macw %d2u,%d1l,<<,%fp@\(10\)&,%sp
    173c:	a2a1 1242      	macw %d2u,%d1l,<<,%a1@-,%d1
    1740:	a6e1 1242      	macw %d2u,%d1l,<<,%a1@-,%a3
    1744:	a4a1 1242      	macw %d2u,%d1l,<<,%a1@-,%d2
    1748:	aee1 1242      	macw %d2u,%d1l,<<,%a1@-,%sp
    174c:	a2a1 1262      	macw %d2u,%d1l,<<,%a1@-&,%d1
    1750:	a6e1 1262      	macw %d2u,%d1l,<<,%a1@-&,%a3
    1754:	a4a1 1262      	macw %d2u,%d1l,<<,%a1@-&,%d2
    1758:	aee1 1262      	macw %d2u,%d1l,<<,%a1@-&,%sp
    175c:	a293 1642      	macw %d2u,%d1l,>>,%a3@,%d1
    1760:	a6d3 1642      	macw %d2u,%d1l,>>,%a3@,%a3
    1764:	a493 1642      	macw %d2u,%d1l,>>,%a3@,%d2
    1768:	aed3 1642      	macw %d2u,%d1l,>>,%a3@,%sp
    176c:	a293 1662      	macw %d2u,%d1l,>>,%a3@&,%d1
    1770:	a6d3 1662      	macw %d2u,%d1l,>>,%a3@&,%a3
    1774:	a493 1662      	macw %d2u,%d1l,>>,%a3@&,%d2
    1778:	aed3 1662      	macw %d2u,%d1l,>>,%a3@&,%sp
    177c:	a29a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d1
    1780:	a6da 1642      	macw %d2u,%d1l,>>,%a2@\+,%a3
    1784:	a49a 1642      	macw %d2u,%d1l,>>,%a2@\+,%d2
    1788:	aeda 1642      	macw %d2u,%d1l,>>,%a2@\+,%sp
    178c:	a29a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d1
    1790:	a6da 1662      	macw %d2u,%d1l,>>,%a2@\+&,%a3
    1794:	a49a 1662      	macw %d2u,%d1l,>>,%a2@\+&,%d2
    1798:	aeda 1662      	macw %d2u,%d1l,>>,%a2@\+&,%sp
    179c:	a2ae 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d1
    17a2:	a6ee 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%a3
    17a8:	a4ae 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%d2
    17ae:	aeee 1642 000a 	macw %d2u,%d1l,>>,%fp@\(10\),%sp
    17b4:	a2ae 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d1
    17ba:	a6ee 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%a3
    17c0:	a4ae 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%d2
    17c6:	aeee 1662 000a 	macw %d2u,%d1l,>>,%fp@\(10\)&,%sp
    17cc:	a2a1 1642      	macw %d2u,%d1l,>>,%a1@-,%d1
    17d0:	a6e1 1642      	macw %d2u,%d1l,>>,%a1@-,%a3
    17d4:	a4a1 1642      	macw %d2u,%d1l,>>,%a1@-,%d2
    17d8:	aee1 1642      	macw %d2u,%d1l,>>,%a1@-,%sp
    17dc:	a2a1 1662      	macw %d2u,%d1l,>>,%a1@-&,%d1
    17e0:	a6e1 1662      	macw %d2u,%d1l,>>,%a1@-&,%a3
    17e4:	a4a1 1662      	macw %d2u,%d1l,>>,%a1@-&,%d2
    17e8:	aee1 1662      	macw %d2u,%d1l,>>,%a1@-&,%sp
    17ec:	a293 a08d      	macw %a5l,%a2u,%a3@,%d1
    17f0:	a6d3 a08d      	macw %a5l,%a2u,%a3@,%a3
    17f4:	a493 a08d      	macw %a5l,%a2u,%a3@,%d2
    17f8:	aed3 a08d      	macw %a5l,%a2u,%a3@,%sp
    17fc:	a293 a0ad      	macw %a5l,%a2u,%a3@&,%d1
    1800:	a6d3 a0ad      	macw %a5l,%a2u,%a3@&,%a3
    1804:	a493 a0ad      	macw %a5l,%a2u,%a3@&,%d2
    1808:	aed3 a0ad      	macw %a5l,%a2u,%a3@&,%sp
    180c:	a29a a08d      	macw %a5l,%a2u,%a2@\+,%d1
    1810:	a6da a08d      	macw %a5l,%a2u,%a2@\+,%a3
    1814:	a49a a08d      	macw %a5l,%a2u,%a2@\+,%d2
    1818:	aeda a08d      	macw %a5l,%a2u,%a2@\+,%sp
    181c:	a29a a0ad      	macw %a5l,%a2u,%a2@\+&,%d1
    1820:	a6da a0ad      	macw %a5l,%a2u,%a2@\+&,%a3
    1824:	a49a a0ad      	macw %a5l,%a2u,%a2@\+&,%d2
    1828:	aeda a0ad      	macw %a5l,%a2u,%a2@\+&,%sp
    182c:	a2ae a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%d1
    1832:	a6ee a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%a3
    1838:	a4ae a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%d2
    183e:	aeee a08d 000a 	macw %a5l,%a2u,%fp@\(10\),%sp
    1844:	a2ae a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d1
    184a:	a6ee a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%a3
    1850:	a4ae a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%d2
    1856:	aeee a0ad 000a 	macw %a5l,%a2u,%fp@\(10\)&,%sp
    185c:	a2a1 a08d      	macw %a5l,%a2u,%a1@-,%d1
    1860:	a6e1 a08d      	macw %a5l,%a2u,%a1@-,%a3
    1864:	a4a1 a08d      	macw %a5l,%a2u,%a1@-,%d2
    1868:	aee1 a08d      	macw %a5l,%a2u,%a1@-,%sp
    186c:	a2a1 a0ad      	macw %a5l,%a2u,%a1@-&,%d1
    1870:	a6e1 a0ad      	macw %a5l,%a2u,%a1@-&,%a3
    1874:	a4a1 a0ad      	macw %a5l,%a2u,%a1@-&,%d2
    1878:	aee1 a0ad      	macw %a5l,%a2u,%a1@-&,%sp
    187c:	a293 a28d      	macw %a5l,%a2u,<<,%a3@,%d1
    1880:	a6d3 a28d      	macw %a5l,%a2u,<<,%a3@,%a3
    1884:	a493 a28d      	macw %a5l,%a2u,<<,%a3@,%d2
    1888:	aed3 a28d      	macw %a5l,%a2u,<<,%a3@,%sp
    188c:	a293 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d1
    1890:	a6d3 a2ad      	macw %a5l,%a2u,<<,%a3@&,%a3
    1894:	a493 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d2
    1898:	aed3 a2ad      	macw %a5l,%a2u,<<,%a3@&,%sp
    189c:	a29a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d1
    18a0:	a6da a28d      	macw %a5l,%a2u,<<,%a2@\+,%a3
    18a4:	a49a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d2
    18a8:	aeda a28d      	macw %a5l,%a2u,<<,%a2@\+,%sp
    18ac:	a29a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d1
    18b0:	a6da a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%a3
    18b4:	a49a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d2
    18b8:	aeda a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%sp
    18bc:	a2ae a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1
    18c2:	a6ee a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3
    18c8:	a4ae a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2
    18ce:	aeee a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp
    18d4:	a2ae a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1
    18da:	a6ee a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3
    18e0:	a4ae a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2
    18e6:	aeee a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp
    18ec:	a2a1 a28d      	macw %a5l,%a2u,<<,%a1@-,%d1
    18f0:	a6e1 a28d      	macw %a5l,%a2u,<<,%a1@-,%a3
    18f4:	a4a1 a28d      	macw %a5l,%a2u,<<,%a1@-,%d2
    18f8:	aee1 a28d      	macw %a5l,%a2u,<<,%a1@-,%sp
    18fc:	a2a1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d1
    1900:	a6e1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%a3
    1904:	a4a1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d2
    1908:	aee1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%sp
    190c:	a293 a68d      	macw %a5l,%a2u,>>,%a3@,%d1
    1910:	a6d3 a68d      	macw %a5l,%a2u,>>,%a3@,%a3
    1914:	a493 a68d      	macw %a5l,%a2u,>>,%a3@,%d2
    1918:	aed3 a68d      	macw %a5l,%a2u,>>,%a3@,%sp
    191c:	a293 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d1
    1920:	a6d3 a6ad      	macw %a5l,%a2u,>>,%a3@&,%a3
    1924:	a493 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d2
    1928:	aed3 a6ad      	macw %a5l,%a2u,>>,%a3@&,%sp
    192c:	a29a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d1
    1930:	a6da a68d      	macw %a5l,%a2u,>>,%a2@\+,%a3
    1934:	a49a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d2
    1938:	aeda a68d      	macw %a5l,%a2u,>>,%a2@\+,%sp
    193c:	a29a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d1
    1940:	a6da a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%a3
    1944:	a49a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d2
    1948:	aeda a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%sp
    194c:	a2ae a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1
    1952:	a6ee a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3
    1958:	a4ae a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2
    195e:	aeee a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp
    1964:	a2ae a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1
    196a:	a6ee a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3
    1970:	a4ae a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2
    1976:	aeee a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp
    197c:	a2a1 a68d      	macw %a5l,%a2u,>>,%a1@-,%d1
    1980:	a6e1 a68d      	macw %a5l,%a2u,>>,%a1@-,%a3
    1984:	a4a1 a68d      	macw %a5l,%a2u,>>,%a1@-,%d2
    1988:	aee1 a68d      	macw %a5l,%a2u,>>,%a1@-,%sp
    198c:	a2a1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d1
    1990:	a6e1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%a3
    1994:	a4a1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d2
    1998:	aee1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%sp
    199c:	a293 a28d      	macw %a5l,%a2u,<<,%a3@,%d1
    19a0:	a6d3 a28d      	macw %a5l,%a2u,<<,%a3@,%a3
    19a4:	a493 a28d      	macw %a5l,%a2u,<<,%a3@,%d2
    19a8:	aed3 a28d      	macw %a5l,%a2u,<<,%a3@,%sp
    19ac:	a293 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d1
    19b0:	a6d3 a2ad      	macw %a5l,%a2u,<<,%a3@&,%a3
    19b4:	a493 a2ad      	macw %a5l,%a2u,<<,%a3@&,%d2
    19b8:	aed3 a2ad      	macw %a5l,%a2u,<<,%a3@&,%sp
    19bc:	a29a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d1
    19c0:	a6da a28d      	macw %a5l,%a2u,<<,%a2@\+,%a3
    19c4:	a49a a28d      	macw %a5l,%a2u,<<,%a2@\+,%d2
    19c8:	aeda a28d      	macw %a5l,%a2u,<<,%a2@\+,%sp
    19cc:	a29a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d1
    19d0:	a6da a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%a3
    19d4:	a49a a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%d2
    19d8:	aeda a2ad      	macw %a5l,%a2u,<<,%a2@\+&,%sp
    19dc:	a2ae a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d1
    19e2:	a6ee a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%a3
    19e8:	a4ae a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%d2
    19ee:	aeee a28d 000a 	macw %a5l,%a2u,<<,%fp@\(10\),%sp
    19f4:	a2ae a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d1
    19fa:	a6ee a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%a3
    1a00:	a4ae a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%d2
    1a06:	aeee a2ad 000a 	macw %a5l,%a2u,<<,%fp@\(10\)&,%sp
    1a0c:	a2a1 a28d      	macw %a5l,%a2u,<<,%a1@-,%d1
    1a10:	a6e1 a28d      	macw %a5l,%a2u,<<,%a1@-,%a3
    1a14:	a4a1 a28d      	macw %a5l,%a2u,<<,%a1@-,%d2
    1a18:	aee1 a28d      	macw %a5l,%a2u,<<,%a1@-,%sp
    1a1c:	a2a1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d1
    1a20:	a6e1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%a3
    1a24:	a4a1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%d2
    1a28:	aee1 a2ad      	macw %a5l,%a2u,<<,%a1@-&,%sp
    1a2c:	a293 a68d      	macw %a5l,%a2u,>>,%a3@,%d1
    1a30:	a6d3 a68d      	macw %a5l,%a2u,>>,%a3@,%a3
    1a34:	a493 a68d      	macw %a5l,%a2u,>>,%a3@,%d2
    1a38:	aed3 a68d      	macw %a5l,%a2u,>>,%a3@,%sp
    1a3c:	a293 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d1
    1a40:	a6d3 a6ad      	macw %a5l,%a2u,>>,%a3@&,%a3
    1a44:	a493 a6ad      	macw %a5l,%a2u,>>,%a3@&,%d2
    1a48:	aed3 a6ad      	macw %a5l,%a2u,>>,%a3@&,%sp
    1a4c:	a29a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d1
    1a50:	a6da a68d      	macw %a5l,%a2u,>>,%a2@\+,%a3
    1a54:	a49a a68d      	macw %a5l,%a2u,>>,%a2@\+,%d2
    1a58:	aeda a68d      	macw %a5l,%a2u,>>,%a2@\+,%sp
    1a5c:	a29a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d1
    1a60:	a6da a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%a3
    1a64:	a49a a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%d2
    1a68:	aeda a6ad      	macw %a5l,%a2u,>>,%a2@\+&,%sp
    1a6c:	a2ae a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d1
    1a72:	a6ee a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%a3
    1a78:	a4ae a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%d2
    1a7e:	aeee a68d 000a 	macw %a5l,%a2u,>>,%fp@\(10\),%sp
    1a84:	a2ae a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d1
    1a8a:	a6ee a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%a3
    1a90:	a4ae a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%d2
    1a96:	aeee a6ad 000a 	macw %a5l,%a2u,>>,%fp@\(10\)&,%sp
    1a9c:	a2a1 a68d      	macw %a5l,%a2u,>>,%a1@-,%d1
    1aa0:	a6e1 a68d      	macw %a5l,%a2u,>>,%a1@-,%a3
    1aa4:	a4a1 a68d      	macw %a5l,%a2u,>>,%a1@-,%d2
    1aa8:	aee1 a68d      	macw %a5l,%a2u,>>,%a1@-,%sp
    1aac:	a2a1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d1
    1ab0:	a6e1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%a3
    1ab4:	a4a1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%d2
    1ab8:	aee1 a6ad      	macw %a5l,%a2u,>>,%a1@-&,%sp
    1abc:	a293 300d      	macw %a5l,%d3l,%a3@,%d1
    1ac0:	a6d3 300d      	macw %a5l,%d3l,%a3@,%a3
    1ac4:	a493 300d      	macw %a5l,%d3l,%a3@,%d2
    1ac8:	aed3 300d      	macw %a5l,%d3l,%a3@,%sp
    1acc:	a293 302d      	macw %a5l,%d3l,%a3@&,%d1
    1ad0:	a6d3 302d      	macw %a5l,%d3l,%a3@&,%a3
    1ad4:	a493 302d      	macw %a5l,%d3l,%a3@&,%d2
    1ad8:	aed3 302d      	macw %a5l,%d3l,%a3@&,%sp
    1adc:	a29a 300d      	macw %a5l,%d3l,%a2@\+,%d1
    1ae0:	a6da 300d      	macw %a5l,%d3l,%a2@\+,%a3
    1ae4:	a49a 300d      	macw %a5l,%d3l,%a2@\+,%d2
    1ae8:	aeda 300d      	macw %a5l,%d3l,%a2@\+,%sp
    1aec:	a29a 302d      	macw %a5l,%d3l,%a2@\+&,%d1
    1af0:	a6da 302d      	macw %a5l,%d3l,%a2@\+&,%a3
    1af4:	a49a 302d      	macw %a5l,%d3l,%a2@\+&,%d2
    1af8:	aeda 302d      	macw %a5l,%d3l,%a2@\+&,%sp
    1afc:	a2ae 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%d1
    1b02:	a6ee 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%a3
    1b08:	a4ae 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%d2
    1b0e:	aeee 300d 000a 	macw %a5l,%d3l,%fp@\(10\),%sp
    1b14:	a2ae 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d1
    1b1a:	a6ee 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%a3
    1b20:	a4ae 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%d2
    1b26:	aeee 302d 000a 	macw %a5l,%d3l,%fp@\(10\)&,%sp
    1b2c:	a2a1 300d      	macw %a5l,%d3l,%a1@-,%d1
    1b30:	a6e1 300d      	macw %a5l,%d3l,%a1@-,%a3
    1b34:	a4a1 300d      	macw %a5l,%d3l,%a1@-,%d2
    1b38:	aee1 300d      	macw %a5l,%d3l,%a1@-,%sp
    1b3c:	a2a1 302d      	macw %a5l,%d3l,%a1@-&,%d1
    1b40:	a6e1 302d      	macw %a5l,%d3l,%a1@-&,%a3
    1b44:	a4a1 302d      	macw %a5l,%d3l,%a1@-&,%d2
    1b48:	aee1 302d      	macw %a5l,%d3l,%a1@-&,%sp
    1b4c:	a293 320d      	macw %a5l,%d3l,<<,%a3@,%d1
    1b50:	a6d3 320d      	macw %a5l,%d3l,<<,%a3@,%a3
    1b54:	a493 320d      	macw %a5l,%d3l,<<,%a3@,%d2
    1b58:	aed3 320d      	macw %a5l,%d3l,<<,%a3@,%sp
    1b5c:	a293 322d      	macw %a5l,%d3l,<<,%a3@&,%d1
    1b60:	a6d3 322d      	macw %a5l,%d3l,<<,%a3@&,%a3
    1b64:	a493 322d      	macw %a5l,%d3l,<<,%a3@&,%d2
    1b68:	aed3 322d      	macw %a5l,%d3l,<<,%a3@&,%sp
    1b6c:	a29a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d1
    1b70:	a6da 320d      	macw %a5l,%d3l,<<,%a2@\+,%a3
    1b74:	a49a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d2
    1b78:	aeda 320d      	macw %a5l,%d3l,<<,%a2@\+,%sp
    1b7c:	a29a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d1
    1b80:	a6da 322d      	macw %a5l,%d3l,<<,%a2@\+&,%a3
    1b84:	a49a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d2
    1b88:	aeda 322d      	macw %a5l,%d3l,<<,%a2@\+&,%sp
    1b8c:	a2ae 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1
    1b92:	a6ee 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3
    1b98:	a4ae 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2
    1b9e:	aeee 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp
    1ba4:	a2ae 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1
    1baa:	a6ee 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3
    1bb0:	a4ae 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2
    1bb6:	aeee 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp
    1bbc:	a2a1 320d      	macw %a5l,%d3l,<<,%a1@-,%d1
    1bc0:	a6e1 320d      	macw %a5l,%d3l,<<,%a1@-,%a3
    1bc4:	a4a1 320d      	macw %a5l,%d3l,<<,%a1@-,%d2
    1bc8:	aee1 320d      	macw %a5l,%d3l,<<,%a1@-,%sp
    1bcc:	a2a1 322d      	macw %a5l,%d3l,<<,%a1@-&,%d1
    1bd0:	a6e1 322d      	macw %a5l,%d3l,<<,%a1@-&,%a3
    1bd4:	a4a1 322d      	macw %a5l,%d3l,<<,%a1@-&,%d2
    1bd8:	aee1 322d      	macw %a5l,%d3l,<<,%a1@-&,%sp
    1bdc:	a293 360d      	macw %a5l,%d3l,>>,%a3@,%d1
    1be0:	a6d3 360d      	macw %a5l,%d3l,>>,%a3@,%a3
    1be4:	a493 360d      	macw %a5l,%d3l,>>,%a3@,%d2
    1be8:	aed3 360d      	macw %a5l,%d3l,>>,%a3@,%sp
    1bec:	a293 362d      	macw %a5l,%d3l,>>,%a3@&,%d1
    1bf0:	a6d3 362d      	macw %a5l,%d3l,>>,%a3@&,%a3
    1bf4:	a493 362d      	macw %a5l,%d3l,>>,%a3@&,%d2
    1bf8:	aed3 362d      	macw %a5l,%d3l,>>,%a3@&,%sp
    1bfc:	a29a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d1
    1c00:	a6da 360d      	macw %a5l,%d3l,>>,%a2@\+,%a3
    1c04:	a49a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d2
    1c08:	aeda 360d      	macw %a5l,%d3l,>>,%a2@\+,%sp
    1c0c:	a29a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d1
    1c10:	a6da 362d      	macw %a5l,%d3l,>>,%a2@\+&,%a3
    1c14:	a49a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d2
    1c18:	aeda 362d      	macw %a5l,%d3l,>>,%a2@\+&,%sp
    1c1c:	a2ae 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1
    1c22:	a6ee 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3
    1c28:	a4ae 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2
    1c2e:	aeee 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp
    1c34:	a2ae 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1
    1c3a:	a6ee 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3
    1c40:	a4ae 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2
    1c46:	aeee 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp
    1c4c:	a2a1 360d      	macw %a5l,%d3l,>>,%a1@-,%d1
    1c50:	a6e1 360d      	macw %a5l,%d3l,>>,%a1@-,%a3
    1c54:	a4a1 360d      	macw %a5l,%d3l,>>,%a1@-,%d2
    1c58:	aee1 360d      	macw %a5l,%d3l,>>,%a1@-,%sp
    1c5c:	a2a1 362d      	macw %a5l,%d3l,>>,%a1@-&,%d1
    1c60:	a6e1 362d      	macw %a5l,%d3l,>>,%a1@-&,%a3
    1c64:	a4a1 362d      	macw %a5l,%d3l,>>,%a1@-&,%d2
    1c68:	aee1 362d      	macw %a5l,%d3l,>>,%a1@-&,%sp
    1c6c:	a293 320d      	macw %a5l,%d3l,<<,%a3@,%d1
    1c70:	a6d3 320d      	macw %a5l,%d3l,<<,%a3@,%a3
    1c74:	a493 320d      	macw %a5l,%d3l,<<,%a3@,%d2
    1c78:	aed3 320d      	macw %a5l,%d3l,<<,%a3@,%sp
    1c7c:	a293 322d      	macw %a5l,%d3l,<<,%a3@&,%d1
    1c80:	a6d3 322d      	macw %a5l,%d3l,<<,%a3@&,%a3
    1c84:	a493 322d      	macw %a5l,%d3l,<<,%a3@&,%d2
    1c88:	aed3 322d      	macw %a5l,%d3l,<<,%a3@&,%sp
    1c8c:	a29a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d1
    1c90:	a6da 320d      	macw %a5l,%d3l,<<,%a2@\+,%a3
    1c94:	a49a 320d      	macw %a5l,%d3l,<<,%a2@\+,%d2
    1c98:	aeda 320d      	macw %a5l,%d3l,<<,%a2@\+,%sp
    1c9c:	a29a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d1
    1ca0:	a6da 322d      	macw %a5l,%d3l,<<,%a2@\+&,%a3
    1ca4:	a49a 322d      	macw %a5l,%d3l,<<,%a2@\+&,%d2
    1ca8:	aeda 322d      	macw %a5l,%d3l,<<,%a2@\+&,%sp
    1cac:	a2ae 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d1
    1cb2:	a6ee 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%a3
    1cb8:	a4ae 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%d2
    1cbe:	aeee 320d 000a 	macw %a5l,%d3l,<<,%fp@\(10\),%sp
    1cc4:	a2ae 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d1
    1cca:	a6ee 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%a3
    1cd0:	a4ae 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%d2
    1cd6:	aeee 322d 000a 	macw %a5l,%d3l,<<,%fp@\(10\)&,%sp
    1cdc:	a2a1 320d      	macw %a5l,%d3l,<<,%a1@-,%d1
    1ce0:	a6e1 320d      	macw %a5l,%d3l,<<,%a1@-,%a3
    1ce4:	a4a1 320d      	macw %a5l,%d3l,<<,%a1@-,%d2
    1ce8:	aee1 320d      	macw %a5l,%d3l,<<,%a1@-,%sp
    1cec:	a2a1 322d      	macw %a5l,%d3l,<<,%a1@-&,%d1
    1cf0:	a6e1 322d      	macw %a5l,%d3l,<<,%a1@-&,%a3
    1cf4:	a4a1 322d      	macw %a5l,%d3l,<<,%a1@-&,%d2
    1cf8:	aee1 322d      	macw %a5l,%d3l,<<,%a1@-&,%sp
    1cfc:	a293 360d      	macw %a5l,%d3l,>>,%a3@,%d1
    1d00:	a6d3 360d      	macw %a5l,%d3l,>>,%a3@,%a3
    1d04:	a493 360d      	macw %a5l,%d3l,>>,%a3@,%d2
    1d08:	aed3 360d      	macw %a5l,%d3l,>>,%a3@,%sp
    1d0c:	a293 362d      	macw %a5l,%d3l,>>,%a3@&,%d1
    1d10:	a6d3 362d      	macw %a5l,%d3l,>>,%a3@&,%a3
    1d14:	a493 362d      	macw %a5l,%d3l,>>,%a3@&,%d2
    1d18:	aed3 362d      	macw %a5l,%d3l,>>,%a3@&,%sp
    1d1c:	a29a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d1
    1d20:	a6da 360d      	macw %a5l,%d3l,>>,%a2@\+,%a3
    1d24:	a49a 360d      	macw %a5l,%d3l,>>,%a2@\+,%d2
    1d28:	aeda 360d      	macw %a5l,%d3l,>>,%a2@\+,%sp
    1d2c:	a29a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d1
    1d30:	a6da 362d      	macw %a5l,%d3l,>>,%a2@\+&,%a3
    1d34:	a49a 362d      	macw %a5l,%d3l,>>,%a2@\+&,%d2
    1d38:	aeda 362d      	macw %a5l,%d3l,>>,%a2@\+&,%sp
    1d3c:	a2ae 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d1
    1d42:	a6ee 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%a3
    1d48:	a4ae 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%d2
    1d4e:	aeee 360d 000a 	macw %a5l,%d3l,>>,%fp@\(10\),%sp
    1d54:	a2ae 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d1
    1d5a:	a6ee 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%a3
    1d60:	a4ae 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%d2
    1d66:	aeee 362d 000a 	macw %a5l,%d3l,>>,%fp@\(10\)&,%sp
    1d6c:	a2a1 360d      	macw %a5l,%d3l,>>,%a1@-,%d1
    1d70:	a6e1 360d      	macw %a5l,%d3l,>>,%a1@-,%a3
    1d74:	a4a1 360d      	macw %a5l,%d3l,>>,%a1@-,%d2
    1d78:	aee1 360d      	macw %a5l,%d3l,>>,%a1@-,%sp
    1d7c:	a2a1 362d      	macw %a5l,%d3l,>>,%a1@-&,%d1
    1d80:	a6e1 362d      	macw %a5l,%d3l,>>,%a1@-&,%a3
    1d84:	a4a1 362d      	macw %a5l,%d3l,>>,%a1@-&,%d2
    1d88:	aee1 362d      	macw %a5l,%d3l,>>,%a1@-&,%sp
    1d8c:	a293 f08d      	macw %a5l,%a7u,%a3@,%d1
    1d90:	a6d3 f08d      	macw %a5l,%a7u,%a3@,%a3
    1d94:	a493 f08d      	macw %a5l,%a7u,%a3@,%d2
    1d98:	aed3 f08d      	macw %a5l,%a7u,%a3@,%sp
    1d9c:	a293 f0ad      	macw %a5l,%a7u,%a3@&,%d1
    1da0:	a6d3 f0ad      	macw %a5l,%a7u,%a3@&,%a3
    1da4:	a493 f0ad      	macw %a5l,%a7u,%a3@&,%d2
    1da8:	aed3 f0ad      	macw %a5l,%a7u,%a3@&,%sp
    1dac:	a29a f08d      	macw %a5l,%a7u,%a2@\+,%d1
    1db0:	a6da f08d      	macw %a5l,%a7u,%a2@\+,%a3
    1db4:	a49a f08d      	macw %a5l,%a7u,%a2@\+,%d2
    1db8:	aeda f08d      	macw %a5l,%a7u,%a2@\+,%sp
    1dbc:	a29a f0ad      	macw %a5l,%a7u,%a2@\+&,%d1
    1dc0:	a6da f0ad      	macw %a5l,%a7u,%a2@\+&,%a3
    1dc4:	a49a f0ad      	macw %a5l,%a7u,%a2@\+&,%d2
    1dc8:	aeda f0ad      	macw %a5l,%a7u,%a2@\+&,%sp
    1dcc:	a2ae f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%d1
    1dd2:	a6ee f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%a3
    1dd8:	a4ae f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%d2
    1dde:	aeee f08d 000a 	macw %a5l,%a7u,%fp@\(10\),%sp
    1de4:	a2ae f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d1
    1dea:	a6ee f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%a3
    1df0:	a4ae f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%d2
    1df6:	aeee f0ad 000a 	macw %a5l,%a7u,%fp@\(10\)&,%sp
    1dfc:	a2a1 f08d      	macw %a5l,%a7u,%a1@-,%d1
    1e00:	a6e1 f08d      	macw %a5l,%a7u,%a1@-,%a3
    1e04:	a4a1 f08d      	macw %a5l,%a7u,%a1@-,%d2
    1e08:	aee1 f08d      	macw %a5l,%a7u,%a1@-,%sp
    1e0c:	a2a1 f0ad      	macw %a5l,%a7u,%a1@-&,%d1
    1e10:	a6e1 f0ad      	macw %a5l,%a7u,%a1@-&,%a3
    1e14:	a4a1 f0ad      	macw %a5l,%a7u,%a1@-&,%d2
    1e18:	aee1 f0ad      	macw %a5l,%a7u,%a1@-&,%sp
    1e1c:	a293 f28d      	macw %a5l,%a7u,<<,%a3@,%d1
    1e20:	a6d3 f28d      	macw %a5l,%a7u,<<,%a3@,%a3
    1e24:	a493 f28d      	macw %a5l,%a7u,<<,%a3@,%d2
    1e28:	aed3 f28d      	macw %a5l,%a7u,<<,%a3@,%sp
    1e2c:	a293 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d1
    1e30:	a6d3 f2ad      	macw %a5l,%a7u,<<,%a3@&,%a3
    1e34:	a493 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d2
    1e38:	aed3 f2ad      	macw %a5l,%a7u,<<,%a3@&,%sp
    1e3c:	a29a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d1
    1e40:	a6da f28d      	macw %a5l,%a7u,<<,%a2@\+,%a3
    1e44:	a49a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d2
    1e48:	aeda f28d      	macw %a5l,%a7u,<<,%a2@\+,%sp
    1e4c:	a29a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d1
    1e50:	a6da f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%a3
    1e54:	a49a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d2
    1e58:	aeda f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%sp
    1e5c:	a2ae f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1
    1e62:	a6ee f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3
    1e68:	a4ae f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2
    1e6e:	aeee f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp
    1e74:	a2ae f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1
    1e7a:	a6ee f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3
    1e80:	a4ae f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2
    1e86:	aeee f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp
    1e8c:	a2a1 f28d      	macw %a5l,%a7u,<<,%a1@-,%d1
    1e90:	a6e1 f28d      	macw %a5l,%a7u,<<,%a1@-,%a3
    1e94:	a4a1 f28d      	macw %a5l,%a7u,<<,%a1@-,%d2
    1e98:	aee1 f28d      	macw %a5l,%a7u,<<,%a1@-,%sp
    1e9c:	a2a1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d1
    1ea0:	a6e1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%a3
    1ea4:	a4a1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d2
    1ea8:	aee1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%sp
    1eac:	a293 f68d      	macw %a5l,%a7u,>>,%a3@,%d1
    1eb0:	a6d3 f68d      	macw %a5l,%a7u,>>,%a3@,%a3
    1eb4:	a493 f68d      	macw %a5l,%a7u,>>,%a3@,%d2
    1eb8:	aed3 f68d      	macw %a5l,%a7u,>>,%a3@,%sp
    1ebc:	a293 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d1
    1ec0:	a6d3 f6ad      	macw %a5l,%a7u,>>,%a3@&,%a3
    1ec4:	a493 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d2
    1ec8:	aed3 f6ad      	macw %a5l,%a7u,>>,%a3@&,%sp
    1ecc:	a29a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d1
    1ed0:	a6da f68d      	macw %a5l,%a7u,>>,%a2@\+,%a3
    1ed4:	a49a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d2
    1ed8:	aeda f68d      	macw %a5l,%a7u,>>,%a2@\+,%sp
    1edc:	a29a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d1
    1ee0:	a6da f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%a3
    1ee4:	a49a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d2
    1ee8:	aeda f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%sp
    1eec:	a2ae f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1
    1ef2:	a6ee f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3
    1ef8:	a4ae f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2
    1efe:	aeee f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp
    1f04:	a2ae f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1
    1f0a:	a6ee f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3
    1f10:	a4ae f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2
    1f16:	aeee f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp
    1f1c:	a2a1 f68d      	macw %a5l,%a7u,>>,%a1@-,%d1
    1f20:	a6e1 f68d      	macw %a5l,%a7u,>>,%a1@-,%a3
    1f24:	a4a1 f68d      	macw %a5l,%a7u,>>,%a1@-,%d2
    1f28:	aee1 f68d      	macw %a5l,%a7u,>>,%a1@-,%sp
    1f2c:	a2a1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d1
    1f30:	a6e1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%a3
    1f34:	a4a1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d2
    1f38:	aee1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%sp
    1f3c:	a293 f28d      	macw %a5l,%a7u,<<,%a3@,%d1
    1f40:	a6d3 f28d      	macw %a5l,%a7u,<<,%a3@,%a3
    1f44:	a493 f28d      	macw %a5l,%a7u,<<,%a3@,%d2
    1f48:	aed3 f28d      	macw %a5l,%a7u,<<,%a3@,%sp
    1f4c:	a293 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d1
    1f50:	a6d3 f2ad      	macw %a5l,%a7u,<<,%a3@&,%a3
    1f54:	a493 f2ad      	macw %a5l,%a7u,<<,%a3@&,%d2
    1f58:	aed3 f2ad      	macw %a5l,%a7u,<<,%a3@&,%sp
    1f5c:	a29a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d1
    1f60:	a6da f28d      	macw %a5l,%a7u,<<,%a2@\+,%a3
    1f64:	a49a f28d      	macw %a5l,%a7u,<<,%a2@\+,%d2
    1f68:	aeda f28d      	macw %a5l,%a7u,<<,%a2@\+,%sp
    1f6c:	a29a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d1
    1f70:	a6da f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%a3
    1f74:	a49a f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%d2
    1f78:	aeda f2ad      	macw %a5l,%a7u,<<,%a2@\+&,%sp
    1f7c:	a2ae f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d1
    1f82:	a6ee f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%a3
    1f88:	a4ae f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%d2
    1f8e:	aeee f28d 000a 	macw %a5l,%a7u,<<,%fp@\(10\),%sp
    1f94:	a2ae f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d1
    1f9a:	a6ee f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%a3
    1fa0:	a4ae f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%d2
    1fa6:	aeee f2ad 000a 	macw %a5l,%a7u,<<,%fp@\(10\)&,%sp
    1fac:	a2a1 f28d      	macw %a5l,%a7u,<<,%a1@-,%d1
    1fb0:	a6e1 f28d      	macw %a5l,%a7u,<<,%a1@-,%a3
    1fb4:	a4a1 f28d      	macw %a5l,%a7u,<<,%a1@-,%d2
    1fb8:	aee1 f28d      	macw %a5l,%a7u,<<,%a1@-,%sp
    1fbc:	a2a1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d1
    1fc0:	a6e1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%a3
    1fc4:	a4a1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%d2
    1fc8:	aee1 f2ad      	macw %a5l,%a7u,<<,%a1@-&,%sp
    1fcc:	a293 f68d      	macw %a5l,%a7u,>>,%a3@,%d1
    1fd0:	a6d3 f68d      	macw %a5l,%a7u,>>,%a3@,%a3
    1fd4:	a493 f68d      	macw %a5l,%a7u,>>,%a3@,%d2
    1fd8:	aed3 f68d      	macw %a5l,%a7u,>>,%a3@,%sp
    1fdc:	a293 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d1
    1fe0:	a6d3 f6ad      	macw %a5l,%a7u,>>,%a3@&,%a3
    1fe4:	a493 f6ad      	macw %a5l,%a7u,>>,%a3@&,%d2
    1fe8:	aed3 f6ad      	macw %a5l,%a7u,>>,%a3@&,%sp
    1fec:	a29a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d1
    1ff0:	a6da f68d      	macw %a5l,%a7u,>>,%a2@\+,%a3
    1ff4:	a49a f68d      	macw %a5l,%a7u,>>,%a2@\+,%d2
    1ff8:	aeda f68d      	macw %a5l,%a7u,>>,%a2@\+,%sp
    1ffc:	a29a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d1
    2000:	a6da f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%a3
    2004:	a49a f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%d2
    2008:	aeda f6ad      	macw %a5l,%a7u,>>,%a2@\+&,%sp
    200c:	a2ae f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d1
    2012:	a6ee f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%a3
    2018:	a4ae f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%d2
    201e:	aeee f68d 000a 	macw %a5l,%a7u,>>,%fp@\(10\),%sp
    2024:	a2ae f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d1
    202a:	a6ee f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%a3
    2030:	a4ae f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%d2
    2036:	aeee f6ad 000a 	macw %a5l,%a7u,>>,%fp@\(10\)&,%sp
    203c:	a2a1 f68d      	macw %a5l,%a7u,>>,%a1@-,%d1
    2040:	a6e1 f68d      	macw %a5l,%a7u,>>,%a1@-,%a3
    2044:	a4a1 f68d      	macw %a5l,%a7u,>>,%a1@-,%d2
    2048:	aee1 f68d      	macw %a5l,%a7u,>>,%a1@-,%sp
    204c:	a2a1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d1
    2050:	a6e1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%a3
    2054:	a4a1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%d2
    2058:	aee1 f6ad      	macw %a5l,%a7u,>>,%a1@-&,%sp
    205c:	a293 100d      	macw %a5l,%d1l,%a3@,%d1
    2060:	a6d3 100d      	macw %a5l,%d1l,%a3@,%a3
    2064:	a493 100d      	macw %a5l,%d1l,%a3@,%d2
    2068:	aed3 100d      	macw %a5l,%d1l,%a3@,%sp
    206c:	a293 102d      	macw %a5l,%d1l,%a3@&,%d1
    2070:	a6d3 102d      	macw %a5l,%d1l,%a3@&,%a3
    2074:	a493 102d      	macw %a5l,%d1l,%a3@&,%d2
    2078:	aed3 102d      	macw %a5l,%d1l,%a3@&,%sp
    207c:	a29a 100d      	macw %a5l,%d1l,%a2@\+,%d1
    2080:	a6da 100d      	macw %a5l,%d1l,%a2@\+,%a3
    2084:	a49a 100d      	macw %a5l,%d1l,%a2@\+,%d2
    2088:	aeda 100d      	macw %a5l,%d1l,%a2@\+,%sp
    208c:	a29a 102d      	macw %a5l,%d1l,%a2@\+&,%d1
    2090:	a6da 102d      	macw %a5l,%d1l,%a2@\+&,%a3
    2094:	a49a 102d      	macw %a5l,%d1l,%a2@\+&,%d2
    2098:	aeda 102d      	macw %a5l,%d1l,%a2@\+&,%sp
    209c:	a2ae 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%d1
    20a2:	a6ee 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%a3
    20a8:	a4ae 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%d2
    20ae:	aeee 100d 000a 	macw %a5l,%d1l,%fp@\(10\),%sp
    20b4:	a2ae 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d1
    20ba:	a6ee 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%a3
    20c0:	a4ae 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%d2
    20c6:	aeee 102d 000a 	macw %a5l,%d1l,%fp@\(10\)&,%sp
    20cc:	a2a1 100d      	macw %a5l,%d1l,%a1@-,%d1
    20d0:	a6e1 100d      	macw %a5l,%d1l,%a1@-,%a3
    20d4:	a4a1 100d      	macw %a5l,%d1l,%a1@-,%d2
    20d8:	aee1 100d      	macw %a5l,%d1l,%a1@-,%sp
    20dc:	a2a1 102d      	macw %a5l,%d1l,%a1@-&,%d1
    20e0:	a6e1 102d      	macw %a5l,%d1l,%a1@-&,%a3
    20e4:	a4a1 102d      	macw %a5l,%d1l,%a1@-&,%d2
    20e8:	aee1 102d      	macw %a5l,%d1l,%a1@-&,%sp
    20ec:	a293 120d      	macw %a5l,%d1l,<<,%a3@,%d1
    20f0:	a6d3 120d      	macw %a5l,%d1l,<<,%a3@,%a3
    20f4:	a493 120d      	macw %a5l,%d1l,<<,%a3@,%d2
    20f8:	aed3 120d      	macw %a5l,%d1l,<<,%a3@,%sp
    20fc:	a293 122d      	macw %a5l,%d1l,<<,%a3@&,%d1
    2100:	a6d3 122d      	macw %a5l,%d1l,<<,%a3@&,%a3
    2104:	a493 122d      	macw %a5l,%d1l,<<,%a3@&,%d2
    2108:	aed3 122d      	macw %a5l,%d1l,<<,%a3@&,%sp
    210c:	a29a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d1
    2110:	a6da 120d      	macw %a5l,%d1l,<<,%a2@\+,%a3
    2114:	a49a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d2
    2118:	aeda 120d      	macw %a5l,%d1l,<<,%a2@\+,%sp
    211c:	a29a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d1
    2120:	a6da 122d      	macw %a5l,%d1l,<<,%a2@\+&,%a3
    2124:	a49a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d2
    2128:	aeda 122d      	macw %a5l,%d1l,<<,%a2@\+&,%sp
    212c:	a2ae 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1
    2132:	a6ee 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3
    2138:	a4ae 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2
    213e:	aeee 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp
    2144:	a2ae 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1
    214a:	a6ee 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3
    2150:	a4ae 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2
    2156:	aeee 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp
    215c:	a2a1 120d      	macw %a5l,%d1l,<<,%a1@-,%d1
    2160:	a6e1 120d      	macw %a5l,%d1l,<<,%a1@-,%a3
    2164:	a4a1 120d      	macw %a5l,%d1l,<<,%a1@-,%d2
    2168:	aee1 120d      	macw %a5l,%d1l,<<,%a1@-,%sp
    216c:	a2a1 122d      	macw %a5l,%d1l,<<,%a1@-&,%d1
    2170:	a6e1 122d      	macw %a5l,%d1l,<<,%a1@-&,%a3
    2174:	a4a1 122d      	macw %a5l,%d1l,<<,%a1@-&,%d2
    2178:	aee1 122d      	macw %a5l,%d1l,<<,%a1@-&,%sp
    217c:	a293 160d      	macw %a5l,%d1l,>>,%a3@,%d1
    2180:	a6d3 160d      	macw %a5l,%d1l,>>,%a3@,%a3
    2184:	a493 160d      	macw %a5l,%d1l,>>,%a3@,%d2
    2188:	aed3 160d      	macw %a5l,%d1l,>>,%a3@,%sp
    218c:	a293 162d      	macw %a5l,%d1l,>>,%a3@&,%d1
    2190:	a6d3 162d      	macw %a5l,%d1l,>>,%a3@&,%a3
    2194:	a493 162d      	macw %a5l,%d1l,>>,%a3@&,%d2
    2198:	aed3 162d      	macw %a5l,%d1l,>>,%a3@&,%sp
    219c:	a29a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d1
    21a0:	a6da 160d      	macw %a5l,%d1l,>>,%a2@\+,%a3
    21a4:	a49a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d2
    21a8:	aeda 160d      	macw %a5l,%d1l,>>,%a2@\+,%sp
    21ac:	a29a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d1
    21b0:	a6da 162d      	macw %a5l,%d1l,>>,%a2@\+&,%a3
    21b4:	a49a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d2
    21b8:	aeda 162d      	macw %a5l,%d1l,>>,%a2@\+&,%sp
    21bc:	a2ae 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1
    21c2:	a6ee 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3
    21c8:	a4ae 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2
    21ce:	aeee 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp
    21d4:	a2ae 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1
    21da:	a6ee 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3
    21e0:	a4ae 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2
    21e6:	aeee 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp
    21ec:	a2a1 160d      	macw %a5l,%d1l,>>,%a1@-,%d1
    21f0:	a6e1 160d      	macw %a5l,%d1l,>>,%a1@-,%a3
    21f4:	a4a1 160d      	macw %a5l,%d1l,>>,%a1@-,%d2
    21f8:	aee1 160d      	macw %a5l,%d1l,>>,%a1@-,%sp
    21fc:	a2a1 162d      	macw %a5l,%d1l,>>,%a1@-&,%d1
    2200:	a6e1 162d      	macw %a5l,%d1l,>>,%a1@-&,%a3
    2204:	a4a1 162d      	macw %a5l,%d1l,>>,%a1@-&,%d2
    2208:	aee1 162d      	macw %a5l,%d1l,>>,%a1@-&,%sp
    220c:	a293 120d      	macw %a5l,%d1l,<<,%a3@,%d1
    2210:	a6d3 120d      	macw %a5l,%d1l,<<,%a3@,%a3
    2214:	a493 120d      	macw %a5l,%d1l,<<,%a3@,%d2
    2218:	aed3 120d      	macw %a5l,%d1l,<<,%a3@,%sp
    221c:	a293 122d      	macw %a5l,%d1l,<<,%a3@&,%d1
    2220:	a6d3 122d      	macw %a5l,%d1l,<<,%a3@&,%a3
    2224:	a493 122d      	macw %a5l,%d1l,<<,%a3@&,%d2
    2228:	aed3 122d      	macw %a5l,%d1l,<<,%a3@&,%sp
    222c:	a29a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d1
    2230:	a6da 120d      	macw %a5l,%d1l,<<,%a2@\+,%a3
    2234:	a49a 120d      	macw %a5l,%d1l,<<,%a2@\+,%d2
    2238:	aeda 120d      	macw %a5l,%d1l,<<,%a2@\+,%sp
    223c:	a29a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d1
    2240:	a6da 122d      	macw %a5l,%d1l,<<,%a2@\+&,%a3
    2244:	a49a 122d      	macw %a5l,%d1l,<<,%a2@\+&,%d2
    2248:	aeda 122d      	macw %a5l,%d1l,<<,%a2@\+&,%sp
    224c:	a2ae 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d1
    2252:	a6ee 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%a3
    2258:	a4ae 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%d2
    225e:	aeee 120d 000a 	macw %a5l,%d1l,<<,%fp@\(10\),%sp
    2264:	a2ae 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d1
    226a:	a6ee 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%a3
    2270:	a4ae 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%d2
    2276:	aeee 122d 000a 	macw %a5l,%d1l,<<,%fp@\(10\)&,%sp
    227c:	a2a1 120d      	macw %a5l,%d1l,<<,%a1@-,%d1
    2280:	a6e1 120d      	macw %a5l,%d1l,<<,%a1@-,%a3
    2284:	a4a1 120d      	macw %a5l,%d1l,<<,%a1@-,%d2
    2288:	aee1 120d      	macw %a5l,%d1l,<<,%a1@-,%sp
    228c:	a2a1 122d      	macw %a5l,%d1l,<<,%a1@-&,%d1
    2290:	a6e1 122d      	macw %a5l,%d1l,<<,%a1@-&,%a3
    2294:	a4a1 122d      	macw %a5l,%d1l,<<,%a1@-&,%d2
    2298:	aee1 122d      	macw %a5l,%d1l,<<,%a1@-&,%sp
    229c:	a293 160d      	macw %a5l,%d1l,>>,%a3@,%d1
    22a0:	a6d3 160d      	macw %a5l,%d1l,>>,%a3@,%a3
    22a4:	a493 160d      	macw %a5l,%d1l,>>,%a3@,%d2
    22a8:	aed3 160d      	macw %a5l,%d1l,>>,%a3@,%sp
    22ac:	a293 162d      	macw %a5l,%d1l,>>,%a3@&,%d1
    22b0:	a6d3 162d      	macw %a5l,%d1l,>>,%a3@&,%a3
    22b4:	a493 162d      	macw %a5l,%d1l,>>,%a3@&,%d2
    22b8:	aed3 162d      	macw %a5l,%d1l,>>,%a3@&,%sp
    22bc:	a29a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d1
    22c0:	a6da 160d      	macw %a5l,%d1l,>>,%a2@\+,%a3
    22c4:	a49a 160d      	macw %a5l,%d1l,>>,%a2@\+,%d2
    22c8:	aeda 160d      	macw %a5l,%d1l,>>,%a2@\+,%sp
    22cc:	a29a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d1
    22d0:	a6da 162d      	macw %a5l,%d1l,>>,%a2@\+&,%a3
    22d4:	a49a 162d      	macw %a5l,%d1l,>>,%a2@\+&,%d2
    22d8:	aeda 162d      	macw %a5l,%d1l,>>,%a2@\+&,%sp
    22dc:	a2ae 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d1
    22e2:	a6ee 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%a3
    22e8:	a4ae 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%d2
    22ee:	aeee 160d 000a 	macw %a5l,%d1l,>>,%fp@\(10\),%sp
    22f4:	a2ae 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d1
    22fa:	a6ee 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%a3
    2300:	a4ae 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%d2
    2306:	aeee 162d 000a 	macw %a5l,%d1l,>>,%fp@\(10\)&,%sp
    230c:	a2a1 160d      	macw %a5l,%d1l,>>,%a1@-,%d1
    2310:	a6e1 160d      	macw %a5l,%d1l,>>,%a1@-,%a3
    2314:	a4a1 160d      	macw %a5l,%d1l,>>,%a1@-,%d2
    2318:	aee1 160d      	macw %a5l,%d1l,>>,%a1@-,%sp
    231c:	a2a1 162d      	macw %a5l,%d1l,>>,%a1@-&,%d1
    2320:	a6e1 162d      	macw %a5l,%d1l,>>,%a1@-&,%a3
    2324:	a4a1 162d      	macw %a5l,%d1l,>>,%a1@-&,%d2
    2328:	aee1 162d      	macw %a5l,%d1l,>>,%a1@-&,%sp
    232c:	a293 a0c6      	macw %d6u,%a2u,%a3@,%d1
    2330:	a6d3 a0c6      	macw %d6u,%a2u,%a3@,%a3
    2334:	a493 a0c6      	macw %d6u,%a2u,%a3@,%d2
    2338:	aed3 a0c6      	macw %d6u,%a2u,%a3@,%sp
    233c:	a293 a0e6      	macw %d6u,%a2u,%a3@&,%d1
    2340:	a6d3 a0e6      	macw %d6u,%a2u,%a3@&,%a3
    2344:	a493 a0e6      	macw %d6u,%a2u,%a3@&,%d2
    2348:	aed3 a0e6      	macw %d6u,%a2u,%a3@&,%sp
    234c:	a29a a0c6      	macw %d6u,%a2u,%a2@\+,%d1
    2350:	a6da a0c6      	macw %d6u,%a2u,%a2@\+,%a3
    2354:	a49a a0c6      	macw %d6u,%a2u,%a2@\+,%d2
    2358:	aeda a0c6      	macw %d6u,%a2u,%a2@\+,%sp
    235c:	a29a a0e6      	macw %d6u,%a2u,%a2@\+&,%d1
    2360:	a6da a0e6      	macw %d6u,%a2u,%a2@\+&,%a3
    2364:	a49a a0e6      	macw %d6u,%a2u,%a2@\+&,%d2
    2368:	aeda a0e6      	macw %d6u,%a2u,%a2@\+&,%sp
    236c:	a2ae a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%d1
    2372:	a6ee a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%a3
    2378:	a4ae a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%d2
    237e:	aeee a0c6 000a 	macw %d6u,%a2u,%fp@\(10\),%sp
    2384:	a2ae a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d1
    238a:	a6ee a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%a3
    2390:	a4ae a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%d2
    2396:	aeee a0e6 000a 	macw %d6u,%a2u,%fp@\(10\)&,%sp
    239c:	a2a1 a0c6      	macw %d6u,%a2u,%a1@-,%d1
    23a0:	a6e1 a0c6      	macw %d6u,%a2u,%a1@-,%a3
    23a4:	a4a1 a0c6      	macw %d6u,%a2u,%a1@-,%d2
    23a8:	aee1 a0c6      	macw %d6u,%a2u,%a1@-,%sp
    23ac:	a2a1 a0e6      	macw %d6u,%a2u,%a1@-&,%d1
    23b0:	a6e1 a0e6      	macw %d6u,%a2u,%a1@-&,%a3
    23b4:	a4a1 a0e6      	macw %d6u,%a2u,%a1@-&,%d2
    23b8:	aee1 a0e6      	macw %d6u,%a2u,%a1@-&,%sp
    23bc:	a293 a2c6      	macw %d6u,%a2u,<<,%a3@,%d1
    23c0:	a6d3 a2c6      	macw %d6u,%a2u,<<,%a3@,%a3
    23c4:	a493 a2c6      	macw %d6u,%a2u,<<,%a3@,%d2
    23c8:	aed3 a2c6      	macw %d6u,%a2u,<<,%a3@,%sp
    23cc:	a293 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d1
    23d0:	a6d3 a2e6      	macw %d6u,%a2u,<<,%a3@&,%a3
    23d4:	a493 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d2
    23d8:	aed3 a2e6      	macw %d6u,%a2u,<<,%a3@&,%sp
    23dc:	a29a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d1
    23e0:	a6da a2c6      	macw %d6u,%a2u,<<,%a2@\+,%a3
    23e4:	a49a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d2
    23e8:	aeda a2c6      	macw %d6u,%a2u,<<,%a2@\+,%sp
    23ec:	a29a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d1
    23f0:	a6da a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%a3
    23f4:	a49a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d2
    23f8:	aeda a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%sp
    23fc:	a2ae a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1
    2402:	a6ee a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3
    2408:	a4ae a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2
    240e:	aeee a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp
    2414:	a2ae a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1
    241a:	a6ee a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3
    2420:	a4ae a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2
    2426:	aeee a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp
    242c:	a2a1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d1
    2430:	a6e1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%a3
    2434:	a4a1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d2
    2438:	aee1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%sp
    243c:	a2a1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d1
    2440:	a6e1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%a3
    2444:	a4a1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d2
    2448:	aee1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%sp
    244c:	a293 a6c6      	macw %d6u,%a2u,>>,%a3@,%d1
    2450:	a6d3 a6c6      	macw %d6u,%a2u,>>,%a3@,%a3
    2454:	a493 a6c6      	macw %d6u,%a2u,>>,%a3@,%d2
    2458:	aed3 a6c6      	macw %d6u,%a2u,>>,%a3@,%sp
    245c:	a293 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d1
    2460:	a6d3 a6e6      	macw %d6u,%a2u,>>,%a3@&,%a3
    2464:	a493 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d2
    2468:	aed3 a6e6      	macw %d6u,%a2u,>>,%a3@&,%sp
    246c:	a29a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d1
    2470:	a6da a6c6      	macw %d6u,%a2u,>>,%a2@\+,%a3
    2474:	a49a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d2
    2478:	aeda a6c6      	macw %d6u,%a2u,>>,%a2@\+,%sp
    247c:	a29a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d1
    2480:	a6da a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%a3
    2484:	a49a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d2
    2488:	aeda a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%sp
    248c:	a2ae a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1
    2492:	a6ee a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3
    2498:	a4ae a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2
    249e:	aeee a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp
    24a4:	a2ae a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1
    24aa:	a6ee a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3
    24b0:	a4ae a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2
    24b6:	aeee a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp
    24bc:	a2a1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d1
    24c0:	a6e1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%a3
    24c4:	a4a1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d2
    24c8:	aee1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%sp
    24cc:	a2a1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d1
    24d0:	a6e1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%a3
    24d4:	a4a1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d2
    24d8:	aee1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%sp
    24dc:	a293 a2c6      	macw %d6u,%a2u,<<,%a3@,%d1
    24e0:	a6d3 a2c6      	macw %d6u,%a2u,<<,%a3@,%a3
    24e4:	a493 a2c6      	macw %d6u,%a2u,<<,%a3@,%d2
    24e8:	aed3 a2c6      	macw %d6u,%a2u,<<,%a3@,%sp
    24ec:	a293 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d1
    24f0:	a6d3 a2e6      	macw %d6u,%a2u,<<,%a3@&,%a3
    24f4:	a493 a2e6      	macw %d6u,%a2u,<<,%a3@&,%d2
    24f8:	aed3 a2e6      	macw %d6u,%a2u,<<,%a3@&,%sp
    24fc:	a29a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d1
    2500:	a6da a2c6      	macw %d6u,%a2u,<<,%a2@\+,%a3
    2504:	a49a a2c6      	macw %d6u,%a2u,<<,%a2@\+,%d2
    2508:	aeda a2c6      	macw %d6u,%a2u,<<,%a2@\+,%sp
    250c:	a29a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d1
    2510:	a6da a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%a3
    2514:	a49a a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%d2
    2518:	aeda a2e6      	macw %d6u,%a2u,<<,%a2@\+&,%sp
    251c:	a2ae a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d1
    2522:	a6ee a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%a3
    2528:	a4ae a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%d2
    252e:	aeee a2c6 000a 	macw %d6u,%a2u,<<,%fp@\(10\),%sp
    2534:	a2ae a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d1
    253a:	a6ee a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%a3
    2540:	a4ae a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%d2
    2546:	aeee a2e6 000a 	macw %d6u,%a2u,<<,%fp@\(10\)&,%sp
    254c:	a2a1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d1
    2550:	a6e1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%a3
    2554:	a4a1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%d2
    2558:	aee1 a2c6      	macw %d6u,%a2u,<<,%a1@-,%sp
    255c:	a2a1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d1
    2560:	a6e1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%a3
    2564:	a4a1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%d2
    2568:	aee1 a2e6      	macw %d6u,%a2u,<<,%a1@-&,%sp
    256c:	a293 a6c6      	macw %d6u,%a2u,>>,%a3@,%d1
    2570:	a6d3 a6c6      	macw %d6u,%a2u,>>,%a3@,%a3
    2574:	a493 a6c6      	macw %d6u,%a2u,>>,%a3@,%d2
    2578:	aed3 a6c6      	macw %d6u,%a2u,>>,%a3@,%sp
    257c:	a293 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d1
    2580:	a6d3 a6e6      	macw %d6u,%a2u,>>,%a3@&,%a3
    2584:	a493 a6e6      	macw %d6u,%a2u,>>,%a3@&,%d2
    2588:	aed3 a6e6      	macw %d6u,%a2u,>>,%a3@&,%sp
    258c:	a29a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d1
    2590:	a6da a6c6      	macw %d6u,%a2u,>>,%a2@\+,%a3
    2594:	a49a a6c6      	macw %d6u,%a2u,>>,%a2@\+,%d2
    2598:	aeda a6c6      	macw %d6u,%a2u,>>,%a2@\+,%sp
    259c:	a29a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d1
    25a0:	a6da a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%a3
    25a4:	a49a a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%d2
    25a8:	aeda a6e6      	macw %d6u,%a2u,>>,%a2@\+&,%sp
    25ac:	a2ae a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d1
    25b2:	a6ee a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%a3
    25b8:	a4ae a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%d2
    25be:	aeee a6c6 000a 	macw %d6u,%a2u,>>,%fp@\(10\),%sp
    25c4:	a2ae a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d1
    25ca:	a6ee a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%a3
    25d0:	a4ae a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%d2
    25d6:	aeee a6e6 000a 	macw %d6u,%a2u,>>,%fp@\(10\)&,%sp
    25dc:	a2a1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d1
    25e0:	a6e1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%a3
    25e4:	a4a1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%d2
    25e8:	aee1 a6c6      	macw %d6u,%a2u,>>,%a1@-,%sp
    25ec:	a2a1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d1
    25f0:	a6e1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%a3
    25f4:	a4a1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%d2
    25f8:	aee1 a6e6      	macw %d6u,%a2u,>>,%a1@-&,%sp
    25fc:	a293 3046      	macw %d6u,%d3l,%a3@,%d1
    2600:	a6d3 3046      	macw %d6u,%d3l,%a3@,%a3
    2604:	a493 3046      	macw %d6u,%d3l,%a3@,%d2
    2608:	aed3 3046      	macw %d6u,%d3l,%a3@,%sp
    260c:	a293 3066      	macw %d6u,%d3l,%a3@&,%d1
    2610:	a6d3 3066      	macw %d6u,%d3l,%a3@&,%a3
    2614:	a493 3066      	macw %d6u,%d3l,%a3@&,%d2
    2618:	aed3 3066      	macw %d6u,%d3l,%a3@&,%sp
    261c:	a29a 3046      	macw %d6u,%d3l,%a2@\+,%d1
    2620:	a6da 3046      	macw %d6u,%d3l,%a2@\+,%a3
    2624:	a49a 3046      	macw %d6u,%d3l,%a2@\+,%d2
    2628:	aeda 3046      	macw %d6u,%d3l,%a2@\+,%sp
    262c:	a29a 3066      	macw %d6u,%d3l,%a2@\+&,%d1
    2630:	a6da 3066      	macw %d6u,%d3l,%a2@\+&,%a3
    2634:	a49a 3066      	macw %d6u,%d3l,%a2@\+&,%d2
    2638:	aeda 3066      	macw %d6u,%d3l,%a2@\+&,%sp
    263c:	a2ae 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%d1
    2642:	a6ee 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%a3
    2648:	a4ae 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%d2
    264e:	aeee 3046 000a 	macw %d6u,%d3l,%fp@\(10\),%sp
    2654:	a2ae 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d1
    265a:	a6ee 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%a3
    2660:	a4ae 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%d2
    2666:	aeee 3066 000a 	macw %d6u,%d3l,%fp@\(10\)&,%sp
    266c:	a2a1 3046      	macw %d6u,%d3l,%a1@-,%d1
    2670:	a6e1 3046      	macw %d6u,%d3l,%a1@-,%a3
    2674:	a4a1 3046      	macw %d6u,%d3l,%a1@-,%d2
    2678:	aee1 3046      	macw %d6u,%d3l,%a1@-,%sp
    267c:	a2a1 3066      	macw %d6u,%d3l,%a1@-&,%d1
    2680:	a6e1 3066      	macw %d6u,%d3l,%a1@-&,%a3
    2684:	a4a1 3066      	macw %d6u,%d3l,%a1@-&,%d2
    2688:	aee1 3066      	macw %d6u,%d3l,%a1@-&,%sp
    268c:	a293 3246      	macw %d6u,%d3l,<<,%a3@,%d1
    2690:	a6d3 3246      	macw %d6u,%d3l,<<,%a3@,%a3
    2694:	a493 3246      	macw %d6u,%d3l,<<,%a3@,%d2
    2698:	aed3 3246      	macw %d6u,%d3l,<<,%a3@,%sp
    269c:	a293 3266      	macw %d6u,%d3l,<<,%a3@&,%d1
    26a0:	a6d3 3266      	macw %d6u,%d3l,<<,%a3@&,%a3
    26a4:	a493 3266      	macw %d6u,%d3l,<<,%a3@&,%d2
    26a8:	aed3 3266      	macw %d6u,%d3l,<<,%a3@&,%sp
    26ac:	a29a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d1
    26b0:	a6da 3246      	macw %d6u,%d3l,<<,%a2@\+,%a3
    26b4:	a49a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d2
    26b8:	aeda 3246      	macw %d6u,%d3l,<<,%a2@\+,%sp
    26bc:	a29a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d1
    26c0:	a6da 3266      	macw %d6u,%d3l,<<,%a2@\+&,%a3
    26c4:	a49a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d2
    26c8:	aeda 3266      	macw %d6u,%d3l,<<,%a2@\+&,%sp
    26cc:	a2ae 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1
    26d2:	a6ee 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3
    26d8:	a4ae 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2
    26de:	aeee 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp
    26e4:	a2ae 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1
    26ea:	a6ee 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3
    26f0:	a4ae 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2
    26f6:	aeee 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp
    26fc:	a2a1 3246      	macw %d6u,%d3l,<<,%a1@-,%d1
    2700:	a6e1 3246      	macw %d6u,%d3l,<<,%a1@-,%a3
    2704:	a4a1 3246      	macw %d6u,%d3l,<<,%a1@-,%d2
    2708:	aee1 3246      	macw %d6u,%d3l,<<,%a1@-,%sp
    270c:	a2a1 3266      	macw %d6u,%d3l,<<,%a1@-&,%d1
    2710:	a6e1 3266      	macw %d6u,%d3l,<<,%a1@-&,%a3
    2714:	a4a1 3266      	macw %d6u,%d3l,<<,%a1@-&,%d2
    2718:	aee1 3266      	macw %d6u,%d3l,<<,%a1@-&,%sp
    271c:	a293 3646      	macw %d6u,%d3l,>>,%a3@,%d1
    2720:	a6d3 3646      	macw %d6u,%d3l,>>,%a3@,%a3
    2724:	a493 3646      	macw %d6u,%d3l,>>,%a3@,%d2
    2728:	aed3 3646      	macw %d6u,%d3l,>>,%a3@,%sp
    272c:	a293 3666      	macw %d6u,%d3l,>>,%a3@&,%d1
    2730:	a6d3 3666      	macw %d6u,%d3l,>>,%a3@&,%a3
    2734:	a493 3666      	macw %d6u,%d3l,>>,%a3@&,%d2
    2738:	aed3 3666      	macw %d6u,%d3l,>>,%a3@&,%sp
    273c:	a29a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d1
    2740:	a6da 3646      	macw %d6u,%d3l,>>,%a2@\+,%a3
    2744:	a49a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d2
    2748:	aeda 3646      	macw %d6u,%d3l,>>,%a2@\+,%sp
    274c:	a29a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d1
    2750:	a6da 3666      	macw %d6u,%d3l,>>,%a2@\+&,%a3
    2754:	a49a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d2
    2758:	aeda 3666      	macw %d6u,%d3l,>>,%a2@\+&,%sp
    275c:	a2ae 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1
    2762:	a6ee 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3
    2768:	a4ae 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2
    276e:	aeee 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp
    2774:	a2ae 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1
    277a:	a6ee 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3
    2780:	a4ae 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2
    2786:	aeee 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp
    278c:	a2a1 3646      	macw %d6u,%d3l,>>,%a1@-,%d1
    2790:	a6e1 3646      	macw %d6u,%d3l,>>,%a1@-,%a3
    2794:	a4a1 3646      	macw %d6u,%d3l,>>,%a1@-,%d2
    2798:	aee1 3646      	macw %d6u,%d3l,>>,%a1@-,%sp
    279c:	a2a1 3666      	macw %d6u,%d3l,>>,%a1@-&,%d1
    27a0:	a6e1 3666      	macw %d6u,%d3l,>>,%a1@-&,%a3
    27a4:	a4a1 3666      	macw %d6u,%d3l,>>,%a1@-&,%d2
    27a8:	aee1 3666      	macw %d6u,%d3l,>>,%a1@-&,%sp
    27ac:	a293 3246      	macw %d6u,%d3l,<<,%a3@,%d1
    27b0:	a6d3 3246      	macw %d6u,%d3l,<<,%a3@,%a3
    27b4:	a493 3246      	macw %d6u,%d3l,<<,%a3@,%d2
    27b8:	aed3 3246      	macw %d6u,%d3l,<<,%a3@,%sp
    27bc:	a293 3266      	macw %d6u,%d3l,<<,%a3@&,%d1
    27c0:	a6d3 3266      	macw %d6u,%d3l,<<,%a3@&,%a3
    27c4:	a493 3266      	macw %d6u,%d3l,<<,%a3@&,%d2
    27c8:	aed3 3266      	macw %d6u,%d3l,<<,%a3@&,%sp
    27cc:	a29a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d1
    27d0:	a6da 3246      	macw %d6u,%d3l,<<,%a2@\+,%a3
    27d4:	a49a 3246      	macw %d6u,%d3l,<<,%a2@\+,%d2
    27d8:	aeda 3246      	macw %d6u,%d3l,<<,%a2@\+,%sp
    27dc:	a29a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d1
    27e0:	a6da 3266      	macw %d6u,%d3l,<<,%a2@\+&,%a3
    27e4:	a49a 3266      	macw %d6u,%d3l,<<,%a2@\+&,%d2
    27e8:	aeda 3266      	macw %d6u,%d3l,<<,%a2@\+&,%sp
    27ec:	a2ae 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d1
    27f2:	a6ee 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%a3
    27f8:	a4ae 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%d2
    27fe:	aeee 3246 000a 	macw %d6u,%d3l,<<,%fp@\(10\),%sp
    2804:	a2ae 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d1
    280a:	a6ee 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%a3
    2810:	a4ae 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%d2
    2816:	aeee 3266 000a 	macw %d6u,%d3l,<<,%fp@\(10\)&,%sp
    281c:	a2a1 3246      	macw %d6u,%d3l,<<,%a1@-,%d1
    2820:	a6e1 3246      	macw %d6u,%d3l,<<,%a1@-,%a3
    2824:	a4a1 3246      	macw %d6u,%d3l,<<,%a1@-,%d2
    2828:	aee1 3246      	macw %d6u,%d3l,<<,%a1@-,%sp
    282c:	a2a1 3266      	macw %d6u,%d3l,<<,%a1@-&,%d1
    2830:	a6e1 3266      	macw %d6u,%d3l,<<,%a1@-&,%a3
    2834:	a4a1 3266      	macw %d6u,%d3l,<<,%a1@-&,%d2
    2838:	aee1 3266      	macw %d6u,%d3l,<<,%a1@-&,%sp
    283c:	a293 3646      	macw %d6u,%d3l,>>,%a3@,%d1
    2840:	a6d3 3646      	macw %d6u,%d3l,>>,%a3@,%a3
    2844:	a493 3646      	macw %d6u,%d3l,>>,%a3@,%d2
    2848:	aed3 3646      	macw %d6u,%d3l,>>,%a3@,%sp
    284c:	a293 3666      	macw %d6u,%d3l,>>,%a3@&,%d1
    2850:	a6d3 3666      	macw %d6u,%d3l,>>,%a3@&,%a3
    2854:	a493 3666      	macw %d6u,%d3l,>>,%a3@&,%d2
    2858:	aed3 3666      	macw %d6u,%d3l,>>,%a3@&,%sp
    285c:	a29a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d1
    2860:	a6da 3646      	macw %d6u,%d3l,>>,%a2@\+,%a3
    2864:	a49a 3646      	macw %d6u,%d3l,>>,%a2@\+,%d2
    2868:	aeda 3646      	macw %d6u,%d3l,>>,%a2@\+,%sp
    286c:	a29a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d1
    2870:	a6da 3666      	macw %d6u,%d3l,>>,%a2@\+&,%a3
    2874:	a49a 3666      	macw %d6u,%d3l,>>,%a2@\+&,%d2
    2878:	aeda 3666      	macw %d6u,%d3l,>>,%a2@\+&,%sp
    287c:	a2ae 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d1
    2882:	a6ee 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%a3
    2888:	a4ae 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%d2
    288e:	aeee 3646 000a 	macw %d6u,%d3l,>>,%fp@\(10\),%sp
    2894:	a2ae 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d1
    289a:	a6ee 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%a3
    28a0:	a4ae 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%d2
    28a6:	aeee 3666 000a 	macw %d6u,%d3l,>>,%fp@\(10\)&,%sp
    28ac:	a2a1 3646      	macw %d6u,%d3l,>>,%a1@-,%d1
    28b0:	a6e1 3646      	macw %d6u,%d3l,>>,%a1@-,%a3
    28b4:	a4a1 3646      	macw %d6u,%d3l,>>,%a1@-,%d2
    28b8:	aee1 3646      	macw %d6u,%d3l,>>,%a1@-,%sp
    28bc:	a2a1 3666      	macw %d6u,%d3l,>>,%a1@-&,%d1
    28c0:	a6e1 3666      	macw %d6u,%d3l,>>,%a1@-&,%a3
    28c4:	a4a1 3666      	macw %d6u,%d3l,>>,%a1@-&,%d2
    28c8:	aee1 3666      	macw %d6u,%d3l,>>,%a1@-&,%sp
    28cc:	a293 f0c6      	macw %d6u,%a7u,%a3@,%d1
    28d0:	a6d3 f0c6      	macw %d6u,%a7u,%a3@,%a3
    28d4:	a493 f0c6      	macw %d6u,%a7u,%a3@,%d2
    28d8:	aed3 f0c6      	macw %d6u,%a7u,%a3@,%sp
    28dc:	a293 f0e6      	macw %d6u,%a7u,%a3@&,%d1
    28e0:	a6d3 f0e6      	macw %d6u,%a7u,%a3@&,%a3
    28e4:	a493 f0e6      	macw %d6u,%a7u,%a3@&,%d2
    28e8:	aed3 f0e6      	macw %d6u,%a7u,%a3@&,%sp
    28ec:	a29a f0c6      	macw %d6u,%a7u,%a2@\+,%d1
    28f0:	a6da f0c6      	macw %d6u,%a7u,%a2@\+,%a3
    28f4:	a49a f0c6      	macw %d6u,%a7u,%a2@\+,%d2
    28f8:	aeda f0c6      	macw %d6u,%a7u,%a2@\+,%sp
    28fc:	a29a f0e6      	macw %d6u,%a7u,%a2@\+&,%d1
    2900:	a6da f0e6      	macw %d6u,%a7u,%a2@\+&,%a3
    2904:	a49a f0e6      	macw %d6u,%a7u,%a2@\+&,%d2
    2908:	aeda f0e6      	macw %d6u,%a7u,%a2@\+&,%sp
    290c:	a2ae f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%d1
    2912:	a6ee f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%a3
    2918:	a4ae f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%d2
    291e:	aeee f0c6 000a 	macw %d6u,%a7u,%fp@\(10\),%sp
    2924:	a2ae f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d1
    292a:	a6ee f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%a3
    2930:	a4ae f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%d2
    2936:	aeee f0e6 000a 	macw %d6u,%a7u,%fp@\(10\)&,%sp
    293c:	a2a1 f0c6      	macw %d6u,%a7u,%a1@-,%d1
    2940:	a6e1 f0c6      	macw %d6u,%a7u,%a1@-,%a3
    2944:	a4a1 f0c6      	macw %d6u,%a7u,%a1@-,%d2
    2948:	aee1 f0c6      	macw %d6u,%a7u,%a1@-,%sp
    294c:	a2a1 f0e6      	macw %d6u,%a7u,%a1@-&,%d1
    2950:	a6e1 f0e6      	macw %d6u,%a7u,%a1@-&,%a3
    2954:	a4a1 f0e6      	macw %d6u,%a7u,%a1@-&,%d2
    2958:	aee1 f0e6      	macw %d6u,%a7u,%a1@-&,%sp
    295c:	a293 f2c6      	macw %d6u,%a7u,<<,%a3@,%d1
    2960:	a6d3 f2c6      	macw %d6u,%a7u,<<,%a3@,%a3
    2964:	a493 f2c6      	macw %d6u,%a7u,<<,%a3@,%d2
    2968:	aed3 f2c6      	macw %d6u,%a7u,<<,%a3@,%sp
    296c:	a293 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d1
    2970:	a6d3 f2e6      	macw %d6u,%a7u,<<,%a3@&,%a3
    2974:	a493 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d2
    2978:	aed3 f2e6      	macw %d6u,%a7u,<<,%a3@&,%sp
    297c:	a29a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d1
    2980:	a6da f2c6      	macw %d6u,%a7u,<<,%a2@\+,%a3
    2984:	a49a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d2
    2988:	aeda f2c6      	macw %d6u,%a7u,<<,%a2@\+,%sp
    298c:	a29a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d1
    2990:	a6da f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%a3
    2994:	a49a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d2
    2998:	aeda f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%sp
    299c:	a2ae f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1
    29a2:	a6ee f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3
    29a8:	a4ae f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2
    29ae:	aeee f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp
    29b4:	a2ae f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1
    29ba:	a6ee f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3
    29c0:	a4ae f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2
    29c6:	aeee f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp
    29cc:	a2a1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d1
    29d0:	a6e1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%a3
    29d4:	a4a1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d2
    29d8:	aee1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%sp
    29dc:	a2a1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d1
    29e0:	a6e1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%a3
    29e4:	a4a1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d2
    29e8:	aee1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%sp
    29ec:	a293 f6c6      	macw %d6u,%a7u,>>,%a3@,%d1
    29f0:	a6d3 f6c6      	macw %d6u,%a7u,>>,%a3@,%a3
    29f4:	a493 f6c6      	macw %d6u,%a7u,>>,%a3@,%d2
    29f8:	aed3 f6c6      	macw %d6u,%a7u,>>,%a3@,%sp
    29fc:	a293 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d1
    2a00:	a6d3 f6e6      	macw %d6u,%a7u,>>,%a3@&,%a3
    2a04:	a493 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d2
    2a08:	aed3 f6e6      	macw %d6u,%a7u,>>,%a3@&,%sp
    2a0c:	a29a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d1
    2a10:	a6da f6c6      	macw %d6u,%a7u,>>,%a2@\+,%a3
    2a14:	a49a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d2
    2a18:	aeda f6c6      	macw %d6u,%a7u,>>,%a2@\+,%sp
    2a1c:	a29a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d1
    2a20:	a6da f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%a3
    2a24:	a49a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d2
    2a28:	aeda f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%sp
    2a2c:	a2ae f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1
    2a32:	a6ee f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3
    2a38:	a4ae f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2
    2a3e:	aeee f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp
    2a44:	a2ae f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1
    2a4a:	a6ee f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3
    2a50:	a4ae f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2
    2a56:	aeee f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp
    2a5c:	a2a1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d1
    2a60:	a6e1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%a3
    2a64:	a4a1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d2
    2a68:	aee1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%sp
    2a6c:	a2a1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d1
    2a70:	a6e1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%a3
    2a74:	a4a1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d2
    2a78:	aee1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%sp
    2a7c:	a293 f2c6      	macw %d6u,%a7u,<<,%a3@,%d1
    2a80:	a6d3 f2c6      	macw %d6u,%a7u,<<,%a3@,%a3
    2a84:	a493 f2c6      	macw %d6u,%a7u,<<,%a3@,%d2
    2a88:	aed3 f2c6      	macw %d6u,%a7u,<<,%a3@,%sp
    2a8c:	a293 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d1
    2a90:	a6d3 f2e6      	macw %d6u,%a7u,<<,%a3@&,%a3
    2a94:	a493 f2e6      	macw %d6u,%a7u,<<,%a3@&,%d2
    2a98:	aed3 f2e6      	macw %d6u,%a7u,<<,%a3@&,%sp
    2a9c:	a29a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d1
    2aa0:	a6da f2c6      	macw %d6u,%a7u,<<,%a2@\+,%a3
    2aa4:	a49a f2c6      	macw %d6u,%a7u,<<,%a2@\+,%d2
    2aa8:	aeda f2c6      	macw %d6u,%a7u,<<,%a2@\+,%sp
    2aac:	a29a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d1
    2ab0:	a6da f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%a3
    2ab4:	a49a f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%d2
    2ab8:	aeda f2e6      	macw %d6u,%a7u,<<,%a2@\+&,%sp
    2abc:	a2ae f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d1
    2ac2:	a6ee f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%a3
    2ac8:	a4ae f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%d2
    2ace:	aeee f2c6 000a 	macw %d6u,%a7u,<<,%fp@\(10\),%sp
    2ad4:	a2ae f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d1
    2ada:	a6ee f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%a3
    2ae0:	a4ae f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%d2
    2ae6:	aeee f2e6 000a 	macw %d6u,%a7u,<<,%fp@\(10\)&,%sp
    2aec:	a2a1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d1
    2af0:	a6e1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%a3
    2af4:	a4a1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%d2
    2af8:	aee1 f2c6      	macw %d6u,%a7u,<<,%a1@-,%sp
    2afc:	a2a1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d1
    2b00:	a6e1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%a3
    2b04:	a4a1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%d2
    2b08:	aee1 f2e6      	macw %d6u,%a7u,<<,%a1@-&,%sp
    2b0c:	a293 f6c6      	macw %d6u,%a7u,>>,%a3@,%d1
    2b10:	a6d3 f6c6      	macw %d6u,%a7u,>>,%a3@,%a3
    2b14:	a493 f6c6      	macw %d6u,%a7u,>>,%a3@,%d2
    2b18:	aed3 f6c6      	macw %d6u,%a7u,>>,%a3@,%sp
    2b1c:	a293 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d1
    2b20:	a6d3 f6e6      	macw %d6u,%a7u,>>,%a3@&,%a3
    2b24:	a493 f6e6      	macw %d6u,%a7u,>>,%a3@&,%d2
    2b28:	aed3 f6e6      	macw %d6u,%a7u,>>,%a3@&,%sp
    2b2c:	a29a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d1
    2b30:	a6da f6c6      	macw %d6u,%a7u,>>,%a2@\+,%a3
    2b34:	a49a f6c6      	macw %d6u,%a7u,>>,%a2@\+,%d2
    2b38:	aeda f6c6      	macw %d6u,%a7u,>>,%a2@\+,%sp
    2b3c:	a29a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d1
    2b40:	a6da f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%a3
    2b44:	a49a f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%d2
    2b48:	aeda f6e6      	macw %d6u,%a7u,>>,%a2@\+&,%sp
    2b4c:	a2ae f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d1
    2b52:	a6ee f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%a3
    2b58:	a4ae f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%d2
    2b5e:	aeee f6c6 000a 	macw %d6u,%a7u,>>,%fp@\(10\),%sp
    2b64:	a2ae f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d1
    2b6a:	a6ee f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%a3
    2b70:	a4ae f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%d2
    2b76:	aeee f6e6 000a 	macw %d6u,%a7u,>>,%fp@\(10\)&,%sp
    2b7c:	a2a1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d1
    2b80:	a6e1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%a3
    2b84:	a4a1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%d2
    2b88:	aee1 f6c6      	macw %d6u,%a7u,>>,%a1@-,%sp
    2b8c:	a2a1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d1
    2b90:	a6e1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%a3
    2b94:	a4a1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%d2
    2b98:	aee1 f6e6      	macw %d6u,%a7u,>>,%a1@-&,%sp
    2b9c:	a293 1046      	macw %d6u,%d1l,%a3@,%d1
    2ba0:	a6d3 1046      	macw %d6u,%d1l,%a3@,%a3
    2ba4:	a493 1046      	macw %d6u,%d1l,%a3@,%d2
    2ba8:	aed3 1046      	macw %d6u,%d1l,%a3@,%sp
    2bac:	a293 1066      	macw %d6u,%d1l,%a3@&,%d1
    2bb0:	a6d3 1066      	macw %d6u,%d1l,%a3@&,%a3
    2bb4:	a493 1066      	macw %d6u,%d1l,%a3@&,%d2
    2bb8:	aed3 1066      	macw %d6u,%d1l,%a3@&,%sp
    2bbc:	a29a 1046      	macw %d6u,%d1l,%a2@\+,%d1
    2bc0:	a6da 1046      	macw %d6u,%d1l,%a2@\+,%a3
    2bc4:	a49a 1046      	macw %d6u,%d1l,%a2@\+,%d2
    2bc8:	aeda 1046      	macw %d6u,%d1l,%a2@\+,%sp
    2bcc:	a29a 1066      	macw %d6u,%d1l,%a2@\+&,%d1
    2bd0:	a6da 1066      	macw %d6u,%d1l,%a2@\+&,%a3
    2bd4:	a49a 1066      	macw %d6u,%d1l,%a2@\+&,%d2
    2bd8:	aeda 1066      	macw %d6u,%d1l,%a2@\+&,%sp
    2bdc:	a2ae 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%d1
    2be2:	a6ee 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%a3
    2be8:	a4ae 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%d2
    2bee:	aeee 1046 000a 	macw %d6u,%d1l,%fp@\(10\),%sp
    2bf4:	a2ae 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d1
    2bfa:	a6ee 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%a3
    2c00:	a4ae 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%d2
    2c06:	aeee 1066 000a 	macw %d6u,%d1l,%fp@\(10\)&,%sp
    2c0c:	a2a1 1046      	macw %d6u,%d1l,%a1@-,%d1
    2c10:	a6e1 1046      	macw %d6u,%d1l,%a1@-,%a3
    2c14:	a4a1 1046      	macw %d6u,%d1l,%a1@-,%d2
    2c18:	aee1 1046      	macw %d6u,%d1l,%a1@-,%sp
    2c1c:	a2a1 1066      	macw %d6u,%d1l,%a1@-&,%d1
    2c20:	a6e1 1066      	macw %d6u,%d1l,%a1@-&,%a3
    2c24:	a4a1 1066      	macw %d6u,%d1l,%a1@-&,%d2
    2c28:	aee1 1066      	macw %d6u,%d1l,%a1@-&,%sp
    2c2c:	a293 1246      	macw %d6u,%d1l,<<,%a3@,%d1
    2c30:	a6d3 1246      	macw %d6u,%d1l,<<,%a3@,%a3
    2c34:	a493 1246      	macw %d6u,%d1l,<<,%a3@,%d2
    2c38:	aed3 1246      	macw %d6u,%d1l,<<,%a3@,%sp
    2c3c:	a293 1266      	macw %d6u,%d1l,<<,%a3@&,%d1
    2c40:	a6d3 1266      	macw %d6u,%d1l,<<,%a3@&,%a3
    2c44:	a493 1266      	macw %d6u,%d1l,<<,%a3@&,%d2
    2c48:	aed3 1266      	macw %d6u,%d1l,<<,%a3@&,%sp
    2c4c:	a29a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d1
    2c50:	a6da 1246      	macw %d6u,%d1l,<<,%a2@\+,%a3
    2c54:	a49a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d2
    2c58:	aeda 1246      	macw %d6u,%d1l,<<,%a2@\+,%sp
    2c5c:	a29a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d1
    2c60:	a6da 1266      	macw %d6u,%d1l,<<,%a2@\+&,%a3
    2c64:	a49a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d2
    2c68:	aeda 1266      	macw %d6u,%d1l,<<,%a2@\+&,%sp
    2c6c:	a2ae 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1
    2c72:	a6ee 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3
    2c78:	a4ae 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2
    2c7e:	aeee 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp
    2c84:	a2ae 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1
    2c8a:	a6ee 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3
    2c90:	a4ae 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2
    2c96:	aeee 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp
    2c9c:	a2a1 1246      	macw %d6u,%d1l,<<,%a1@-,%d1
    2ca0:	a6e1 1246      	macw %d6u,%d1l,<<,%a1@-,%a3
    2ca4:	a4a1 1246      	macw %d6u,%d1l,<<,%a1@-,%d2
    2ca8:	aee1 1246      	macw %d6u,%d1l,<<,%a1@-,%sp
    2cac:	a2a1 1266      	macw %d6u,%d1l,<<,%a1@-&,%d1
    2cb0:	a6e1 1266      	macw %d6u,%d1l,<<,%a1@-&,%a3
    2cb4:	a4a1 1266      	macw %d6u,%d1l,<<,%a1@-&,%d2
    2cb8:	aee1 1266      	macw %d6u,%d1l,<<,%a1@-&,%sp
    2cbc:	a293 1646      	macw %d6u,%d1l,>>,%a3@,%d1
    2cc0:	a6d3 1646      	macw %d6u,%d1l,>>,%a3@,%a3
    2cc4:	a493 1646      	macw %d6u,%d1l,>>,%a3@,%d2
    2cc8:	aed3 1646      	macw %d6u,%d1l,>>,%a3@,%sp
    2ccc:	a293 1666      	macw %d6u,%d1l,>>,%a3@&,%d1
    2cd0:	a6d3 1666      	macw %d6u,%d1l,>>,%a3@&,%a3
    2cd4:	a493 1666      	macw %d6u,%d1l,>>,%a3@&,%d2
    2cd8:	aed3 1666      	macw %d6u,%d1l,>>,%a3@&,%sp
    2cdc:	a29a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d1
    2ce0:	a6da 1646      	macw %d6u,%d1l,>>,%a2@\+,%a3
    2ce4:	a49a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d2
    2ce8:	aeda 1646      	macw %d6u,%d1l,>>,%a2@\+,%sp
    2cec:	a29a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d1
    2cf0:	a6da 1666      	macw %d6u,%d1l,>>,%a2@\+&,%a3
    2cf4:	a49a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d2
    2cf8:	aeda 1666      	macw %d6u,%d1l,>>,%a2@\+&,%sp
    2cfc:	a2ae 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1
    2d02:	a6ee 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3
    2d08:	a4ae 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2
    2d0e:	aeee 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp
    2d14:	a2ae 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1
    2d1a:	a6ee 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3
    2d20:	a4ae 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2
    2d26:	aeee 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp
    2d2c:	a2a1 1646      	macw %d6u,%d1l,>>,%a1@-,%d1
    2d30:	a6e1 1646      	macw %d6u,%d1l,>>,%a1@-,%a3
    2d34:	a4a1 1646      	macw %d6u,%d1l,>>,%a1@-,%d2
    2d38:	aee1 1646      	macw %d6u,%d1l,>>,%a1@-,%sp
    2d3c:	a2a1 1666      	macw %d6u,%d1l,>>,%a1@-&,%d1
    2d40:	a6e1 1666      	macw %d6u,%d1l,>>,%a1@-&,%a3
    2d44:	a4a1 1666      	macw %d6u,%d1l,>>,%a1@-&,%d2
    2d48:	aee1 1666      	macw %d6u,%d1l,>>,%a1@-&,%sp
    2d4c:	a293 1246      	macw %d6u,%d1l,<<,%a3@,%d1
    2d50:	a6d3 1246      	macw %d6u,%d1l,<<,%a3@,%a3
    2d54:	a493 1246      	macw %d6u,%d1l,<<,%a3@,%d2
    2d58:	aed3 1246      	macw %d6u,%d1l,<<,%a3@,%sp
    2d5c:	a293 1266      	macw %d6u,%d1l,<<,%a3@&,%d1
    2d60:	a6d3 1266      	macw %d6u,%d1l,<<,%a3@&,%a3
    2d64:	a493 1266      	macw %d6u,%d1l,<<,%a3@&,%d2
    2d68:	aed3 1266      	macw %d6u,%d1l,<<,%a3@&,%sp
    2d6c:	a29a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d1
    2d70:	a6da 1246      	macw %d6u,%d1l,<<,%a2@\+,%a3
    2d74:	a49a 1246      	macw %d6u,%d1l,<<,%a2@\+,%d2
    2d78:	aeda 1246      	macw %d6u,%d1l,<<,%a2@\+,%sp
    2d7c:	a29a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d1
    2d80:	a6da 1266      	macw %d6u,%d1l,<<,%a2@\+&,%a3
    2d84:	a49a 1266      	macw %d6u,%d1l,<<,%a2@\+&,%d2
    2d88:	aeda 1266      	macw %d6u,%d1l,<<,%a2@\+&,%sp
    2d8c:	a2ae 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d1
    2d92:	a6ee 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%a3
    2d98:	a4ae 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%d2
    2d9e:	aeee 1246 000a 	macw %d6u,%d1l,<<,%fp@\(10\),%sp
    2da4:	a2ae 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d1
    2daa:	a6ee 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%a3
    2db0:	a4ae 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%d2
    2db6:	aeee 1266 000a 	macw %d6u,%d1l,<<,%fp@\(10\)&,%sp
    2dbc:	a2a1 1246      	macw %d6u,%d1l,<<,%a1@-,%d1
    2dc0:	a6e1 1246      	macw %d6u,%d1l,<<,%a1@-,%a3
    2dc4:	a4a1 1246      	macw %d6u,%d1l,<<,%a1@-,%d2
    2dc8:	aee1 1246      	macw %d6u,%d1l,<<,%a1@-,%sp
    2dcc:	a2a1 1266      	macw %d6u,%d1l,<<,%a1@-&,%d1
    2dd0:	a6e1 1266      	macw %d6u,%d1l,<<,%a1@-&,%a3
    2dd4:	a4a1 1266      	macw %d6u,%d1l,<<,%a1@-&,%d2
    2dd8:	aee1 1266      	macw %d6u,%d1l,<<,%a1@-&,%sp
    2ddc:	a293 1646      	macw %d6u,%d1l,>>,%a3@,%d1
    2de0:	a6d3 1646      	macw %d6u,%d1l,>>,%a3@,%a3
    2de4:	a493 1646      	macw %d6u,%d1l,>>,%a3@,%d2
    2de8:	aed3 1646      	macw %d6u,%d1l,>>,%a3@,%sp
    2dec:	a293 1666      	macw %d6u,%d1l,>>,%a3@&,%d1
    2df0:	a6d3 1666      	macw %d6u,%d1l,>>,%a3@&,%a3
    2df4:	a493 1666      	macw %d6u,%d1l,>>,%a3@&,%d2
    2df8:	aed3 1666      	macw %d6u,%d1l,>>,%a3@&,%sp
    2dfc:	a29a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d1
    2e00:	a6da 1646      	macw %d6u,%d1l,>>,%a2@\+,%a3
    2e04:	a49a 1646      	macw %d6u,%d1l,>>,%a2@\+,%d2
    2e08:	aeda 1646      	macw %d6u,%d1l,>>,%a2@\+,%sp
    2e0c:	a29a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d1
    2e10:	a6da 1666      	macw %d6u,%d1l,>>,%a2@\+&,%a3
    2e14:	a49a 1666      	macw %d6u,%d1l,>>,%a2@\+&,%d2
    2e18:	aeda 1666      	macw %d6u,%d1l,>>,%a2@\+&,%sp
    2e1c:	a2ae 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d1
    2e22:	a6ee 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%a3
    2e28:	a4ae 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%d2
    2e2e:	aeee 1646 000a 	macw %d6u,%d1l,>>,%fp@\(10\),%sp
    2e34:	a2ae 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d1
    2e3a:	a6ee 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%a3
    2e40:	a4ae 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%d2
    2e46:	aeee 1666 000a 	macw %d6u,%d1l,>>,%fp@\(10\)&,%sp
    2e4c:	a2a1 1646      	macw %d6u,%d1l,>>,%a1@-,%d1
    2e50:	a6e1 1646      	macw %d6u,%d1l,>>,%a1@-,%a3
    2e54:	a4a1 1646      	macw %d6u,%d1l,>>,%a1@-,%d2
    2e58:	aee1 1646      	macw %d6u,%d1l,>>,%a1@-,%sp
    2e5c:	a2a1 1666      	macw %d6u,%d1l,>>,%a1@-&,%d1
    2e60:	a6e1 1666      	macw %d6u,%d1l,>>,%a1@-&,%a3
    2e64:	a4a1 1666      	macw %d6u,%d1l,>>,%a1@-&,%d2
    2e68:	aee1 1666      	macw %d6u,%d1l,>>,%a1@-&,%sp
    2e6c:	a649 0800      	macl %a1,%a3
    2e70:	a649 0a00      	macl %a1,%a3,<<
    2e74:	a649 0e00      	macl %a1,%a3,>>
    2e78:	a649 0a00      	macl %a1,%a3,<<
    2e7c:	a649 0e00      	macl %a1,%a3,>>
    2e80:	a809 0800      	macl %a1,%d4
    2e84:	a809 0a00      	macl %a1,%d4,<<
    2e88:	a809 0e00      	macl %a1,%d4,>>
    2e8c:	a809 0a00      	macl %a1,%d4,<<
    2e90:	a809 0e00      	macl %a1,%d4,>>
    2e94:	a646 0800      	macl %d6,%a3
    2e98:	a646 0a00      	macl %d6,%a3,<<
    2e9c:	a646 0e00      	macl %d6,%a3,>>
    2ea0:	a646 0a00      	macl %d6,%a3,<<
    2ea4:	a646 0e00      	macl %d6,%a3,>>
    2ea8:	a806 0800      	macl %d6,%d4
    2eac:	a806 0a00      	macl %d6,%d4,<<
    2eb0:	a806 0e00      	macl %d6,%d4,>>
    2eb4:	a806 0a00      	macl %d6,%d4,<<
    2eb8:	a806 0e00      	macl %d6,%d4,>>
    2ebc:	a293 b809      	macl %a1,%a3,%a3@,%d1
    2ec0:	a6d3 b809      	macl %a1,%a3,%a3@,%a3
    2ec4:	a493 b809      	macl %a1,%a3,%a3@,%d2
    2ec8:	aed3 b809      	macl %a1,%a3,%a3@,%sp
    2ecc:	a293 b829      	macl %a1,%a3,%a3@&,%d1
    2ed0:	a6d3 b829      	macl %a1,%a3,%a3@&,%a3
    2ed4:	a493 b829      	macl %a1,%a3,%a3@&,%d2
    2ed8:	aed3 b829      	macl %a1,%a3,%a3@&,%sp
    2edc:	a29a b809      	macl %a1,%a3,%a2@\+,%d1
    2ee0:	a6da b809      	macl %a1,%a3,%a2@\+,%a3
    2ee4:	a49a b809      	macl %a1,%a3,%a2@\+,%d2
    2ee8:	aeda b809      	macl %a1,%a3,%a2@\+,%sp
    2eec:	a29a b829      	macl %a1,%a3,%a2@\+&,%d1
    2ef0:	a6da b829      	macl %a1,%a3,%a2@\+&,%a3
    2ef4:	a49a b829      	macl %a1,%a3,%a2@\+&,%d2
    2ef8:	aeda b829      	macl %a1,%a3,%a2@\+&,%sp
    2efc:	a2ae b809 000a 	macl %a1,%a3,%fp@\(10\),%d1
    2f02:	a6ee b809 000a 	macl %a1,%a3,%fp@\(10\),%a3
    2f08:	a4ae b809 000a 	macl %a1,%a3,%fp@\(10\),%d2
    2f0e:	aeee b809 000a 	macl %a1,%a3,%fp@\(10\),%sp
    2f14:	a2ae b829 000a 	macl %a1,%a3,%fp@\(10\)&,%d1
    2f1a:	a6ee b829 000a 	macl %a1,%a3,%fp@\(10\)&,%a3
    2f20:	a4ae b829 000a 	macl %a1,%a3,%fp@\(10\)&,%d2
    2f26:	aeee b829 000a 	macl %a1,%a3,%fp@\(10\)&,%sp
    2f2c:	a2a1 b809      	macl %a1,%a3,%a1@-,%d1
    2f30:	a6e1 b809      	macl %a1,%a3,%a1@-,%a3
    2f34:	a4a1 b809      	macl %a1,%a3,%a1@-,%d2
    2f38:	aee1 b809      	macl %a1,%a3,%a1@-,%sp
    2f3c:	a2a1 b829      	macl %a1,%a3,%a1@-&,%d1
    2f40:	a6e1 b829      	macl %a1,%a3,%a1@-&,%a3
    2f44:	a4a1 b829      	macl %a1,%a3,%a1@-&,%d2
    2f48:	aee1 b829      	macl %a1,%a3,%a1@-&,%sp
    2f4c:	a293 ba09      	macl %a1,%a3,<<,%a3@,%d1
    2f50:	a6d3 ba09      	macl %a1,%a3,<<,%a3@,%a3
    2f54:	a493 ba09      	macl %a1,%a3,<<,%a3@,%d2
    2f58:	aed3 ba09      	macl %a1,%a3,<<,%a3@,%sp
    2f5c:	a293 ba29      	macl %a1,%a3,<<,%a3@&,%d1
    2f60:	a6d3 ba29      	macl %a1,%a3,<<,%a3@&,%a3
    2f64:	a493 ba29      	macl %a1,%a3,<<,%a3@&,%d2
    2f68:	aed3 ba29      	macl %a1,%a3,<<,%a3@&,%sp
    2f6c:	a29a ba09      	macl %a1,%a3,<<,%a2@\+,%d1
    2f70:	a6da ba09      	macl %a1,%a3,<<,%a2@\+,%a3
    2f74:	a49a ba09      	macl %a1,%a3,<<,%a2@\+,%d2
    2f78:	aeda ba09      	macl %a1,%a3,<<,%a2@\+,%sp
    2f7c:	a29a ba29      	macl %a1,%a3,<<,%a2@\+&,%d1
    2f80:	a6da ba29      	macl %a1,%a3,<<,%a2@\+&,%a3
    2f84:	a49a ba29      	macl %a1,%a3,<<,%a2@\+&,%d2
    2f88:	aeda ba29      	macl %a1,%a3,<<,%a2@\+&,%sp
    2f8c:	a2ae ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1
    2f92:	a6ee ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3
    2f98:	a4ae ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2
    2f9e:	aeee ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp
    2fa4:	a2ae ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1
    2faa:	a6ee ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3
    2fb0:	a4ae ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2
    2fb6:	aeee ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp
    2fbc:	a2a1 ba09      	macl %a1,%a3,<<,%a1@-,%d1
    2fc0:	a6e1 ba09      	macl %a1,%a3,<<,%a1@-,%a3
    2fc4:	a4a1 ba09      	macl %a1,%a3,<<,%a1@-,%d2
    2fc8:	aee1 ba09      	macl %a1,%a3,<<,%a1@-,%sp
    2fcc:	a2a1 ba29      	macl %a1,%a3,<<,%a1@-&,%d1
    2fd0:	a6e1 ba29      	macl %a1,%a3,<<,%a1@-&,%a3
    2fd4:	a4a1 ba29      	macl %a1,%a3,<<,%a1@-&,%d2
    2fd8:	aee1 ba29      	macl %a1,%a3,<<,%a1@-&,%sp
    2fdc:	a293 be09      	macl %a1,%a3,>>,%a3@,%d1
    2fe0:	a6d3 be09      	macl %a1,%a3,>>,%a3@,%a3
    2fe4:	a493 be09      	macl %a1,%a3,>>,%a3@,%d2
    2fe8:	aed3 be09      	macl %a1,%a3,>>,%a3@,%sp
    2fec:	a293 be29      	macl %a1,%a3,>>,%a3@&,%d1
    2ff0:	a6d3 be29      	macl %a1,%a3,>>,%a3@&,%a3
    2ff4:	a493 be29      	macl %a1,%a3,>>,%a3@&,%d2
    2ff8:	aed3 be29      	macl %a1,%a3,>>,%a3@&,%sp
    2ffc:	a29a be09      	macl %a1,%a3,>>,%a2@\+,%d1
    3000:	a6da be09      	macl %a1,%a3,>>,%a2@\+,%a3
    3004:	a49a be09      	macl %a1,%a3,>>,%a2@\+,%d2
    3008:	aeda be09      	macl %a1,%a3,>>,%a2@\+,%sp
    300c:	a29a be29      	macl %a1,%a3,>>,%a2@\+&,%d1
    3010:	a6da be29      	macl %a1,%a3,>>,%a2@\+&,%a3
    3014:	a49a be29      	macl %a1,%a3,>>,%a2@\+&,%d2
    3018:	aeda be29      	macl %a1,%a3,>>,%a2@\+&,%sp
    301c:	a2ae be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1
    3022:	a6ee be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3
    3028:	a4ae be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2
    302e:	aeee be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp
    3034:	a2ae be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1
    303a:	a6ee be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3
    3040:	a4ae be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2
    3046:	aeee be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp
    304c:	a2a1 be09      	macl %a1,%a3,>>,%a1@-,%d1
    3050:	a6e1 be09      	macl %a1,%a3,>>,%a1@-,%a3
    3054:	a4a1 be09      	macl %a1,%a3,>>,%a1@-,%d2
    3058:	aee1 be09      	macl %a1,%a3,>>,%a1@-,%sp
    305c:	a2a1 be29      	macl %a1,%a3,>>,%a1@-&,%d1
    3060:	a6e1 be29      	macl %a1,%a3,>>,%a1@-&,%a3
    3064:	a4a1 be29      	macl %a1,%a3,>>,%a1@-&,%d2
    3068:	aee1 be29      	macl %a1,%a3,>>,%a1@-&,%sp
    306c:	a293 ba09      	macl %a1,%a3,<<,%a3@,%d1
    3070:	a6d3 ba09      	macl %a1,%a3,<<,%a3@,%a3
    3074:	a493 ba09      	macl %a1,%a3,<<,%a3@,%d2
    3078:	aed3 ba09      	macl %a1,%a3,<<,%a3@,%sp
    307c:	a293 ba29      	macl %a1,%a3,<<,%a3@&,%d1
    3080:	a6d3 ba29      	macl %a1,%a3,<<,%a3@&,%a3
    3084:	a493 ba29      	macl %a1,%a3,<<,%a3@&,%d2
    3088:	aed3 ba29      	macl %a1,%a3,<<,%a3@&,%sp
    308c:	a29a ba09      	macl %a1,%a3,<<,%a2@\+,%d1
    3090:	a6da ba09      	macl %a1,%a3,<<,%a2@\+,%a3
    3094:	a49a ba09      	macl %a1,%a3,<<,%a2@\+,%d2
    3098:	aeda ba09      	macl %a1,%a3,<<,%a2@\+,%sp
    309c:	a29a ba29      	macl %a1,%a3,<<,%a2@\+&,%d1
    30a0:	a6da ba29      	macl %a1,%a3,<<,%a2@\+&,%a3
    30a4:	a49a ba29      	macl %a1,%a3,<<,%a2@\+&,%d2
    30a8:	aeda ba29      	macl %a1,%a3,<<,%a2@\+&,%sp
    30ac:	a2ae ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d1
    30b2:	a6ee ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%a3
    30b8:	a4ae ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%d2
    30be:	aeee ba09 000a 	macl %a1,%a3,<<,%fp@\(10\),%sp
    30c4:	a2ae ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d1
    30ca:	a6ee ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%a3
    30d0:	a4ae ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%d2
    30d6:	aeee ba29 000a 	macl %a1,%a3,<<,%fp@\(10\)&,%sp
    30dc:	a2a1 ba09      	macl %a1,%a3,<<,%a1@-,%d1
    30e0:	a6e1 ba09      	macl %a1,%a3,<<,%a1@-,%a3
    30e4:	a4a1 ba09      	macl %a1,%a3,<<,%a1@-,%d2
    30e8:	aee1 ba09      	macl %a1,%a3,<<,%a1@-,%sp
    30ec:	a2a1 ba29      	macl %a1,%a3,<<,%a1@-&,%d1
    30f0:	a6e1 ba29      	macl %a1,%a3,<<,%a1@-&,%a3
    30f4:	a4a1 ba29      	macl %a1,%a3,<<,%a1@-&,%d2
    30f8:	aee1 ba29      	macl %a1,%a3,<<,%a1@-&,%sp
    30fc:	a293 be09      	macl %a1,%a3,>>,%a3@,%d1
    3100:	a6d3 be09      	macl %a1,%a3,>>,%a3@,%a3
    3104:	a493 be09      	macl %a1,%a3,>>,%a3@,%d2
    3108:	aed3 be09      	macl %a1,%a3,>>,%a3@,%sp
    310c:	a293 be29      	macl %a1,%a3,>>,%a3@&,%d1
    3110:	a6d3 be29      	macl %a1,%a3,>>,%a3@&,%a3
    3114:	a493 be29      	macl %a1,%a3,>>,%a3@&,%d2
    3118:	aed3 be29      	macl %a1,%a3,>>,%a3@&,%sp
    311c:	a29a be09      	macl %a1,%a3,>>,%a2@\+,%d1
    3120:	a6da be09      	macl %a1,%a3,>>,%a2@\+,%a3
    3124:	a49a be09      	macl %a1,%a3,>>,%a2@\+,%d2
    3128:	aeda be09      	macl %a1,%a3,>>,%a2@\+,%sp
    312c:	a29a be29      	macl %a1,%a3,>>,%a2@\+&,%d1
    3130:	a6da be29      	macl %a1,%a3,>>,%a2@\+&,%a3
    3134:	a49a be29      	macl %a1,%a3,>>,%a2@\+&,%d2
    3138:	aeda be29      	macl %a1,%a3,>>,%a2@\+&,%sp
    313c:	a2ae be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d1
    3142:	a6ee be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%a3
    3148:	a4ae be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%d2
    314e:	aeee be09 000a 	macl %a1,%a3,>>,%fp@\(10\),%sp
    3154:	a2ae be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d1
    315a:	a6ee be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%a3
    3160:	a4ae be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%d2
    3166:	aeee be29 000a 	macl %a1,%a3,>>,%fp@\(10\)&,%sp
    316c:	a2a1 be09      	macl %a1,%a3,>>,%a1@-,%d1
    3170:	a6e1 be09      	macl %a1,%a3,>>,%a1@-,%a3
    3174:	a4a1 be09      	macl %a1,%a3,>>,%a1@-,%d2
    3178:	aee1 be09      	macl %a1,%a3,>>,%a1@-,%sp
    317c:	a2a1 be29      	macl %a1,%a3,>>,%a1@-&,%d1
    3180:	a6e1 be29      	macl %a1,%a3,>>,%a1@-&,%a3
    3184:	a4a1 be29      	macl %a1,%a3,>>,%a1@-&,%d2
    3188:	aee1 be29      	macl %a1,%a3,>>,%a1@-&,%sp
    318c:	a293 4809      	macl %a1,%d4,%a3@,%d1
    3190:	a6d3 4809      	macl %a1,%d4,%a3@,%a3
    3194:	a493 4809      	macl %a1,%d4,%a3@,%d2
    3198:	aed3 4809      	macl %a1,%d4,%a3@,%sp
    319c:	a293 4829      	macl %a1,%d4,%a3@&,%d1
    31a0:	a6d3 4829      	macl %a1,%d4,%a3@&,%a3
    31a4:	a493 4829      	macl %a1,%d4,%a3@&,%d2
    31a8:	aed3 4829      	macl %a1,%d4,%a3@&,%sp
    31ac:	a29a 4809      	macl %a1,%d4,%a2@\+,%d1
    31b0:	a6da 4809      	macl %a1,%d4,%a2@\+,%a3
    31b4:	a49a 4809      	macl %a1,%d4,%a2@\+,%d2
    31b8:	aeda 4809      	macl %a1,%d4,%a2@\+,%sp
    31bc:	a29a 4829      	macl %a1,%d4,%a2@\+&,%d1
    31c0:	a6da 4829      	macl %a1,%d4,%a2@\+&,%a3
    31c4:	a49a 4829      	macl %a1,%d4,%a2@\+&,%d2
    31c8:	aeda 4829      	macl %a1,%d4,%a2@\+&,%sp
    31cc:	a2ae 4809 000a 	macl %a1,%d4,%fp@\(10\),%d1
    31d2:	a6ee 4809 000a 	macl %a1,%d4,%fp@\(10\),%a3
    31d8:	a4ae 4809 000a 	macl %a1,%d4,%fp@\(10\),%d2
    31de:	aeee 4809 000a 	macl %a1,%d4,%fp@\(10\),%sp
    31e4:	a2ae 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%d1
    31ea:	a6ee 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%a3
    31f0:	a4ae 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%d2
    31f6:	aeee 4829 000a 	macl %a1,%d4,%fp@\(10\)&,%sp
    31fc:	a2a1 4809      	macl %a1,%d4,%a1@-,%d1
    3200:	a6e1 4809      	macl %a1,%d4,%a1@-,%a3
    3204:	a4a1 4809      	macl %a1,%d4,%a1@-,%d2
    3208:	aee1 4809      	macl %a1,%d4,%a1@-,%sp
    320c:	a2a1 4829      	macl %a1,%d4,%a1@-&,%d1
    3210:	a6e1 4829      	macl %a1,%d4,%a1@-&,%a3
    3214:	a4a1 4829      	macl %a1,%d4,%a1@-&,%d2
    3218:	aee1 4829      	macl %a1,%d4,%a1@-&,%sp
    321c:	a293 4a09      	macl %a1,%d4,<<,%a3@,%d1
    3220:	a6d3 4a09      	macl %a1,%d4,<<,%a3@,%a3
    3224:	a493 4a09      	macl %a1,%d4,<<,%a3@,%d2
    3228:	aed3 4a09      	macl %a1,%d4,<<,%a3@,%sp
    322c:	a293 4a29      	macl %a1,%d4,<<,%a3@&,%d1
    3230:	a6d3 4a29      	macl %a1,%d4,<<,%a3@&,%a3
    3234:	a493 4a29      	macl %a1,%d4,<<,%a3@&,%d2
    3238:	aed3 4a29      	macl %a1,%d4,<<,%a3@&,%sp
    323c:	a29a 4a09      	macl %a1,%d4,<<,%a2@\+,%d1
    3240:	a6da 4a09      	macl %a1,%d4,<<,%a2@\+,%a3
    3244:	a49a 4a09      	macl %a1,%d4,<<,%a2@\+,%d2
    3248:	aeda 4a09      	macl %a1,%d4,<<,%a2@\+,%sp
    324c:	a29a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d1
    3250:	a6da 4a29      	macl %a1,%d4,<<,%a2@\+&,%a3
    3254:	a49a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d2
    3258:	aeda 4a29      	macl %a1,%d4,<<,%a2@\+&,%sp
    325c:	a2ae 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1
    3262:	a6ee 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3
    3268:	a4ae 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2
    326e:	aeee 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp
    3274:	a2ae 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1
    327a:	a6ee 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3
    3280:	a4ae 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2
    3286:	aeee 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp
    328c:	a2a1 4a09      	macl %a1,%d4,<<,%a1@-,%d1
    3290:	a6e1 4a09      	macl %a1,%d4,<<,%a1@-,%a3
    3294:	a4a1 4a09      	macl %a1,%d4,<<,%a1@-,%d2
    3298:	aee1 4a09      	macl %a1,%d4,<<,%a1@-,%sp
    329c:	a2a1 4a29      	macl %a1,%d4,<<,%a1@-&,%d1
    32a0:	a6e1 4a29      	macl %a1,%d4,<<,%a1@-&,%a3
    32a4:	a4a1 4a29      	macl %a1,%d4,<<,%a1@-&,%d2
    32a8:	aee1 4a29      	macl %a1,%d4,<<,%a1@-&,%sp
    32ac:	a293 4e09      	macl %a1,%d4,>>,%a3@,%d1
    32b0:	a6d3 4e09      	macl %a1,%d4,>>,%a3@,%a3
    32b4:	a493 4e09      	macl %a1,%d4,>>,%a3@,%d2
    32b8:	aed3 4e09      	macl %a1,%d4,>>,%a3@,%sp
    32bc:	a293 4e29      	macl %a1,%d4,>>,%a3@&,%d1
    32c0:	a6d3 4e29      	macl %a1,%d4,>>,%a3@&,%a3
    32c4:	a493 4e29      	macl %a1,%d4,>>,%a3@&,%d2
    32c8:	aed3 4e29      	macl %a1,%d4,>>,%a3@&,%sp
    32cc:	a29a 4e09      	macl %a1,%d4,>>,%a2@\+,%d1
    32d0:	a6da 4e09      	macl %a1,%d4,>>,%a2@\+,%a3
    32d4:	a49a 4e09      	macl %a1,%d4,>>,%a2@\+,%d2
    32d8:	aeda 4e09      	macl %a1,%d4,>>,%a2@\+,%sp
    32dc:	a29a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d1
    32e0:	a6da 4e29      	macl %a1,%d4,>>,%a2@\+&,%a3
    32e4:	a49a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d2
    32e8:	aeda 4e29      	macl %a1,%d4,>>,%a2@\+&,%sp
    32ec:	a2ae 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1
    32f2:	a6ee 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3
    32f8:	a4ae 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2
    32fe:	aeee 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp
    3304:	a2ae 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1
    330a:	a6ee 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3
    3310:	a4ae 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2
    3316:	aeee 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp
    331c:	a2a1 4e09      	macl %a1,%d4,>>,%a1@-,%d1
    3320:	a6e1 4e09      	macl %a1,%d4,>>,%a1@-,%a3
    3324:	a4a1 4e09      	macl %a1,%d4,>>,%a1@-,%d2
    3328:	aee1 4e09      	macl %a1,%d4,>>,%a1@-,%sp
    332c:	a2a1 4e29      	macl %a1,%d4,>>,%a1@-&,%d1
    3330:	a6e1 4e29      	macl %a1,%d4,>>,%a1@-&,%a3
    3334:	a4a1 4e29      	macl %a1,%d4,>>,%a1@-&,%d2
    3338:	aee1 4e29      	macl %a1,%d4,>>,%a1@-&,%sp
    333c:	a293 4a09      	macl %a1,%d4,<<,%a3@,%d1
    3340:	a6d3 4a09      	macl %a1,%d4,<<,%a3@,%a3
    3344:	a493 4a09      	macl %a1,%d4,<<,%a3@,%d2
    3348:	aed3 4a09      	macl %a1,%d4,<<,%a3@,%sp
    334c:	a293 4a29      	macl %a1,%d4,<<,%a3@&,%d1
    3350:	a6d3 4a29      	macl %a1,%d4,<<,%a3@&,%a3
    3354:	a493 4a29      	macl %a1,%d4,<<,%a3@&,%d2
    3358:	aed3 4a29      	macl %a1,%d4,<<,%a3@&,%sp
    335c:	a29a 4a09      	macl %a1,%d4,<<,%a2@\+,%d1
    3360:	a6da 4a09      	macl %a1,%d4,<<,%a2@\+,%a3
    3364:	a49a 4a09      	macl %a1,%d4,<<,%a2@\+,%d2
    3368:	aeda 4a09      	macl %a1,%d4,<<,%a2@\+,%sp
    336c:	a29a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d1
    3370:	a6da 4a29      	macl %a1,%d4,<<,%a2@\+&,%a3
    3374:	a49a 4a29      	macl %a1,%d4,<<,%a2@\+&,%d2
    3378:	aeda 4a29      	macl %a1,%d4,<<,%a2@\+&,%sp
    337c:	a2ae 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d1
    3382:	a6ee 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%a3
    3388:	a4ae 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%d2
    338e:	aeee 4a09 000a 	macl %a1,%d4,<<,%fp@\(10\),%sp
    3394:	a2ae 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d1
    339a:	a6ee 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%a3
    33a0:	a4ae 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%d2
    33a6:	aeee 4a29 000a 	macl %a1,%d4,<<,%fp@\(10\)&,%sp
    33ac:	a2a1 4a09      	macl %a1,%d4,<<,%a1@-,%d1
    33b0:	a6e1 4a09      	macl %a1,%d4,<<,%a1@-,%a3
    33b4:	a4a1 4a09      	macl %a1,%d4,<<,%a1@-,%d2
    33b8:	aee1 4a09      	macl %a1,%d4,<<,%a1@-,%sp
    33bc:	a2a1 4a29      	macl %a1,%d4,<<,%a1@-&,%d1
    33c0:	a6e1 4a29      	macl %a1,%d4,<<,%a1@-&,%a3
    33c4:	a4a1 4a29      	macl %a1,%d4,<<,%a1@-&,%d2
    33c8:	aee1 4a29      	macl %a1,%d4,<<,%a1@-&,%sp
    33cc:	a293 4e09      	macl %a1,%d4,>>,%a3@,%d1
    33d0:	a6d3 4e09      	macl %a1,%d4,>>,%a3@,%a3
    33d4:	a493 4e09      	macl %a1,%d4,>>,%a3@,%d2
    33d8:	aed3 4e09      	macl %a1,%d4,>>,%a3@,%sp
    33dc:	a293 4e29      	macl %a1,%d4,>>,%a3@&,%d1
    33e0:	a6d3 4e29      	macl %a1,%d4,>>,%a3@&,%a3
    33e4:	a493 4e29      	macl %a1,%d4,>>,%a3@&,%d2
    33e8:	aed3 4e29      	macl %a1,%d4,>>,%a3@&,%sp
    33ec:	a29a 4e09      	macl %a1,%d4,>>,%a2@\+,%d1
    33f0:	a6da 4e09      	macl %a1,%d4,>>,%a2@\+,%a3
    33f4:	a49a 4e09      	macl %a1,%d4,>>,%a2@\+,%d2
    33f8:	aeda 4e09      	macl %a1,%d4,>>,%a2@\+,%sp
    33fc:	a29a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d1
    3400:	a6da 4e29      	macl %a1,%d4,>>,%a2@\+&,%a3
    3404:	a49a 4e29      	macl %a1,%d4,>>,%a2@\+&,%d2
    3408:	aeda 4e29      	macl %a1,%d4,>>,%a2@\+&,%sp
    340c:	a2ae 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d1
    3412:	a6ee 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%a3
    3418:	a4ae 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%d2
    341e:	aeee 4e09 000a 	macl %a1,%d4,>>,%fp@\(10\),%sp
    3424:	a2ae 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d1
    342a:	a6ee 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%a3
    3430:	a4ae 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%d2
    3436:	aeee 4e29 000a 	macl %a1,%d4,>>,%fp@\(10\)&,%sp
    343c:	a2a1 4e09      	macl %a1,%d4,>>,%a1@-,%d1
    3440:	a6e1 4e09      	macl %a1,%d4,>>,%a1@-,%a3
    3444:	a4a1 4e09      	macl %a1,%d4,>>,%a1@-,%d2
    3448:	aee1 4e09      	macl %a1,%d4,>>,%a1@-,%sp
    344c:	a2a1 4e29      	macl %a1,%d4,>>,%a1@-&,%d1
    3450:	a6e1 4e29      	macl %a1,%d4,>>,%a1@-&,%a3
    3454:	a4a1 4e29      	macl %a1,%d4,>>,%a1@-&,%d2
    3458:	aee1 4e29      	macl %a1,%d4,>>,%a1@-&,%sp
    345c:	a293 b806      	macl %d6,%a3,%a3@,%d1
    3460:	a6d3 b806      	macl %d6,%a3,%a3@,%a3
    3464:	a493 b806      	macl %d6,%a3,%a3@,%d2
    3468:	aed3 b806      	macl %d6,%a3,%a3@,%sp
    346c:	a293 b826      	macl %d6,%a3,%a3@&,%d1
    3470:	a6d3 b826      	macl %d6,%a3,%a3@&,%a3
    3474:	a493 b826      	macl %d6,%a3,%a3@&,%d2
    3478:	aed3 b826      	macl %d6,%a3,%a3@&,%sp
    347c:	a29a b806      	macl %d6,%a3,%a2@\+,%d1
    3480:	a6da b806      	macl %d6,%a3,%a2@\+,%a3
    3484:	a49a b806      	macl %d6,%a3,%a2@\+,%d2
    3488:	aeda b806      	macl %d6,%a3,%a2@\+,%sp
    348c:	a29a b826      	macl %d6,%a3,%a2@\+&,%d1
    3490:	a6da b826      	macl %d6,%a3,%a2@\+&,%a3
    3494:	a49a b826      	macl %d6,%a3,%a2@\+&,%d2
    3498:	aeda b826      	macl %d6,%a3,%a2@\+&,%sp
    349c:	a2ae b806 000a 	macl %d6,%a3,%fp@\(10\),%d1
    34a2:	a6ee b806 000a 	macl %d6,%a3,%fp@\(10\),%a3
    34a8:	a4ae b806 000a 	macl %d6,%a3,%fp@\(10\),%d2
    34ae:	aeee b806 000a 	macl %d6,%a3,%fp@\(10\),%sp
    34b4:	a2ae b826 000a 	macl %d6,%a3,%fp@\(10\)&,%d1
    34ba:	a6ee b826 000a 	macl %d6,%a3,%fp@\(10\)&,%a3
    34c0:	a4ae b826 000a 	macl %d6,%a3,%fp@\(10\)&,%d2
    34c6:	aeee b826 000a 	macl %d6,%a3,%fp@\(10\)&,%sp
    34cc:	a2a1 b806      	macl %d6,%a3,%a1@-,%d1
    34d0:	a6e1 b806      	macl %d6,%a3,%a1@-,%a3
    34d4:	a4a1 b806      	macl %d6,%a3,%a1@-,%d2
    34d8:	aee1 b806      	macl %d6,%a3,%a1@-,%sp
    34dc:	a2a1 b826      	macl %d6,%a3,%a1@-&,%d1
    34e0:	a6e1 b826      	macl %d6,%a3,%a1@-&,%a3
    34e4:	a4a1 b826      	macl %d6,%a3,%a1@-&,%d2
    34e8:	aee1 b826      	macl %d6,%a3,%a1@-&,%sp
    34ec:	a293 ba06      	macl %d6,%a3,<<,%a3@,%d1
    34f0:	a6d3 ba06      	macl %d6,%a3,<<,%a3@,%a3
    34f4:	a493 ba06      	macl %d6,%a3,<<,%a3@,%d2
    34f8:	aed3 ba06      	macl %d6,%a3,<<,%a3@,%sp
    34fc:	a293 ba26      	macl %d6,%a3,<<,%a3@&,%d1
    3500:	a6d3 ba26      	macl %d6,%a3,<<,%a3@&,%a3
    3504:	a493 ba26      	macl %d6,%a3,<<,%a3@&,%d2
    3508:	aed3 ba26      	macl %d6,%a3,<<,%a3@&,%sp
    350c:	a29a ba06      	macl %d6,%a3,<<,%a2@\+,%d1
    3510:	a6da ba06      	macl %d6,%a3,<<,%a2@\+,%a3
    3514:	a49a ba06      	macl %d6,%a3,<<,%a2@\+,%d2
    3518:	aeda ba06      	macl %d6,%a3,<<,%a2@\+,%sp
    351c:	a29a ba26      	macl %d6,%a3,<<,%a2@\+&,%d1
    3520:	a6da ba26      	macl %d6,%a3,<<,%a2@\+&,%a3
    3524:	a49a ba26      	macl %d6,%a3,<<,%a2@\+&,%d2
    3528:	aeda ba26      	macl %d6,%a3,<<,%a2@\+&,%sp
    352c:	a2ae ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1
    3532:	a6ee ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3
    3538:	a4ae ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2
    353e:	aeee ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp
    3544:	a2ae ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1
    354a:	a6ee ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3
    3550:	a4ae ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2
    3556:	aeee ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp
    355c:	a2a1 ba06      	macl %d6,%a3,<<,%a1@-,%d1
    3560:	a6e1 ba06      	macl %d6,%a3,<<,%a1@-,%a3
    3564:	a4a1 ba06      	macl %d6,%a3,<<,%a1@-,%d2
    3568:	aee1 ba06      	macl %d6,%a3,<<,%a1@-,%sp
    356c:	a2a1 ba26      	macl %d6,%a3,<<,%a1@-&,%d1
    3570:	a6e1 ba26      	macl %d6,%a3,<<,%a1@-&,%a3
    3574:	a4a1 ba26      	macl %d6,%a3,<<,%a1@-&,%d2
    3578:	aee1 ba26      	macl %d6,%a3,<<,%a1@-&,%sp
    357c:	a293 be06      	macl %d6,%a3,>>,%a3@,%d1
    3580:	a6d3 be06      	macl %d6,%a3,>>,%a3@,%a3
    3584:	a493 be06      	macl %d6,%a3,>>,%a3@,%d2
    3588:	aed3 be06      	macl %d6,%a3,>>,%a3@,%sp
    358c:	a293 be26      	macl %d6,%a3,>>,%a3@&,%d1
    3590:	a6d3 be26      	macl %d6,%a3,>>,%a3@&,%a3
    3594:	a493 be26      	macl %d6,%a3,>>,%a3@&,%d2
    3598:	aed3 be26      	macl %d6,%a3,>>,%a3@&,%sp
    359c:	a29a be06      	macl %d6,%a3,>>,%a2@\+,%d1
    35a0:	a6da be06      	macl %d6,%a3,>>,%a2@\+,%a3
    35a4:	a49a be06      	macl %d6,%a3,>>,%a2@\+,%d2
    35a8:	aeda be06      	macl %d6,%a3,>>,%a2@\+,%sp
    35ac:	a29a be26      	macl %d6,%a3,>>,%a2@\+&,%d1
    35b0:	a6da be26      	macl %d6,%a3,>>,%a2@\+&,%a3
    35b4:	a49a be26      	macl %d6,%a3,>>,%a2@\+&,%d2
    35b8:	aeda be26      	macl %d6,%a3,>>,%a2@\+&,%sp
    35bc:	a2ae be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1
    35c2:	a6ee be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3
    35c8:	a4ae be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2
    35ce:	aeee be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp
    35d4:	a2ae be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1
    35da:	a6ee be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3
    35e0:	a4ae be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2
    35e6:	aeee be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp
    35ec:	a2a1 be06      	macl %d6,%a3,>>,%a1@-,%d1
    35f0:	a6e1 be06      	macl %d6,%a3,>>,%a1@-,%a3
    35f4:	a4a1 be06      	macl %d6,%a3,>>,%a1@-,%d2
    35f8:	aee1 be06      	macl %d6,%a3,>>,%a1@-,%sp
    35fc:	a2a1 be26      	macl %d6,%a3,>>,%a1@-&,%d1
    3600:	a6e1 be26      	macl %d6,%a3,>>,%a1@-&,%a3
    3604:	a4a1 be26      	macl %d6,%a3,>>,%a1@-&,%d2
    3608:	aee1 be26      	macl %d6,%a3,>>,%a1@-&,%sp
    360c:	a293 ba06      	macl %d6,%a3,<<,%a3@,%d1
    3610:	a6d3 ba06      	macl %d6,%a3,<<,%a3@,%a3
    3614:	a493 ba06      	macl %d6,%a3,<<,%a3@,%d2
    3618:	aed3 ba06      	macl %d6,%a3,<<,%a3@,%sp
    361c:	a293 ba26      	macl %d6,%a3,<<,%a3@&,%d1
    3620:	a6d3 ba26      	macl %d6,%a3,<<,%a3@&,%a3
    3624:	a493 ba26      	macl %d6,%a3,<<,%a3@&,%d2
    3628:	aed3 ba26      	macl %d6,%a3,<<,%a3@&,%sp
    362c:	a29a ba06      	macl %d6,%a3,<<,%a2@\+,%d1
    3630:	a6da ba06      	macl %d6,%a3,<<,%a2@\+,%a3
    3634:	a49a ba06      	macl %d6,%a3,<<,%a2@\+,%d2
    3638:	aeda ba06      	macl %d6,%a3,<<,%a2@\+,%sp
    363c:	a29a ba26      	macl %d6,%a3,<<,%a2@\+&,%d1
    3640:	a6da ba26      	macl %d6,%a3,<<,%a2@\+&,%a3
    3644:	a49a ba26      	macl %d6,%a3,<<,%a2@\+&,%d2
    3648:	aeda ba26      	macl %d6,%a3,<<,%a2@\+&,%sp
    364c:	a2ae ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d1
    3652:	a6ee ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%a3
    3658:	a4ae ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%d2
    365e:	aeee ba06 000a 	macl %d6,%a3,<<,%fp@\(10\),%sp
    3664:	a2ae ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d1
    366a:	a6ee ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%a3
    3670:	a4ae ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%d2
    3676:	aeee ba26 000a 	macl %d6,%a3,<<,%fp@\(10\)&,%sp
    367c:	a2a1 ba06      	macl %d6,%a3,<<,%a1@-,%d1
    3680:	a6e1 ba06      	macl %d6,%a3,<<,%a1@-,%a3
    3684:	a4a1 ba06      	macl %d6,%a3,<<,%a1@-,%d2
    3688:	aee1 ba06      	macl %d6,%a3,<<,%a1@-,%sp
    368c:	a2a1 ba26      	macl %d6,%a3,<<,%a1@-&,%d1
    3690:	a6e1 ba26      	macl %d6,%a3,<<,%a1@-&,%a3
    3694:	a4a1 ba26      	macl %d6,%a3,<<,%a1@-&,%d2
    3698:	aee1 ba26      	macl %d6,%a3,<<,%a1@-&,%sp
    369c:	a293 be06      	macl %d6,%a3,>>,%a3@,%d1
    36a0:	a6d3 be06      	macl %d6,%a3,>>,%a3@,%a3
    36a4:	a493 be06      	macl %d6,%a3,>>,%a3@,%d2
    36a8:	aed3 be06      	macl %d6,%a3,>>,%a3@,%sp
    36ac:	a293 be26      	macl %d6,%a3,>>,%a3@&,%d1
    36b0:	a6d3 be26      	macl %d6,%a3,>>,%a3@&,%a3
    36b4:	a493 be26      	macl %d6,%a3,>>,%a3@&,%d2
    36b8:	aed3 be26      	macl %d6,%a3,>>,%a3@&,%sp
    36bc:	a29a be06      	macl %d6,%a3,>>,%a2@\+,%d1
    36c0:	a6da be06      	macl %d6,%a3,>>,%a2@\+,%a3
    36c4:	a49a be06      	macl %d6,%a3,>>,%a2@\+,%d2
    36c8:	aeda be06      	macl %d6,%a3,>>,%a2@\+,%sp
    36cc:	a29a be26      	macl %d6,%a3,>>,%a2@\+&,%d1
    36d0:	a6da be26      	macl %d6,%a3,>>,%a2@\+&,%a3
    36d4:	a49a be26      	macl %d6,%a3,>>,%a2@\+&,%d2
    36d8:	aeda be26      	macl %d6,%a3,>>,%a2@\+&,%sp
    36dc:	a2ae be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d1
    36e2:	a6ee be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%a3
    36e8:	a4ae be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%d2
    36ee:	aeee be06 000a 	macl %d6,%a3,>>,%fp@\(10\),%sp
    36f4:	a2ae be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d1
    36fa:	a6ee be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%a3
    3700:	a4ae be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%d2
    3706:	aeee be26 000a 	macl %d6,%a3,>>,%fp@\(10\)&,%sp
    370c:	a2a1 be06      	macl %d6,%a3,>>,%a1@-,%d1
    3710:	a6e1 be06      	macl %d6,%a3,>>,%a1@-,%a3
    3714:	a4a1 be06      	macl %d6,%a3,>>,%a1@-,%d2
    3718:	aee1 be06      	macl %d6,%a3,>>,%a1@-,%sp
    371c:	a2a1 be26      	macl %d6,%a3,>>,%a1@-&,%d1
    3720:	a6e1 be26      	macl %d6,%a3,>>,%a1@-&,%a3
    3724:	a4a1 be26      	macl %d6,%a3,>>,%a1@-&,%d2
    3728:	aee1 be26      	macl %d6,%a3,>>,%a1@-&,%sp
    372c:	a293 4806      	macl %d6,%d4,%a3@,%d1
    3730:	a6d3 4806      	macl %d6,%d4,%a3@,%a3
    3734:	a493 4806      	macl %d6,%d4,%a3@,%d2
    3738:	aed3 4806      	macl %d6,%d4,%a3@,%sp
    373c:	a293 4826      	macl %d6,%d4,%a3@&,%d1
    3740:	a6d3 4826      	macl %d6,%d4,%a3@&,%a3
    3744:	a493 4826      	macl %d6,%d4,%a3@&,%d2
    3748:	aed3 4826      	macl %d6,%d4,%a3@&,%sp
    374c:	a29a 4806      	macl %d6,%d4,%a2@\+,%d1
    3750:	a6da 4806      	macl %d6,%d4,%a2@\+,%a3
    3754:	a49a 4806      	macl %d6,%d4,%a2@\+,%d2
    3758:	aeda 4806      	macl %d6,%d4,%a2@\+,%sp
    375c:	a29a 4826      	macl %d6,%d4,%a2@\+&,%d1
    3760:	a6da 4826      	macl %d6,%d4,%a2@\+&,%a3
    3764:	a49a 4826      	macl %d6,%d4,%a2@\+&,%d2
    3768:	aeda 4826      	macl %d6,%d4,%a2@\+&,%sp
    376c:	a2ae 4806 000a 	macl %d6,%d4,%fp@\(10\),%d1
    3772:	a6ee 4806 000a 	macl %d6,%d4,%fp@\(10\),%a3
    3778:	a4ae 4806 000a 	macl %d6,%d4,%fp@\(10\),%d2
    377e:	aeee 4806 000a 	macl %d6,%d4,%fp@\(10\),%sp
    3784:	a2ae 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%d1
    378a:	a6ee 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%a3
    3790:	a4ae 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%d2
    3796:	aeee 4826 000a 	macl %d6,%d4,%fp@\(10\)&,%sp
    379c:	a2a1 4806      	macl %d6,%d4,%a1@-,%d1
    37a0:	a6e1 4806      	macl %d6,%d4,%a1@-,%a3
    37a4:	a4a1 4806      	macl %d6,%d4,%a1@-,%d2
    37a8:	aee1 4806      	macl %d6,%d4,%a1@-,%sp
    37ac:	a2a1 4826      	macl %d6,%d4,%a1@-&,%d1
    37b0:	a6e1 4826      	macl %d6,%d4,%a1@-&,%a3
    37b4:	a4a1 4826      	macl %d6,%d4,%a1@-&,%d2
    37b8:	aee1 4826      	macl %d6,%d4,%a1@-&,%sp
    37bc:	a293 4a06      	macl %d6,%d4,<<,%a3@,%d1
    37c0:	a6d3 4a06      	macl %d6,%d4,<<,%a3@,%a3
    37c4:	a493 4a06      	macl %d6,%d4,<<,%a3@,%d2
    37c8:	aed3 4a06      	macl %d6,%d4,<<,%a3@,%sp
    37cc:	a293 4a26      	macl %d6,%d4,<<,%a3@&,%d1
    37d0:	a6d3 4a26      	macl %d6,%d4,<<,%a3@&,%a3
    37d4:	a493 4a26      	macl %d6,%d4,<<,%a3@&,%d2
    37d8:	aed3 4a26      	macl %d6,%d4,<<,%a3@&,%sp
    37dc:	a29a 4a06      	macl %d6,%d4,<<,%a2@\+,%d1
    37e0:	a6da 4a06      	macl %d6,%d4,<<,%a2@\+,%a3
    37e4:	a49a 4a06      	macl %d6,%d4,<<,%a2@\+,%d2
    37e8:	aeda 4a06      	macl %d6,%d4,<<,%a2@\+,%sp
    37ec:	a29a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d1
    37f0:	a6da 4a26      	macl %d6,%d4,<<,%a2@\+&,%a3
    37f4:	a49a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d2
    37f8:	aeda 4a26      	macl %d6,%d4,<<,%a2@\+&,%sp
    37fc:	a2ae 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1
    3802:	a6ee 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3
    3808:	a4ae 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2
    380e:	aeee 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp
    3814:	a2ae 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1
    381a:	a6ee 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3
    3820:	a4ae 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2
    3826:	aeee 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp
    382c:	a2a1 4a06      	macl %d6,%d4,<<,%a1@-,%d1
    3830:	a6e1 4a06      	macl %d6,%d4,<<,%a1@-,%a3
    3834:	a4a1 4a06      	macl %d6,%d4,<<,%a1@-,%d2
    3838:	aee1 4a06      	macl %d6,%d4,<<,%a1@-,%sp
    383c:	a2a1 4a26      	macl %d6,%d4,<<,%a1@-&,%d1
    3840:	a6e1 4a26      	macl %d6,%d4,<<,%a1@-&,%a3
    3844:	a4a1 4a26      	macl %d6,%d4,<<,%a1@-&,%d2
    3848:	aee1 4a26      	macl %d6,%d4,<<,%a1@-&,%sp
    384c:	a293 4e06      	macl %d6,%d4,>>,%a3@,%d1
    3850:	a6d3 4e06      	macl %d6,%d4,>>,%a3@,%a3
    3854:	a493 4e06      	macl %d6,%d4,>>,%a3@,%d2
    3858:	aed3 4e06      	macl %d6,%d4,>>,%a3@,%sp
    385c:	a293 4e26      	macl %d6,%d4,>>,%a3@&,%d1
    3860:	a6d3 4e26      	macl %d6,%d4,>>,%a3@&,%a3
    3864:	a493 4e26      	macl %d6,%d4,>>,%a3@&,%d2
    3868:	aed3 4e26      	macl %d6,%d4,>>,%a3@&,%sp
    386c:	a29a 4e06      	macl %d6,%d4,>>,%a2@\+,%d1
    3870:	a6da 4e06      	macl %d6,%d4,>>,%a2@\+,%a3
    3874:	a49a 4e06      	macl %d6,%d4,>>,%a2@\+,%d2
    3878:	aeda 4e06      	macl %d6,%d4,>>,%a2@\+,%sp
    387c:	a29a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d1
    3880:	a6da 4e26      	macl %d6,%d4,>>,%a2@\+&,%a3
    3884:	a49a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d2
    3888:	aeda 4e26      	macl %d6,%d4,>>,%a2@\+&,%sp
    388c:	a2ae 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1
    3892:	a6ee 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3
    3898:	a4ae 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2
    389e:	aeee 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp
    38a4:	a2ae 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1
    38aa:	a6ee 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3
    38b0:	a4ae 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2
    38b6:	aeee 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp
    38bc:	a2a1 4e06      	macl %d6,%d4,>>,%a1@-,%d1
    38c0:	a6e1 4e06      	macl %d6,%d4,>>,%a1@-,%a3
    38c4:	a4a1 4e06      	macl %d6,%d4,>>,%a1@-,%d2
    38c8:	aee1 4e06      	macl %d6,%d4,>>,%a1@-,%sp
    38cc:	a2a1 4e26      	macl %d6,%d4,>>,%a1@-&,%d1
    38d0:	a6e1 4e26      	macl %d6,%d4,>>,%a1@-&,%a3
    38d4:	a4a1 4e26      	macl %d6,%d4,>>,%a1@-&,%d2
    38d8:	aee1 4e26      	macl %d6,%d4,>>,%a1@-&,%sp
    38dc:	a293 4a06      	macl %d6,%d4,<<,%a3@,%d1
    38e0:	a6d3 4a06      	macl %d6,%d4,<<,%a3@,%a3
    38e4:	a493 4a06      	macl %d6,%d4,<<,%a3@,%d2
    38e8:	aed3 4a06      	macl %d6,%d4,<<,%a3@,%sp
    38ec:	a293 4a26      	macl %d6,%d4,<<,%a3@&,%d1
    38f0:	a6d3 4a26      	macl %d6,%d4,<<,%a3@&,%a3
    38f4:	a493 4a26      	macl %d6,%d4,<<,%a3@&,%d2
    38f8:	aed3 4a26      	macl %d6,%d4,<<,%a3@&,%sp
    38fc:	a29a 4a06      	macl %d6,%d4,<<,%a2@\+,%d1
    3900:	a6da 4a06      	macl %d6,%d4,<<,%a2@\+,%a3
    3904:	a49a 4a06      	macl %d6,%d4,<<,%a2@\+,%d2
    3908:	aeda 4a06      	macl %d6,%d4,<<,%a2@\+,%sp
    390c:	a29a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d1
    3910:	a6da 4a26      	macl %d6,%d4,<<,%a2@\+&,%a3
    3914:	a49a 4a26      	macl %d6,%d4,<<,%a2@\+&,%d2
    3918:	aeda 4a26      	macl %d6,%d4,<<,%a2@\+&,%sp
    391c:	a2ae 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d1
    3922:	a6ee 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%a3
    3928:	a4ae 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%d2
    392e:	aeee 4a06 000a 	macl %d6,%d4,<<,%fp@\(10\),%sp
    3934:	a2ae 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d1
    393a:	a6ee 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%a3
    3940:	a4ae 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%d2
    3946:	aeee 4a26 000a 	macl %d6,%d4,<<,%fp@\(10\)&,%sp
    394c:	a2a1 4a06      	macl %d6,%d4,<<,%a1@-,%d1
    3950:	a6e1 4a06      	macl %d6,%d4,<<,%a1@-,%a3
    3954:	a4a1 4a06      	macl %d6,%d4,<<,%a1@-,%d2
    3958:	aee1 4a06      	macl %d6,%d4,<<,%a1@-,%sp
    395c:	a2a1 4a26      	macl %d6,%d4,<<,%a1@-&,%d1
    3960:	a6e1 4a26      	macl %d6,%d4,<<,%a1@-&,%a3
    3964:	a4a1 4a26      	macl %d6,%d4,<<,%a1@-&,%d2
    3968:	aee1 4a26      	macl %d6,%d4,<<,%a1@-&,%sp
    396c:	a293 4e06      	macl %d6,%d4,>>,%a3@,%d1
    3970:	a6d3 4e06      	macl %d6,%d4,>>,%a3@,%a3
    3974:	a493 4e06      	macl %d6,%d4,>>,%a3@,%d2
    3978:	aed3 4e06      	macl %d6,%d4,>>,%a3@,%sp
    397c:	a293 4e26      	macl %d6,%d4,>>,%a3@&,%d1
    3980:	a6d3 4e26      	macl %d6,%d4,>>,%a3@&,%a3
    3984:	a493 4e26      	macl %d6,%d4,>>,%a3@&,%d2
    3988:	aed3 4e26      	macl %d6,%d4,>>,%a3@&,%sp
    398c:	a29a 4e06      	macl %d6,%d4,>>,%a2@\+,%d1
    3990:	a6da 4e06      	macl %d6,%d4,>>,%a2@\+,%a3
    3994:	a49a 4e06      	macl %d6,%d4,>>,%a2@\+,%d2
    3998:	aeda 4e06      	macl %d6,%d4,>>,%a2@\+,%sp
    399c:	a29a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d1
    39a0:	a6da 4e26      	macl %d6,%d4,>>,%a2@\+&,%a3
    39a4:	a49a 4e26      	macl %d6,%d4,>>,%a2@\+&,%d2
    39a8:	aeda 4e26      	macl %d6,%d4,>>,%a2@\+&,%sp
    39ac:	a2ae 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d1
    39b2:	a6ee 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%a3
    39b8:	a4ae 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%d2
    39be:	aeee 4e06 000a 	macl %d6,%d4,>>,%fp@\(10\),%sp
    39c4:	a2ae 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d1
    39ca:	a6ee 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%a3
    39d0:	a4ae 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%d2
    39d6:	aeee 4e26 000a 	macl %d6,%d4,>>,%fp@\(10\)&,%sp
    39dc:	a2a1 4e06      	macl %d6,%d4,>>,%a1@-,%d1
    39e0:	a6e1 4e06      	macl %d6,%d4,>>,%a1@-,%a3
    39e4:	a4a1 4e06      	macl %d6,%d4,>>,%a1@-,%d2
    39e8:	aee1 4e06      	macl %d6,%d4,>>,%a1@-,%sp
    39ec:	a2a1 4e26      	macl %d6,%d4,>>,%a1@-&,%d1
    39f0:	a6e1 4e26      	macl %d6,%d4,>>,%a1@-&,%a3
    39f4:	a4a1 4e26      	macl %d6,%d4,>>,%a1@-&,%d2
    39f8:	aee1 4e26      	macl %d6,%d4,>>,%a1@-&,%sp
