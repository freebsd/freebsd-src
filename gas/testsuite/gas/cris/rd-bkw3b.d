#as: --underscore --em=criself --march=v32
#source: brokw-3b.s
#objdump: -dr

.*:     file format .*-cris

Disassembly of section \.text:

00000000 <start>:
       0:	4002                	moveq 0,r0
       2:	af0c 5700           	subs\.b 87,r0
       6:	cf0d 2900           	bound\.b 0x29,r0
       a:	75f9                	lapcq 14 <sym2>,acr
       c:	1f05                	addi r0\.w,acr
       e:	3ff8                	adds\.w \[acr\],acr
      10:	bf09                	jump acr
      12:	b005                	nop 

00000014 <sym2>:
      14:	b401 .*
      16:	aa01 .*
      18:	a001 .*
      1a:	9601 .*
      1c:	8c01 .*
      1e:	8201 .*
      20:	7801 .*
      22:	6e01 .*
      24:	6401 .*
      26:	5a01 .*
      28:	5001 .*
      2a:	4601 .*
      2c:	3c01 .*
      2e:	3201 .*
      30:	2801 .*
      32:	1e01 .*
      34:	1401 .*
      36:	0a01 .*
      38:	0001 .*
      3a:	f600 .*
      3c:	ec00 .*
      3e:	e200 .*
      40:	d800 .*
      42:	ce00 .*
      44:	c400 .*
      46:	ba00 .*
      48:	b000 .*
      4a:	a600 .*
      4c:	9c00 .*
      4e:	9200 .*
      50:	8800 .*
      52:	7e00 .*
      54:	7400 .*
      56:	6a00 .*
      58:	6000 .*
      5a:	5600 .*
      5c:	4c00 .*
      5e:	4200 .*
      60:	3800 .*
      62:	2e00 .*
      64:	2400 .*
      66:	1a00 .*
      68:	0000 .*
	\.\.\.
      76:	0000 .*
      78:	4102                	moveq 1,r0
      7a:	ffed 5601           	ba 1d0 <next_label>
      7e:	b005                	nop 
      80:	bf0e a481 0000      	ba 8224 <sym43>
      86:	b005                	nop 
      88:	bf0e 9a81 0000      	ba 8222 <sym42>
      8e:	b005                	nop 
      90:	bf0e 9081 0000      	ba 8220 <sym41>
      96:	b005                	nop 
      98:	bf0e 8681 0000      	ba 821e <sym40>
      9e:	b005                	nop 
      a0:	bf0e 7c81 0000      	ba 821c <sym39>
      a6:	b005                	nop 
      a8:	bf0e 7281 0000      	ba 821a <sym38>
      ae:	b005                	nop 
      b0:	bf0e 6881 0000      	ba 8218 <sym37>
      b6:	b005                	nop 
      b8:	bf0e 5e81 0000      	ba 8216 <sym36>
      be:	b005                	nop 
      c0:	bf0e 5481 0000      	ba 8214 <sym35>
      c6:	b005                	nop 
      c8:	bf0e 4a81 0000      	ba 8212 <sym34>
      ce:	b005                	nop 
      d0:	bf0e 4081 0000      	ba 8210 <sym33>
      d6:	b005                	nop 
      d8:	bf0e 3681 0000      	ba 820e <sym32>
      de:	b005                	nop 
      e0:	bf0e 2c81 0000      	ba 820c <sym31>
      e6:	b005                	nop 
      e8:	bf0e 2281 0000      	ba 820a <sym30>
      ee:	b005                	nop 
      f0:	bf0e 1881 0000      	ba 8208 <sym29>
      f6:	b005                	nop 
      f8:	bf0e 0e81 0000      	ba 8206 <sym28>
      fe:	b005                	nop 
     100:	bf0e 0481 0000      	ba 8204 <sym27>
     106:	b005                	nop 
     108:	bf0e fa80 0000      	ba 8202 <sym26>
     10e:	b005                	nop 
     110:	bf0e f080 0000      	ba 8200 <sym25>
     116:	b005                	nop 
     118:	bf0e e680 0000      	ba 81fe <sym24>
     11e:	b005                	nop 
     120:	bf0e dc80 0000      	ba 81fc <sym23>
     126:	b005                	nop 
     128:	bf0e d280 0000      	ba 81fa <sym22>
     12e:	b005                	nop 
     130:	bf0e c880 0000      	ba 81f8 <sym21>
     136:	b005                	nop 
     138:	bf0e be80 0000      	ba 81f6 <sym20>
     13e:	b005                	nop 
     140:	bf0e b480 0000      	ba 81f4 <sym19>
     146:	b005                	nop 
     148:	bf0e aa80 0000      	ba 81f2 <sym18>
     14e:	b005                	nop 
     150:	bf0e a080 0000      	ba 81f0 <sym17>
     156:	b005                	nop 
     158:	bf0e 9680 0000      	ba 81ee <sym16>
     15e:	b005                	nop 
     160:	bf0e 8c80 0000      	ba 81ec <sym15>
     166:	b005                	nop 
     168:	bf0e 8280 0000      	ba 81ea <sym14>
     16e:	b005                	nop 
     170:	bf0e 7880 0000      	ba 81e8 <sym13>
     176:	b005                	nop 
     178:	bf0e 6e80 0000      	ba 81e6 <sym12>
     17e:	b005                	nop 
     180:	bf0e 6480 0000      	ba 81e4 <sym11>
     186:	b005                	nop 
     188:	bf0e 5a80 0000      	ba 81e2 <sym10>
     18e:	b005                	nop 
     190:	bf0e 5080 0000      	ba 81e0 <sym9>
     196:	b005                	nop 
     198:	bf0e 4680 0000      	ba 81de <sym8>
     19e:	b005                	nop 
     1a0:	bf0e 3c80 0000      	ba 81dc <sym7>
     1a6:	b005                	nop 
     1a8:	bf0e 3280 0000      	ba 81da <sym6>
     1ae:	b005                	nop 
     1b0:	bf0e 2880 0000      	ba 81d8 <sym5>
     1b6:	b005                	nop 
     1b8:	bf0e 1e80 0000      	ba 81d6 <sym4>
     1be:	b005                	nop 
     1c0:	bf0e 1480 0000      	ba 81d4 <sym3>
     1c6:	b005                	nop 
     1c8:	bf0e 0a80 0000      	ba 81d2 <sym1>
     1ce:	b005                	nop 

000001d0 <next_label>:
     1d0:	4202                	moveq 2,r0
	\.\.\.

000081d2 <sym1>:
    81d2:	7d02                	moveq -3,r0

000081d4 <sym3>:
    81d4:	4302                	moveq 3,r0

000081d6 <sym4>:
    81d6:	4402                	moveq 4,r0

000081d8 <sym5>:
    81d8:	4502                	moveq 5,r0

000081da <sym6>:
    81da:	4602                	moveq 6,r0

000081dc <sym7>:
    81dc:	4702                	moveq 7,r0

000081de <sym8>:
    81de:	4802                	moveq 8,r0

000081e0 <sym9>:
    81e0:	4902                	moveq 9,r0

000081e2 <sym10>:
    81e2:	4a02                	moveq 10,r0

000081e4 <sym11>:
    81e4:	4b02                	moveq 11,r0

000081e6 <sym12>:
    81e6:	4c02                	moveq 12,r0

000081e8 <sym13>:
    81e8:	4d02                	moveq 13,r0

000081ea <sym14>:
    81ea:	4e02                	moveq 14,r0

000081ec <sym15>:
    81ec:	4f02                	moveq 15,r0

000081ee <sym16>:
    81ee:	5002                	moveq 16,r0

000081f0 <sym17>:
    81f0:	5102                	moveq 17,r0

000081f2 <sym18>:
    81f2:	5202                	moveq 18,r0

000081f4 <sym19>:
    81f4:	5302                	moveq 19,r0

000081f6 <sym20>:
    81f6:	5402                	moveq 20,r0

000081f8 <sym21>:
    81f8:	5502                	moveq 21,r0

000081fa <sym22>:
    81fa:	5602                	moveq 22,r0

000081fc <sym23>:
    81fc:	5702                	moveq 23,r0

000081fe <sym24>:
    81fe:	5802                	moveq 24,r0

00008200 <sym25>:
    8200:	5902                	moveq 25,r0

00008202 <sym26>:
    8202:	5a02                	moveq 26,r0

00008204 <sym27>:
    8204:	5b02                	moveq 27,r0

00008206 <sym28>:
    8206:	5c02                	moveq 28,r0

00008208 <sym29>:
    8208:	5d02                	moveq 29,r0

0000820a <sym30>:
    820a:	5e02                	moveq 30,r0

0000820c <sym31>:
    820c:	5f02                	moveq 31,r0

0000820e <sym32>:
    820e:	6002                	moveq -32,r0

00008210 <sym33>:
    8210:	6102                	moveq -31,r0

00008212 <sym34>:
    8212:	6202                	moveq -30,r0

00008214 <sym35>:
    8214:	6302                	moveq -29,r0

00008216 <sym36>:
    8216:	6402                	moveq -28,r0

00008218 <sym37>:
    8218:	6502                	moveq -27,r0

0000821a <sym38>:
    821a:	6602                	moveq -26,r0

0000821c <sym39>:
    821c:	6702                	moveq -25,r0

0000821e <sym40>:
    821e:	6802                	moveq -24,r0

00008220 <sym41>:
    8220:	6902                	moveq -23,r0

00008222 <sym42>:
    8222:	6a02                	moveq -22,r0

00008224 <sym43>:
    8224:	6b02                	moveq -21,r0
	\.\.\.
