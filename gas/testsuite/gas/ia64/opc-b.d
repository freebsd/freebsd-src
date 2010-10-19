#as: -xnone -mhint.b=ok -mtune=itanium1
#objdump: -d
#name: ia64 opc-b

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
       0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
       6:	00 f8 15 00 20 00 	      \(p02\) br\.cond\.sptk\.few 0x2bf0
       c:	00 00 00 40       	            br\.few 0x0;;
      10:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      16:	00 f0 15 00 22 00 	      \(p02\) br\.cond\.sptk\.few\.clr 0x2bf0
      1c:	f0 ff ff 4c       	            br\.few\.clr 0x0;;
      20:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      26:	00 e8 15 00 20 00 	      \(p02\) br\.cond\.sptk\.few 0x2bf0
      2c:	e0 ff ff 48       	            br\.few 0x0;;
      30:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      36:	00 e0 15 00 22 00 	      \(p02\) br\.cond\.sptk\.few\.clr 0x2bf0
      3c:	d0 ff ff 4c       	            br\.few\.clr 0x0;;
      40:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      46:	00 dc 15 00 20 00 	      \(p02\) br\.cond\.sptk\.many 0x2bf0
      4c:	c8 ff ff 48       	            br\.many 0x0;;
      50:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      56:	00 d4 15 00 22 00 	      \(p02\) br\.cond\.sptk\.many\.clr 0x2bf0
      5c:	b8 ff ff 4c       	            br\.many\.clr 0x0;;
      60:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      66:	00 c8 15 80 20 00 	      \(p02\) br\.cond\.spnt\.few 0x2bf0
      6c:	a0 ff ff 49       	            br\.cond\.spnt\.few 0x0;;
      70:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      76:	00 c0 15 80 22 00 	      \(p02\) br\.cond\.spnt\.few\.clr 0x2bf0
      7c:	90 ff ff 4d       	            br\.cond\.spnt\.few\.clr 0x0;;
      80:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      86:	00 b8 15 80 20 00 	      \(p02\) br\.cond\.spnt\.few 0x2bf0
      8c:	80 ff ff 49       	            br\.cond\.spnt\.few 0x0;;
      90:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      96:	00 b0 15 80 22 00 	      \(p02\) br\.cond\.spnt\.few\.clr 0x2bf0
      9c:	70 ff ff 4d       	            br\.cond\.spnt\.few\.clr 0x0;;
      a0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      a6:	00 ac 15 80 20 00 	      \(p02\) br\.cond\.spnt\.many 0x2bf0
      ac:	68 ff ff 49       	            br\.cond\.spnt\.many 0x0;;
      b0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      b6:	00 a4 15 80 22 00 	      \(p02\) br\.cond\.spnt\.many\.clr 0x2bf0
      bc:	58 ff ff 4d       	            br\.cond\.spnt\.many\.clr 0x0;;
      c0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      c6:	00 98 15 00 21 00 	      \(p02\) br\.cond\.dptk\.few 0x2bf0
      cc:	40 ff ff 4a       	            br\.cond\.dptk\.few 0x0;;
      d0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      d6:	00 90 15 00 23 00 	      \(p02\) br\.cond\.dptk\.few\.clr 0x2bf0
      dc:	30 ff ff 4e       	            br\.cond\.dptk\.few\.clr 0x0;;
      e0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      e6:	00 88 15 00 21 00 	      \(p02\) br\.cond\.dptk\.few 0x2bf0
      ec:	20 ff ff 4a       	            br\.cond\.dptk\.few 0x0;;
      f0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
      f6:	00 80 15 00 23 00 	      \(p02\) br\.cond\.dptk\.few\.clr 0x2bf0
      fc:	10 ff ff 4e       	            br\.cond\.dptk\.few\.clr 0x0;;
     100:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     106:	00 7c 15 00 21 00 	      \(p02\) br\.cond\.dptk\.many 0x2bf0
     10c:	08 ff ff 4a       	            br\.cond\.dptk\.many 0x0;;
     110:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     116:	00 74 15 00 23 00 	      \(p02\) br\.cond\.dptk\.many\.clr 0x2bf0
     11c:	f8 fe ff 4e       	            br\.cond\.dptk\.many\.clr 0x0;;
     120:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     126:	00 68 15 80 21 00 	      \(p02\) br\.cond\.dpnt\.few 0x2bf0
     12c:	e0 fe ff 4b       	            br\.cond\.dpnt\.few 0x0;;
     130:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     136:	00 60 15 80 23 00 	      \(p02\) br\.cond\.dpnt\.few\.clr 0x2bf0
     13c:	d0 fe ff 4f       	            br\.cond\.dpnt\.few\.clr 0x0;;
     140:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     146:	00 58 15 80 21 00 	      \(p02\) br\.cond\.dpnt\.few 0x2bf0
     14c:	c0 fe ff 4b       	            br\.cond\.dpnt\.few 0x0;;
     150:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     156:	00 50 15 80 23 00 	      \(p02\) br\.cond\.dpnt\.few\.clr 0x2bf0
     15c:	b0 fe ff 4f       	            br\.cond\.dpnt\.few\.clr 0x0;;
     160:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     166:	00 4c 15 80 21 00 	      \(p02\) br\.cond\.dpnt\.many 0x2bf0
     16c:	a8 fe ff 4b       	            br\.cond\.dpnt\.many 0x0;;
     170:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     176:	00 44 15 80 23 00 	      \(p02\) br\.cond\.dpnt\.many\.clr 0x2bf0
     17c:	98 fe ff 4f       	            br\.cond\.dpnt\.many\.clr 0x0;;
     180:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     186:	00 00 00 00 10 41 	            nop\.b 0x0
     18c:	70 2a 00 40       	      \(p02\) br\.wexit\.sptk\.few 0x2bf0;;
     190:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     196:	00 00 00 00 10 40 	            nop\.b 0x0
     19c:	60 2a 00 40       	            br\.wexit\.sptk\.few 0x2bf0;;
     1a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1a6:	00 00 00 00 10 41 	            nop\.b 0x0
     1ac:	50 2a 00 44       	      \(p02\) br\.wexit\.sptk\.few\.clr 0x2bf0;;
     1b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1b6:	00 00 00 00 10 40 	            nop\.b 0x0
     1bc:	40 2a 00 44       	            br\.wexit\.sptk\.few\.clr 0x2bf0;;
     1c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1c6:	00 00 00 00 10 41 	            nop\.b 0x0
     1cc:	30 2a 00 40       	      \(p02\) br\.wexit\.sptk\.few 0x2bf0;;
     1d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1d6:	00 00 00 00 10 40 	            nop\.b 0x0
     1dc:	20 2a 00 40       	            br\.wexit\.sptk\.few 0x2bf0;;
     1e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1e6:	00 00 00 00 10 41 	            nop\.b 0x0
     1ec:	10 2a 00 44       	      \(p02\) br\.wexit\.sptk\.few\.clr 0x2bf0;;
     1f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     1f6:	00 00 00 00 10 40 	            nop\.b 0x0
     1fc:	00 2a 00 44       	            br\.wexit\.sptk\.few\.clr 0x2bf0;;
     200:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     206:	00 00 00 00 10 41 	            nop\.b 0x0
     20c:	f8 29 00 40       	      \(p02\) br\.wexit\.sptk\.many 0x2bf0;;
     210:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     216:	00 00 00 00 10 40 	            nop\.b 0x0
     21c:	e8 29 00 40       	            br\.wexit\.sptk\.many 0x2bf0;;
     220:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     226:	00 00 00 00 10 41 	            nop\.b 0x0
     22c:	d8 29 00 44       	      \(p02\) br\.wexit\.sptk\.many\.clr 0x2bf0;;
     230:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     236:	00 00 00 00 10 40 	            nop\.b 0x0
     23c:	c8 29 00 44       	            br\.wexit\.sptk\.many\.clr 0x2bf0;;
     240:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     246:	00 00 00 00 10 41 	            nop\.b 0x0
     24c:	b0 29 00 41       	      \(p02\) br\.wexit\.spnt\.few 0x2bf0;;
     250:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     256:	00 00 00 00 10 40 	            nop\.b 0x0
     25c:	a0 29 00 41       	            br\.wexit\.spnt\.few 0x2bf0;;
     260:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     266:	00 00 00 00 10 41 	            nop\.b 0x0
     26c:	90 29 00 45       	      \(p02\) br\.wexit\.spnt\.few\.clr 0x2bf0;;
     270:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     276:	00 00 00 00 10 40 	            nop\.b 0x0
     27c:	80 29 00 45       	            br\.wexit\.spnt\.few\.clr 0x2bf0;;
     280:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     286:	00 00 00 00 10 41 	            nop\.b 0x0
     28c:	70 29 00 41       	      \(p02\) br\.wexit\.spnt\.few 0x2bf0;;
     290:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     296:	00 00 00 00 10 40 	            nop\.b 0x0
     29c:	60 29 00 41       	            br\.wexit\.spnt\.few 0x2bf0;;
     2a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2a6:	00 00 00 00 10 41 	            nop\.b 0x0
     2ac:	50 29 00 45       	      \(p02\) br\.wexit\.spnt\.few\.clr 0x2bf0;;
     2b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2b6:	00 00 00 00 10 40 	            nop\.b 0x0
     2bc:	40 29 00 45       	            br\.wexit\.spnt\.few\.clr 0x2bf0;;
     2c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2c6:	00 00 00 00 10 41 	            nop\.b 0x0
     2cc:	38 29 00 41       	      \(p02\) br\.wexit\.spnt\.many 0x2bf0;;
     2d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2d6:	00 00 00 00 10 40 	            nop\.b 0x0
     2dc:	28 29 00 41       	            br\.wexit\.spnt\.many 0x2bf0;;
     2e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2e6:	00 00 00 00 10 41 	            nop\.b 0x0
     2ec:	18 29 00 45       	      \(p02\) br\.wexit\.spnt\.many\.clr 0x2bf0;;
     2f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     2f6:	00 00 00 00 10 40 	            nop\.b 0x0
     2fc:	08 29 00 45       	            br\.wexit\.spnt\.many\.clr 0x2bf0;;
     300:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     306:	00 00 00 00 10 41 	            nop\.b 0x0
     30c:	f0 28 00 42       	      \(p02\) br\.wexit\.dptk\.few 0x2bf0;;
     310:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     316:	00 00 00 00 10 40 	            nop\.b 0x0
     31c:	e0 28 00 42       	            br\.wexit\.dptk\.few 0x2bf0;;
     320:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     326:	00 00 00 00 10 41 	            nop\.b 0x0
     32c:	d0 28 00 46       	      \(p02\) br\.wexit\.dptk\.few\.clr 0x2bf0;;
     330:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     336:	00 00 00 00 10 40 	            nop\.b 0x0
     33c:	c0 28 00 46       	            br\.wexit\.dptk\.few\.clr 0x2bf0;;
     340:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     346:	00 00 00 00 10 41 	            nop\.b 0x0
     34c:	b0 28 00 42       	      \(p02\) br\.wexit\.dptk\.few 0x2bf0;;
     350:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     356:	00 00 00 00 10 40 	            nop\.b 0x0
     35c:	a0 28 00 42       	            br\.wexit\.dptk\.few 0x2bf0;;
     360:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     366:	00 00 00 00 10 41 	            nop\.b 0x0
     36c:	90 28 00 46       	      \(p02\) br\.wexit\.dptk\.few\.clr 0x2bf0;;
     370:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     376:	00 00 00 00 10 40 	            nop\.b 0x0
     37c:	80 28 00 46       	            br\.wexit\.dptk\.few\.clr 0x2bf0;;
     380:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     386:	00 00 00 00 10 41 	            nop\.b 0x0
     38c:	78 28 00 42       	      \(p02\) br\.wexit\.dptk\.many 0x2bf0;;
     390:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     396:	00 00 00 00 10 40 	            nop\.b 0x0
     39c:	68 28 00 42       	            br\.wexit\.dptk\.many 0x2bf0;;
     3a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3a6:	00 00 00 00 10 41 	            nop\.b 0x0
     3ac:	58 28 00 46       	      \(p02\) br\.wexit\.dptk\.many\.clr 0x2bf0;;
     3b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3b6:	00 00 00 00 10 40 	            nop\.b 0x0
     3bc:	48 28 00 46       	            br\.wexit\.dptk\.many\.clr 0x2bf0;;
     3c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3c6:	00 00 00 00 10 41 	            nop\.b 0x0
     3cc:	30 28 00 43       	      \(p02\) br\.wexit\.dpnt\.few 0x2bf0;;
     3d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3d6:	00 00 00 00 10 40 	            nop\.b 0x0
     3dc:	20 28 00 43       	            br\.wexit\.dpnt\.few 0x2bf0;;
     3e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3e6:	00 00 00 00 10 41 	            nop\.b 0x0
     3ec:	10 28 00 47       	      \(p02\) br\.wexit\.dpnt\.few\.clr 0x2bf0;;
     3f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     3f6:	00 00 00 00 10 40 	            nop\.b 0x0
     3fc:	00 28 00 47       	            br\.wexit\.dpnt\.few\.clr 0x2bf0;;
     400:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     406:	00 00 00 00 10 41 	            nop\.b 0x0
     40c:	f0 27 00 43       	      \(p02\) br\.wexit\.dpnt\.few 0x2bf0;;
     410:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     416:	00 00 00 00 10 40 	            nop\.b 0x0
     41c:	e0 27 00 43       	            br\.wexit\.dpnt\.few 0x2bf0;;
     420:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     426:	00 00 00 00 10 41 	            nop\.b 0x0
     42c:	d0 27 00 47       	      \(p02\) br\.wexit\.dpnt\.few\.clr 0x2bf0;;
     430:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     436:	00 00 00 00 10 40 	            nop\.b 0x0
     43c:	c0 27 00 47       	            br\.wexit\.dpnt\.few\.clr 0x2bf0;;
     440:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     446:	00 00 00 00 10 41 	            nop\.b 0x0
     44c:	b8 27 00 43       	      \(p02\) br\.wexit\.dpnt\.many 0x2bf0;;
     450:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     456:	00 00 00 00 10 40 	            nop\.b 0x0
     45c:	a8 27 00 43       	            br\.wexit\.dpnt\.many 0x2bf0;;
     460:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     466:	00 00 00 00 10 41 	            nop\.b 0x0
     46c:	98 27 00 47       	      \(p02\) br\.wexit\.dpnt\.many\.clr 0x2bf0;;
     470:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     476:	00 00 00 00 10 40 	            nop\.b 0x0
     47c:	88 27 00 47       	            br\.wexit\.dpnt\.many\.clr 0x2bf0;;
     480:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     486:	00 00 00 00 10 61 	            nop\.b 0x0
     48c:	70 27 00 40       	      \(p02\) br\.wtop\.sptk\.few 0x2bf0;;
     490:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     496:	00 00 00 00 10 60 	            nop\.b 0x0
     49c:	60 27 00 40       	            br\.wtop\.sptk\.few 0x2bf0;;
     4a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4a6:	00 00 00 00 10 61 	            nop\.b 0x0
     4ac:	50 27 00 44       	      \(p02\) br\.wtop\.sptk\.few\.clr 0x2bf0;;
     4b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4b6:	00 00 00 00 10 60 	            nop\.b 0x0
     4bc:	40 27 00 44       	            br\.wtop\.sptk\.few\.clr 0x2bf0;;
     4c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4c6:	00 00 00 00 10 61 	            nop\.b 0x0
     4cc:	30 27 00 40       	      \(p02\) br\.wtop\.sptk\.few 0x2bf0;;
     4d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4d6:	00 00 00 00 10 60 	            nop\.b 0x0
     4dc:	20 27 00 40       	            br\.wtop\.sptk\.few 0x2bf0;;
     4e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4e6:	00 00 00 00 10 61 	            nop\.b 0x0
     4ec:	10 27 00 44       	      \(p02\) br\.wtop\.sptk\.few\.clr 0x2bf0;;
     4f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     4f6:	00 00 00 00 10 60 	            nop\.b 0x0
     4fc:	00 27 00 44       	            br\.wtop\.sptk\.few\.clr 0x2bf0;;
     500:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     506:	00 00 00 00 10 61 	            nop\.b 0x0
     50c:	f8 26 00 40       	      \(p02\) br\.wtop\.sptk\.many 0x2bf0;;
     510:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     516:	00 00 00 00 10 60 	            nop\.b 0x0
     51c:	e8 26 00 40       	            br\.wtop\.sptk\.many 0x2bf0;;
     520:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     526:	00 00 00 00 10 61 	            nop\.b 0x0
     52c:	d8 26 00 44       	      \(p02\) br\.wtop\.sptk\.many\.clr 0x2bf0;;
     530:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     536:	00 00 00 00 10 60 	            nop\.b 0x0
     53c:	c8 26 00 44       	            br\.wtop\.sptk\.many\.clr 0x2bf0;;
     540:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     546:	00 00 00 00 10 61 	            nop\.b 0x0
     54c:	b0 26 00 41       	      \(p02\) br\.wtop\.spnt\.few 0x2bf0;;
     550:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     556:	00 00 00 00 10 60 	            nop\.b 0x0
     55c:	a0 26 00 41       	            br\.wtop\.spnt\.few 0x2bf0;;
     560:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     566:	00 00 00 00 10 61 	            nop\.b 0x0
     56c:	90 26 00 45       	      \(p02\) br\.wtop\.spnt\.few\.clr 0x2bf0;;
     570:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     576:	00 00 00 00 10 60 	            nop\.b 0x0
     57c:	80 26 00 45       	            br\.wtop\.spnt\.few\.clr 0x2bf0;;
     580:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     586:	00 00 00 00 10 61 	            nop\.b 0x0
     58c:	70 26 00 41       	      \(p02\) br\.wtop\.spnt\.few 0x2bf0;;
     590:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     596:	00 00 00 00 10 60 	            nop\.b 0x0
     59c:	60 26 00 41       	            br\.wtop\.spnt\.few 0x2bf0;;
     5a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5a6:	00 00 00 00 10 61 	            nop\.b 0x0
     5ac:	50 26 00 45       	      \(p02\) br\.wtop\.spnt\.few\.clr 0x2bf0;;
     5b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5b6:	00 00 00 00 10 60 	            nop\.b 0x0
     5bc:	40 26 00 45       	            br\.wtop\.spnt\.few\.clr 0x2bf0;;
     5c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5c6:	00 00 00 00 10 61 	            nop\.b 0x0
     5cc:	38 26 00 41       	      \(p02\) br\.wtop\.spnt\.many 0x2bf0;;
     5d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5d6:	00 00 00 00 10 60 	            nop\.b 0x0
     5dc:	28 26 00 41       	            br\.wtop\.spnt\.many 0x2bf0;;
     5e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5e6:	00 00 00 00 10 61 	            nop\.b 0x0
     5ec:	18 26 00 45       	      \(p02\) br\.wtop\.spnt\.many\.clr 0x2bf0;;
     5f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     5f6:	00 00 00 00 10 60 	            nop\.b 0x0
     5fc:	08 26 00 45       	            br\.wtop\.spnt\.many\.clr 0x2bf0;;
     600:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     606:	00 00 00 00 10 61 	            nop\.b 0x0
     60c:	f0 25 00 42       	      \(p02\) br\.wtop\.dptk\.few 0x2bf0;;
     610:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     616:	00 00 00 00 10 60 	            nop\.b 0x0
     61c:	e0 25 00 42       	            br\.wtop\.dptk\.few 0x2bf0;;
     620:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     626:	00 00 00 00 10 61 	            nop\.b 0x0
     62c:	d0 25 00 46       	      \(p02\) br\.wtop\.dptk\.few\.clr 0x2bf0;;
     630:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     636:	00 00 00 00 10 60 	            nop\.b 0x0
     63c:	c0 25 00 46       	            br\.wtop\.dptk\.few\.clr 0x2bf0;;
     640:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     646:	00 00 00 00 10 61 	            nop\.b 0x0
     64c:	b0 25 00 42       	      \(p02\) br\.wtop\.dptk\.few 0x2bf0;;
     650:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     656:	00 00 00 00 10 60 	            nop\.b 0x0
     65c:	a0 25 00 42       	            br\.wtop\.dptk\.few 0x2bf0;;
     660:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     666:	00 00 00 00 10 61 	            nop\.b 0x0
     66c:	90 25 00 46       	      \(p02\) br\.wtop\.dptk\.few\.clr 0x2bf0;;
     670:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     676:	00 00 00 00 10 60 	            nop\.b 0x0
     67c:	80 25 00 46       	            br\.wtop\.dptk\.few\.clr 0x2bf0;;
     680:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     686:	00 00 00 00 10 61 	            nop\.b 0x0
     68c:	78 25 00 42       	      \(p02\) br\.wtop\.dptk\.many 0x2bf0;;
     690:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     696:	00 00 00 00 10 60 	            nop\.b 0x0
     69c:	68 25 00 42       	            br\.wtop\.dptk\.many 0x2bf0;;
     6a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6a6:	00 00 00 00 10 61 	            nop\.b 0x0
     6ac:	58 25 00 46       	      \(p02\) br\.wtop\.dptk\.many\.clr 0x2bf0;;
     6b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6b6:	00 00 00 00 10 60 	            nop\.b 0x0
     6bc:	48 25 00 46       	            br\.wtop\.dptk\.many\.clr 0x2bf0;;
     6c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6c6:	00 00 00 00 10 61 	            nop\.b 0x0
     6cc:	30 25 00 43       	      \(p02\) br\.wtop\.dpnt\.few 0x2bf0;;
     6d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6d6:	00 00 00 00 10 60 	            nop\.b 0x0
     6dc:	20 25 00 43       	            br\.wtop\.dpnt\.few 0x2bf0;;
     6e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6e6:	00 00 00 00 10 61 	            nop\.b 0x0
     6ec:	10 25 00 47       	      \(p02\) br\.wtop\.dpnt\.few\.clr 0x2bf0;;
     6f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     6f6:	00 00 00 00 10 60 	            nop\.b 0x0
     6fc:	00 25 00 47       	            br\.wtop\.dpnt\.few\.clr 0x2bf0;;
     700:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     706:	00 00 00 00 10 61 	            nop\.b 0x0
     70c:	f0 24 00 43       	      \(p02\) br\.wtop\.dpnt\.few 0x2bf0;;
     710:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     716:	00 00 00 00 10 60 	            nop\.b 0x0
     71c:	e0 24 00 43       	            br\.wtop\.dpnt\.few 0x2bf0;;
     720:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     726:	00 00 00 00 10 61 	            nop\.b 0x0
     72c:	d0 24 00 47       	      \(p02\) br\.wtop\.dpnt\.few\.clr 0x2bf0;;
     730:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     736:	00 00 00 00 10 60 	            nop\.b 0x0
     73c:	c0 24 00 47       	            br\.wtop\.dpnt\.few\.clr 0x2bf0;;
     740:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     746:	00 00 00 00 10 61 	            nop\.b 0x0
     74c:	b8 24 00 43       	      \(p02\) br\.wtop\.dpnt\.many 0x2bf0;;
     750:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     756:	00 00 00 00 10 60 	            nop\.b 0x0
     75c:	a8 24 00 43       	            br\.wtop\.dpnt\.many 0x2bf0;;
     760:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     766:	00 00 00 00 10 61 	            nop\.b 0x0
     76c:	98 24 00 47       	      \(p02\) br\.wtop\.dpnt\.many\.clr 0x2bf0;;
     770:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     776:	00 00 00 00 10 60 	            nop\.b 0x0
     77c:	88 24 00 47       	            br\.wtop\.dpnt\.many\.clr 0x2bf0;;
     780:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     786:	00 00 00 00 10 a0 	            nop\.b 0x0
     78c:	70 24 00 40       	            br\.cloop\.sptk\.few 0x2bf0;;
     790:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     796:	00 00 00 00 10 a0 	            nop\.b 0x0
     79c:	60 24 00 44       	            br\.cloop\.sptk\.few\.clr 0x2bf0;;
     7a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7a6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7ac:	50 24 00 40       	            br\.cloop\.sptk\.few 0x2bf0;;
     7b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7b6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7bc:	40 24 00 44       	            br\.cloop\.sptk\.few\.clr 0x2bf0;;
     7c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7c6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7cc:	38 24 00 40       	            br\.cloop\.sptk\.many 0x2bf0;;
     7d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7d6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7dc:	28 24 00 44       	            br\.cloop\.sptk\.many\.clr 0x2bf0;;
     7e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7e6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7ec:	10 24 00 41       	            br\.cloop\.spnt\.few 0x2bf0;;
     7f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     7f6:	00 00 00 00 10 a0 	            nop\.b 0x0
     7fc:	00 24 00 45       	            br\.cloop\.spnt\.few\.clr 0x2bf0;;
     800:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     806:	00 00 00 00 10 a0 	            nop\.b 0x0
     80c:	f0 23 00 41       	            br\.cloop\.spnt\.few 0x2bf0;;
     810:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     816:	00 00 00 00 10 a0 	            nop\.b 0x0
     81c:	e0 23 00 45       	            br\.cloop\.spnt\.few\.clr 0x2bf0;;
     820:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     826:	00 00 00 00 10 a0 	            nop\.b 0x0
     82c:	d8 23 00 41       	            br\.cloop\.spnt\.many 0x2bf0;;
     830:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     836:	00 00 00 00 10 a0 	            nop\.b 0x0
     83c:	c8 23 00 45       	            br\.cloop\.spnt\.many\.clr 0x2bf0;;
     840:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     846:	00 00 00 00 10 a0 	            nop\.b 0x0
     84c:	b0 23 00 42       	            br\.cloop\.dptk\.few 0x2bf0;;
     850:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     856:	00 00 00 00 10 a0 	            nop\.b 0x0
     85c:	a0 23 00 46       	            br\.cloop\.dptk\.few\.clr 0x2bf0;;
     860:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     866:	00 00 00 00 10 a0 	            nop\.b 0x0
     86c:	90 23 00 42       	            br\.cloop\.dptk\.few 0x2bf0;;
     870:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     876:	00 00 00 00 10 a0 	            nop\.b 0x0
     87c:	80 23 00 46       	            br\.cloop\.dptk\.few\.clr 0x2bf0;;
     880:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     886:	00 00 00 00 10 a0 	            nop\.b 0x0
     88c:	78 23 00 42       	            br\.cloop\.dptk\.many 0x2bf0;;
     890:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     896:	00 00 00 00 10 a0 	            nop\.b 0x0
     89c:	68 23 00 46       	            br\.cloop\.dptk\.many\.clr 0x2bf0;;
     8a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8a6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8ac:	50 23 00 43       	            br\.cloop\.dpnt\.few 0x2bf0;;
     8b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8b6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8bc:	40 23 00 47       	            br\.cloop\.dpnt\.few\.clr 0x2bf0;;
     8c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8c6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8cc:	30 23 00 43       	            br\.cloop\.dpnt\.few 0x2bf0;;
     8d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8d6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8dc:	20 23 00 47       	            br\.cloop\.dpnt\.few\.clr 0x2bf0;;
     8e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8e6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8ec:	18 23 00 43       	            br\.cloop\.dpnt\.many 0x2bf0;;
     8f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     8f6:	00 00 00 00 10 a0 	            nop\.b 0x0
     8fc:	08 23 00 47       	            br\.cloop\.dpnt\.many\.clr 0x2bf0;;
     900:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     906:	00 00 00 00 10 c0 	            nop\.b 0x0
     90c:	f0 22 00 40       	            br\.cexit\.sptk\.few 0x2bf0;;
     910:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     916:	00 00 00 00 10 c0 	            nop\.b 0x0
     91c:	e0 22 00 44       	            br\.cexit\.sptk\.few\.clr 0x2bf0;;
     920:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     926:	00 00 00 00 10 c0 	            nop\.b 0x0
     92c:	d0 22 00 40       	            br\.cexit\.sptk\.few 0x2bf0;;
     930:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     936:	00 00 00 00 10 c0 	            nop\.b 0x0
     93c:	c0 22 00 44       	            br\.cexit\.sptk\.few\.clr 0x2bf0;;
     940:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     946:	00 00 00 00 10 c0 	            nop\.b 0x0
     94c:	b8 22 00 40       	            br\.cexit\.sptk\.many 0x2bf0;;
     950:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     956:	00 00 00 00 10 c0 	            nop\.b 0x0
     95c:	a8 22 00 44       	            br\.cexit\.sptk\.many\.clr 0x2bf0;;
     960:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     966:	00 00 00 00 10 c0 	            nop\.b 0x0
     96c:	90 22 00 41       	            br\.cexit\.spnt\.few 0x2bf0;;
     970:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     976:	00 00 00 00 10 c0 	            nop\.b 0x0
     97c:	80 22 00 45       	            br\.cexit\.spnt\.few\.clr 0x2bf0;;
     980:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     986:	00 00 00 00 10 c0 	            nop\.b 0x0
     98c:	70 22 00 41       	            br\.cexit\.spnt\.few 0x2bf0;;
     990:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     996:	00 00 00 00 10 c0 	            nop\.b 0x0
     99c:	60 22 00 45       	            br\.cexit\.spnt\.few\.clr 0x2bf0;;
     9a0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9a6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9ac:	58 22 00 41       	            br\.cexit\.spnt\.many 0x2bf0;;
     9b0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9b6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9bc:	48 22 00 45       	            br\.cexit\.spnt\.many\.clr 0x2bf0;;
     9c0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9c6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9cc:	30 22 00 42       	            br\.cexit\.dptk\.few 0x2bf0;;
     9d0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9d6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9dc:	20 22 00 46       	            br\.cexit\.dptk\.few\.clr 0x2bf0;;
     9e0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9e6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9ec:	10 22 00 42       	            br\.cexit\.dptk\.few 0x2bf0;;
     9f0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     9f6:	00 00 00 00 10 c0 	            nop\.b 0x0
     9fc:	00 22 00 46       	            br\.cexit\.dptk\.few\.clr 0x2bf0;;
     a00:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a06:	00 00 00 00 10 c0 	            nop\.b 0x0
     a0c:	f8 21 00 42       	            br\.cexit\.dptk\.many 0x2bf0;;
     a10:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a16:	00 00 00 00 10 c0 	            nop\.b 0x0
     a1c:	e8 21 00 46       	            br\.cexit\.dptk\.many\.clr 0x2bf0;;
     a20:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a26:	00 00 00 00 10 c0 	            nop\.b 0x0
     a2c:	d0 21 00 43       	            br\.cexit\.dpnt\.few 0x2bf0;;
     a30:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a36:	00 00 00 00 10 c0 	            nop\.b 0x0
     a3c:	c0 21 00 47       	            br\.cexit\.dpnt\.few\.clr 0x2bf0;;
     a40:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a46:	00 00 00 00 10 c0 	            nop\.b 0x0
     a4c:	b0 21 00 43       	            br\.cexit\.dpnt\.few 0x2bf0;;
     a50:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a56:	00 00 00 00 10 c0 	            nop\.b 0x0
     a5c:	a0 21 00 47       	            br\.cexit\.dpnt\.few\.clr 0x2bf0;;
     a60:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a66:	00 00 00 00 10 c0 	            nop\.b 0x0
     a6c:	98 21 00 43       	            br\.cexit\.dpnt\.many 0x2bf0;;
     a70:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a76:	00 00 00 00 10 c0 	            nop\.b 0x0
     a7c:	88 21 00 47       	            br\.cexit\.dpnt\.many\.clr 0x2bf0;;
     a80:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a86:	00 00 00 00 10 e0 	            nop\.b 0x0
     a8c:	70 21 00 40       	            br\.ctop\.sptk\.few 0x2bf0;;
     a90:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     a96:	00 00 00 00 10 e0 	            nop\.b 0x0
     a9c:	60 21 00 44       	            br\.ctop\.sptk\.few\.clr 0x2bf0;;
     aa0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     aa6:	00 00 00 00 10 e0 	            nop\.b 0x0
     aac:	50 21 00 40       	            br\.ctop\.sptk\.few 0x2bf0;;
     ab0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ab6:	00 00 00 00 10 e0 	            nop\.b 0x0
     abc:	40 21 00 44       	            br\.ctop\.sptk\.few\.clr 0x2bf0;;
     ac0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ac6:	00 00 00 00 10 e0 	            nop\.b 0x0
     acc:	38 21 00 40       	            br\.ctop\.sptk\.many 0x2bf0;;
     ad0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ad6:	00 00 00 00 10 e0 	            nop\.b 0x0
     adc:	28 21 00 44       	            br\.ctop\.sptk\.many\.clr 0x2bf0;;
     ae0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ae6:	00 00 00 00 10 e0 	            nop\.b 0x0
     aec:	10 21 00 41       	            br\.ctop\.spnt\.few 0x2bf0;;
     af0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     af6:	00 00 00 00 10 e0 	            nop\.b 0x0
     afc:	00 21 00 45       	            br\.ctop\.spnt\.few\.clr 0x2bf0;;
     b00:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b06:	00 00 00 00 10 e0 	            nop\.b 0x0
     b0c:	f0 20 00 41       	            br\.ctop\.spnt\.few 0x2bf0;;
     b10:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b16:	00 00 00 00 10 e0 	            nop\.b 0x0
     b1c:	e0 20 00 45       	            br\.ctop\.spnt\.few\.clr 0x2bf0;;
     b20:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b26:	00 00 00 00 10 e0 	            nop\.b 0x0
     b2c:	d8 20 00 41       	            br\.ctop\.spnt\.many 0x2bf0;;
     b30:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b36:	00 00 00 00 10 e0 	            nop\.b 0x0
     b3c:	c8 20 00 45       	            br\.ctop\.spnt\.many\.clr 0x2bf0;;
     b40:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b46:	00 00 00 00 10 e0 	            nop\.b 0x0
     b4c:	b0 20 00 42       	            br\.ctop\.dptk\.few 0x2bf0;;
     b50:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b56:	00 00 00 00 10 e0 	            nop\.b 0x0
     b5c:	a0 20 00 46       	            br\.ctop\.dptk\.few\.clr 0x2bf0;;
     b60:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b66:	00 00 00 00 10 e0 	            nop\.b 0x0
     b6c:	90 20 00 42       	            br\.ctop\.dptk\.few 0x2bf0;;
     b70:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b76:	00 00 00 00 10 e0 	            nop\.b 0x0
     b7c:	80 20 00 46       	            br\.ctop\.dptk\.few\.clr 0x2bf0;;
     b80:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b86:	00 00 00 00 10 e0 	            nop\.b 0x0
     b8c:	78 20 00 42       	            br\.ctop\.dptk\.many 0x2bf0;;
     b90:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     b96:	00 00 00 00 10 e0 	            nop\.b 0x0
     b9c:	68 20 00 46       	            br\.ctop\.dptk\.many\.clr 0x2bf0;;
     ba0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ba6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bac:	50 20 00 43       	            br\.ctop\.dpnt\.few 0x2bf0;;
     bb0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     bb6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bbc:	40 20 00 47       	            br\.ctop\.dpnt\.few\.clr 0x2bf0;;
     bc0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     bc6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bcc:	30 20 00 43       	            br\.ctop\.dpnt\.few 0x2bf0;;
     bd0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     bd6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bdc:	20 20 00 47       	            br\.ctop\.dpnt\.few\.clr 0x2bf0;;
     be0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     be6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bec:	18 20 00 43       	            br\.ctop\.dpnt\.many 0x2bf0;;
     bf0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     bf6:	00 00 00 00 10 e0 	            nop\.b 0x0
     bfc:	08 20 00 47       	            br\.ctop\.dpnt\.many\.clr 0x2bf0;;
     c00:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c06:	00 f8 0f 00 28 00 	      \(p02\) br\.call\.sptk\.few b0=0x2bf0
     c0c:	00 f4 ff 58       	            br\.call\.sptk\.few b0=0x0;;
     c10:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c16:	00 f0 0f 00 2a 00 	      \(p02\) br\.call\.sptk\.few\.clr b0=0x2bf0
     c1c:	f0 f3 ff 5c       	            br\.call\.sptk\.few\.clr b0=0x0;;
     c20:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c26:	00 e8 0f 00 28 00 	      \(p02\) br\.call\.sptk\.few b0=0x2bf0
     c2c:	e0 f3 ff 58       	            br\.call\.sptk\.few b0=0x0;;
     c30:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c36:	00 e0 0f 00 2a 00 	      \(p02\) br\.call\.sptk\.few\.clr b0=0x2bf0
     c3c:	d0 f3 ff 5c       	            br\.call\.sptk\.few\.clr b0=0x0;;
     c40:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c46:	00 dc 0f 00 28 00 	      \(p02\) br\.call\.sptk\.many b0=0x2bf0
     c4c:	c8 f3 ff 58       	            br\.call\.sptk\.many b0=0x0;;
     c50:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c56:	00 d4 0f 00 2a 00 	      \(p02\) br\.call\.sptk\.many\.clr b0=0x2bf0
     c5c:	b8 f3 ff 5c       	            br\.call\.sptk\.many\.clr b0=0x0;;
     c60:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c66:	00 c8 0f 80 28 00 	      \(p02\) br\.call\.spnt\.few b0=0x2bf0
     c6c:	a0 f3 ff 59       	            br\.call\.spnt\.few b0=0x0;;
     c70:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c76:	00 c0 0f 80 2a 00 	      \(p02\) br\.call\.spnt\.few\.clr b0=0x2bf0
     c7c:	90 f3 ff 5d       	            br\.call\.spnt\.few\.clr b0=0x0;;
     c80:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c86:	00 b8 0f 80 28 00 	      \(p02\) br\.call\.spnt\.few b0=0x2bf0
     c8c:	80 f3 ff 59       	            br\.call\.spnt\.few b0=0x0;;
     c90:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     c96:	00 b0 0f 80 2a 00 	      \(p02\) br\.call\.spnt\.few\.clr b0=0x2bf0
     c9c:	70 f3 ff 5d       	            br\.call\.spnt\.few\.clr b0=0x0;;
     ca0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ca6:	00 ac 0f 80 28 00 	      \(p02\) br\.call\.spnt\.many b0=0x2bf0
     cac:	68 f3 ff 59       	            br\.call\.spnt\.many b0=0x0;;
     cb0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     cb6:	00 a4 0f 80 2a 00 	      \(p02\) br\.call\.spnt\.many\.clr b0=0x2bf0
     cbc:	58 f3 ff 5d       	            br\.call\.spnt\.many\.clr b0=0x0;;
     cc0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     cc6:	00 98 0f 00 29 00 	      \(p02\) br\.call\.dptk\.few b0=0x2bf0
     ccc:	40 f3 ff 5a       	            br\.call\.dptk\.few b0=0x0;;
     cd0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     cd6:	00 90 0f 00 2b 00 	      \(p02\) br\.call\.dptk\.few\.clr b0=0x2bf0
     cdc:	30 f3 ff 5e       	            br\.call\.dptk\.few\.clr b0=0x0;;
     ce0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ce6:	00 88 0f 00 29 00 	      \(p02\) br\.call\.dptk\.few b0=0x2bf0
     cec:	20 f3 ff 5a       	            br\.call\.dptk\.few b0=0x0;;
     cf0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     cf6:	00 80 0f 00 2b 00 	      \(p02\) br\.call\.dptk\.few\.clr b0=0x2bf0
     cfc:	10 f3 ff 5e       	            br\.call\.dptk\.few\.clr b0=0x0;;
     d00:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d06:	00 7c 0f 00 29 00 	      \(p02\) br\.call\.dptk\.many b0=0x2bf0
     d0c:	08 f3 ff 5a       	            br\.call\.dptk\.many b0=0x0;;
     d10:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d16:	00 74 0f 00 2b 00 	      \(p02\) br\.call\.dptk\.many\.clr b0=0x2bf0
     d1c:	f8 f2 ff 5e       	            br\.call\.dptk\.many\.clr b0=0x0;;
     d20:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d26:	00 68 0f 80 29 00 	      \(p02\) br\.call\.dpnt\.few b0=0x2bf0
     d2c:	e0 f2 ff 5b       	            br\.call\.dpnt\.few b0=0x0;;
     d30:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d36:	00 60 0f 80 2b 00 	      \(p02\) br\.call\.dpnt\.few\.clr b0=0x2bf0
     d3c:	d0 f2 ff 5f       	            br\.call\.dpnt\.few\.clr b0=0x0;;
     d40:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d46:	00 58 0f 80 29 00 	      \(p02\) br\.call\.dpnt\.few b0=0x2bf0
     d4c:	c0 f2 ff 5b       	            br\.call\.dpnt\.few b0=0x0;;
     d50:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d56:	00 50 0f 80 2b 00 	      \(p02\) br\.call\.dpnt\.few\.clr b0=0x2bf0
     d5c:	b0 f2 ff 5f       	            br\.call\.dpnt\.few\.clr b0=0x0;;
     d60:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d66:	00 4c 0f 80 29 00 	      \(p02\) br\.call\.dpnt\.many b0=0x2bf0
     d6c:	a8 f2 ff 5b       	            br\.call\.dpnt\.many b0=0x0;;
     d70:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d76:	00 44 0f 80 2b 00 	      \(p02\) br\.call\.dpnt\.many\.clr b0=0x2bf0
     d7c:	98 f2 ff 5f       	            br\.call\.dpnt\.many\.clr b0=0x0;;
     d80:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d86:	00 10 00 40 00 00 	      \(p02\) br\.cond\.sptk\.few b2
     d8c:	20 00 80 00       	            br\.few b2;;
     d90:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     d96:	00 10 00 40 02 00 	      \(p02\) br\.cond\.sptk\.few\.clr b2
     d9c:	20 00 80 04       	            br\.few\.clr b2;;
     da0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     da6:	00 10 00 40 00 00 	      \(p02\) br\.cond\.sptk\.few b2
     dac:	20 00 80 00       	            br\.few b2;;
     db0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     db6:	00 10 00 40 02 00 	      \(p02\) br\.cond\.sptk\.few\.clr b2
     dbc:	20 00 80 04       	            br\.few\.clr b2;;
     dc0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     dc6:	00 14 00 40 00 00 	      \(p02\) br\.cond\.sptk\.many b2
     dcc:	28 00 80 00       	            br\.many b2;;
     dd0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     dd6:	00 14 00 40 02 00 	      \(p02\) br\.cond\.sptk\.many\.clr b2
     ddc:	28 00 80 04       	            br\.many\.clr b2;;
     de0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     de6:	00 10 00 c0 00 00 	      \(p02\) br\.cond\.spnt\.few b2
     dec:	20 00 80 01       	            br\.cond\.spnt\.few b2;;
     df0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     df6:	00 10 00 c0 02 00 	      \(p02\) br\.cond\.spnt\.few\.clr b2
     dfc:	20 00 80 05       	            br\.cond\.spnt\.few\.clr b2;;
     e00:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e06:	00 10 00 c0 00 00 	      \(p02\) br\.cond\.spnt\.few b2
     e0c:	20 00 80 01       	            br\.cond\.spnt\.few b2;;
     e10:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e16:	00 10 00 c0 02 00 	      \(p02\) br\.cond\.spnt\.few\.clr b2
     e1c:	20 00 80 05       	            br\.cond\.spnt\.few\.clr b2;;
     e20:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e26:	00 14 00 c0 00 00 	      \(p02\) br\.cond\.spnt\.many b2
     e2c:	28 00 80 01       	            br\.cond\.spnt\.many b2;;
     e30:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e36:	00 14 00 c0 02 00 	      \(p02\) br\.cond\.spnt\.many\.clr b2
     e3c:	28 00 80 05       	            br\.cond\.spnt\.many\.clr b2;;
     e40:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e46:	00 10 00 40 01 00 	      \(p02\) br\.cond\.dptk\.few b2
     e4c:	20 00 80 02       	            br\.cond\.dptk\.few b2;;
     e50:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e56:	00 10 00 40 03 00 	      \(p02\) br\.cond\.dptk\.few\.clr b2
     e5c:	20 00 80 06       	            br\.cond\.dptk\.few\.clr b2;;
     e60:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e66:	00 10 00 40 01 00 	      \(p02\) br\.cond\.dptk\.few b2
     e6c:	20 00 80 02       	            br\.cond\.dptk\.few b2;;
     e70:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e76:	00 10 00 40 03 00 	      \(p02\) br\.cond\.dptk\.few\.clr b2
     e7c:	20 00 80 06       	            br\.cond\.dptk\.few\.clr b2;;
     e80:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e86:	00 14 00 40 01 00 	      \(p02\) br\.cond\.dptk\.many b2
     e8c:	28 00 80 02       	            br\.cond\.dptk\.many b2;;
     e90:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     e96:	00 14 00 40 03 00 	      \(p02\) br\.cond\.dptk\.many\.clr b2
     e9c:	28 00 80 06       	            br\.cond\.dptk\.many\.clr b2;;
     ea0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ea6:	00 10 00 c0 01 00 	      \(p02\) br\.cond\.dpnt\.few b2
     eac:	20 00 80 03       	            br\.cond\.dpnt\.few b2;;
     eb0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     eb6:	00 10 00 c0 03 00 	      \(p02\) br\.cond\.dpnt\.few\.clr b2
     ebc:	20 00 80 07       	            br\.cond\.dpnt\.few\.clr b2;;
     ec0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ec6:	00 10 00 c0 01 00 	      \(p02\) br\.cond\.dpnt\.few b2
     ecc:	20 00 80 03       	            br\.cond\.dpnt\.few b2;;
     ed0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ed6:	00 10 00 c0 03 00 	      \(p02\) br\.cond\.dpnt\.few\.clr b2
     edc:	20 00 80 07       	            br\.cond\.dpnt\.few\.clr b2;;
     ee0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ee6:	00 14 00 c0 01 00 	      \(p02\) br\.cond\.dpnt\.many b2
     eec:	28 00 80 03       	            br\.cond\.dpnt\.many b2;;
     ef0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
     ef6:	00 14 00 c0 03 00 	      \(p02\) br\.cond\.dpnt\.many\.clr b2
     efc:	28 00 80 07       	            br\.cond\.dpnt\.many\.clr b2;;
     f00:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f06:	00 00 00 00 10 20 	            nop\.b 0x0
     f0c:	20 00 80 00       	            br\.ia\.sptk\.few b2;;
     f10:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f16:	00 00 00 00 10 20 	            nop\.b 0x0
     f1c:	20 00 80 04       	            br\.ia\.sptk\.few\.clr b2;;
     f20:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f26:	00 00 00 00 10 20 	            nop\.b 0x0
     f2c:	20 00 80 00       	            br\.ia\.sptk\.few b2;;
     f30:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f36:	00 00 00 00 10 20 	            nop\.b 0x0
     f3c:	20 00 80 04       	            br\.ia\.sptk\.few\.clr b2;;
     f40:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f46:	00 00 00 00 10 20 	            nop\.b 0x0
     f4c:	28 00 80 00       	            br\.ia\.sptk\.many b2;;
     f50:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f56:	00 00 00 00 10 20 	            nop\.b 0x0
     f5c:	28 00 80 04       	            br\.ia\.sptk\.many\.clr b2;;
     f60:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f66:	00 00 00 00 10 20 	            nop\.b 0x0
     f6c:	20 00 80 01       	            br\.ia\.spnt\.few b2;;
     f70:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f76:	00 00 00 00 10 20 	            nop\.b 0x0
     f7c:	20 00 80 05       	            br\.ia\.spnt\.few\.clr b2;;
     f80:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f86:	00 00 00 00 10 20 	            nop\.b 0x0
     f8c:	20 00 80 01       	            br\.ia\.spnt\.few b2;;
     f90:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     f96:	00 00 00 00 10 20 	            nop\.b 0x0
     f9c:	20 00 80 05       	            br\.ia\.spnt\.few\.clr b2;;
     fa0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     fa6:	00 00 00 00 10 20 	            nop\.b 0x0
     fac:	28 00 80 01       	            br\.ia\.spnt\.many b2;;
     fb0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     fb6:	00 00 00 00 10 20 	            nop\.b 0x0
     fbc:	28 00 80 05       	            br\.ia\.spnt\.many\.clr b2;;
     fc0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     fc6:	00 00 00 00 10 20 	            nop\.b 0x0
     fcc:	20 00 80 02       	            br\.ia\.dptk\.few b2;;
     fd0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     fd6:	00 00 00 00 10 20 	            nop\.b 0x0
     fdc:	20 00 80 06       	            br\.ia\.dptk\.few\.clr b2;;
     fe0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     fe6:	00 00 00 00 10 20 	            nop\.b 0x0
     fec:	20 00 80 02       	            br\.ia\.dptk\.few b2;;
     ff0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
     ff6:	00 00 00 00 10 20 	            nop\.b 0x0
     ffc:	20 00 80 06       	            br\.ia\.dptk\.few\.clr b2;;
    1000:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1006:	00 00 00 00 10 20 	            nop\.b 0x0
    100c:	28 00 80 02       	            br\.ia\.dptk\.many b2;;
    1010:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1016:	00 00 00 00 10 20 	            nop\.b 0x0
    101c:	28 00 80 06       	            br\.ia\.dptk\.many\.clr b2;;
    1020:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1026:	00 00 00 00 10 20 	            nop\.b 0x0
    102c:	20 00 80 03       	            br\.ia\.dpnt\.few b2;;
    1030:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1036:	00 00 00 00 10 20 	            nop\.b 0x0
    103c:	20 00 80 07       	            br\.ia\.dpnt\.few\.clr b2;;
    1040:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1046:	00 00 00 00 10 20 	            nop\.b 0x0
    104c:	20 00 80 03       	            br\.ia\.dpnt\.few b2;;
    1050:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1056:	00 00 00 00 10 20 	            nop\.b 0x0
    105c:	20 00 80 07       	            br\.ia\.dpnt\.few\.clr b2;;
    1060:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1066:	00 00 00 00 10 20 	            nop\.b 0x0
    106c:	28 00 80 03       	            br\.ia\.dpnt\.many b2;;
    1070:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    1076:	00 00 00 00 10 20 	            nop\.b 0x0
    107c:	28 00 80 07       	            br\.ia\.dpnt\.many\.clr b2;;
    1080:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1086:	40 10 00 42 00 80 	      \(p02\) br\.ret\.sptk\.few b2
    108c:	20 00 84 00       	            br\.ret\.sptk\.few b2;;
    1090:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1096:	40 10 00 42 02 80 	      \(p02\) br\.ret\.sptk\.few\.clr b2
    109c:	20 00 84 04       	            br\.ret\.sptk\.few\.clr b2;;
    10a0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10a6:	40 10 00 42 00 80 	      \(p02\) br\.ret\.sptk\.few b2
    10ac:	20 00 84 00       	            br\.ret\.sptk\.few b2;;
    10b0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10b6:	40 10 00 42 02 80 	      \(p02\) br\.ret\.sptk\.few\.clr b2
    10bc:	20 00 84 04       	            br\.ret\.sptk\.few\.clr b2;;
    10c0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10c6:	40 14 00 42 00 80 	      \(p02\) br\.ret\.sptk\.many b2
    10cc:	28 00 84 00       	            br\.ret\.sptk\.many b2;;
    10d0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10d6:	40 14 00 42 02 80 	      \(p02\) br\.ret\.sptk\.many\.clr b2
    10dc:	28 00 84 04       	            br\.ret\.sptk\.many\.clr b2;;
    10e0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10e6:	40 10 00 c2 00 80 	      \(p02\) br\.ret\.spnt\.few b2
    10ec:	20 00 84 01       	            br\.ret\.spnt\.few b2;;
    10f0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    10f6:	40 10 00 c2 02 80 	      \(p02\) br\.ret\.spnt\.few\.clr b2
    10fc:	20 00 84 05       	            br\.ret\.spnt\.few\.clr b2;;
    1100:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1106:	40 10 00 c2 00 80 	      \(p02\) br\.ret\.spnt\.few b2
    110c:	20 00 84 01       	            br\.ret\.spnt\.few b2;;
    1110:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1116:	40 10 00 c2 02 80 	      \(p02\) br\.ret\.spnt\.few\.clr b2
    111c:	20 00 84 05       	            br\.ret\.spnt\.few\.clr b2;;
    1120:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1126:	40 14 00 c2 00 80 	      \(p02\) br\.ret\.spnt\.many b2
    112c:	28 00 84 01       	            br\.ret\.spnt\.many b2;;
    1130:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1136:	40 14 00 c2 02 80 	      \(p02\) br\.ret\.spnt\.many\.clr b2
    113c:	28 00 84 05       	            br\.ret\.spnt\.many\.clr b2;;
    1140:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1146:	40 10 00 42 01 80 	      \(p02\) br\.ret\.dptk\.few b2
    114c:	20 00 84 02       	            br\.ret\.dptk\.few b2;;
    1150:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1156:	40 10 00 42 03 80 	      \(p02\) br\.ret\.dptk\.few\.clr b2
    115c:	20 00 84 06       	            br\.ret\.dptk\.few\.clr b2;;
    1160:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1166:	40 10 00 42 01 80 	      \(p02\) br\.ret\.dptk\.few b2
    116c:	20 00 84 02       	            br\.ret\.dptk\.few b2;;
    1170:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1176:	40 10 00 42 03 80 	      \(p02\) br\.ret\.dptk\.few\.clr b2
    117c:	20 00 84 06       	            br\.ret\.dptk\.few\.clr b2;;
    1180:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1186:	40 14 00 42 01 80 	      \(p02\) br\.ret\.dptk\.many b2
    118c:	28 00 84 02       	            br\.ret\.dptk\.many b2;;
    1190:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1196:	40 14 00 42 03 80 	      \(p02\) br\.ret\.dptk\.many\.clr b2
    119c:	28 00 84 06       	            br\.ret\.dptk\.many\.clr b2;;
    11a0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11a6:	40 10 00 c2 01 80 	      \(p02\) br\.ret\.dpnt\.few b2
    11ac:	20 00 84 03       	            br\.ret\.dpnt\.few b2;;
    11b0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11b6:	40 10 00 c2 03 80 	      \(p02\) br\.ret\.dpnt\.few\.clr b2
    11bc:	20 00 84 07       	            br\.ret\.dpnt\.few\.clr b2;;
    11c0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11c6:	40 10 00 c2 01 80 	      \(p02\) br\.ret\.dpnt\.few b2
    11cc:	20 00 84 03       	            br\.ret\.dpnt\.few b2;;
    11d0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11d6:	40 10 00 c2 03 80 	      \(p02\) br\.ret\.dpnt\.few\.clr b2
    11dc:	20 00 84 07       	            br\.ret\.dpnt\.few\.clr b2;;
    11e0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11e6:	40 14 00 c2 01 80 	      \(p02\) br\.ret\.dpnt\.many b2
    11ec:	28 00 84 03       	            br\.ret\.dpnt\.many b2;;
    11f0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    11f6:	40 14 00 c2 03 80 	      \(p02\) br\.ret\.dpnt\.many\.clr b2
    11fc:	28 00 84 07       	            br\.ret\.dpnt\.many\.clr b2;;
    1200:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1206:	00 10 00 40 08 00 	      \(p02\) br\.call\.sptk\.few b0=b2
    120c:	20 00 80 10       	            br\.call\.sptk\.few b0=b2;;
    1210:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1216:	00 10 00 40 0a 00 	      \(p02\) br\.call\.sptk\.few\.clr b0=b2
    121c:	20 00 80 14       	            br\.call\.sptk\.few\.clr b0=b2;;
    1220:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1226:	00 10 00 40 08 00 	      \(p02\) br\.call\.sptk\.few b0=b2
    122c:	20 00 80 10       	            br\.call\.sptk\.few b0=b2;;
    1230:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1236:	00 10 00 40 0a 00 	      \(p02\) br\.call\.sptk\.few\.clr b0=b2
    123c:	20 00 80 14       	            br\.call\.sptk\.few\.clr b0=b2;;
    1240:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1246:	00 14 00 40 08 00 	      \(p02\) br\.call\.sptk\.many b0=b2
    124c:	28 00 80 10       	            br\.call\.sptk\.many b0=b2;;
    1250:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1256:	00 14 00 40 0a 00 	      \(p02\) br\.call\.sptk\.many\.clr b0=b2
    125c:	28 00 80 14       	            br\.call\.sptk\.many\.clr b0=b2;;
    1260:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1266:	00 10 00 c0 08 00 	      \(p02\) br\.call\.spnt\.few b0=b2
    126c:	20 00 80 11       	            br\.call\.spnt\.few b0=b2;;
    1270:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1276:	00 10 00 c0 0a 00 	      \(p02\) br\.call\.spnt\.few\.clr b0=b2
    127c:	20 00 80 15       	            br\.call\.spnt\.few\.clr b0=b2;;
    1280:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1286:	00 10 00 c0 08 00 	      \(p02\) br\.call\.spnt\.few b0=b2
    128c:	20 00 80 11       	            br\.call\.spnt\.few b0=b2;;
    1290:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1296:	00 10 00 c0 0a 00 	      \(p02\) br\.call\.spnt\.few\.clr b0=b2
    129c:	20 00 80 15       	            br\.call\.spnt\.few\.clr b0=b2;;
    12a0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12a6:	00 14 00 c0 08 00 	      \(p02\) br\.call\.spnt\.many b0=b2
    12ac:	28 00 80 11       	            br\.call\.spnt\.many b0=b2;;
    12b0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12b6:	00 14 00 c0 0a 00 	      \(p02\) br\.call\.spnt\.many\.clr b0=b2
    12bc:	28 00 80 15       	            br\.call\.spnt\.many\.clr b0=b2;;
    12c0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12c6:	00 10 00 40 09 00 	      \(p02\) br\.call\.dptk\.few b0=b2
    12cc:	20 00 80 12       	            br\.call\.dptk\.few b0=b2;;
    12d0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12d6:	00 10 00 40 0b 00 	      \(p02\) br\.call\.dptk\.few\.clr b0=b2
    12dc:	20 00 80 16       	            br\.call\.dptk\.few\.clr b0=b2;;
    12e0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12e6:	00 10 00 40 09 00 	      \(p02\) br\.call\.dptk\.few b0=b2
    12ec:	20 00 80 12       	            br\.call\.dptk\.few b0=b2;;
    12f0:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    12f6:	00 10 00 40 0b 00 	      \(p02\) br\.call\.dptk\.few\.clr b0=b2
    12fc:	20 00 80 16       	            br\.call\.dptk\.few\.clr b0=b2;;
    1300:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1306:	00 14 00 40 09 00 	      \(p02\) br\.call\.dptk\.many b0=b2
    130c:	28 00 80 12       	            br\.call\.dptk\.many b0=b2;;
    1310:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1316:	00 14 00 40 0b 00 	      \(p02\) br\.call\.dptk\.many\.clr b0=b2
    131c:	28 00 80 16       	            br\.call\.dptk\.many\.clr b0=b2;;
    1320:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1326:	00 10 00 c0 09 00 	      \(p02\) br\.call\.dpnt\.few b0=b2
    132c:	20 00 80 13       	            br\.call\.dpnt\.few b0=b2;;
    1330:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1336:	00 10 00 c0 0b 00 	      \(p02\) br\.call\.dpnt\.few\.clr b0=b2
    133c:	20 00 80 17       	            br\.call\.dpnt\.few\.clr b0=b2;;
    1340:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1346:	00 10 00 c0 09 00 	      \(p02\) br\.call\.dpnt\.few b0=b2
    134c:	20 00 80 13       	            br\.call\.dpnt\.few b0=b2;;
    1350:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1356:	00 10 00 c0 0b 00 	      \(p02\) br\.call\.dpnt\.few\.clr b0=b2
    135c:	20 00 80 17       	            br\.call\.dpnt\.few\.clr b0=b2;;
    1360:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1366:	00 14 00 c0 09 00 	      \(p02\) br\.call\.dpnt\.many b0=b2
    136c:	28 00 80 13       	            br\.call\.dpnt\.many b0=b2;;
    1370:	17 00 00 00 00 88 	\[BBB\]       nop\.b 0x0
    1376:	00 14 00 c0 0b 00 	      \(p02\) br\.call\.dpnt\.many\.clr b0=b2
    137c:	28 00 80 17       	            br\.call\.dpnt\.many\.clr b0=b2;;
    1380:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1386:	00 00 00 00 10 40 	            nop\.b 0x0
    138c:	80 ec ff 78       	            brp\.sptk 0x0,0x13a0;;
    1390:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1396:	00 00 00 00 10 20 	            nop\.b 0x0
    139c:	70 ec ff 7c       	            brp\.sptk\.imp 0x0,0x13a0;;
    13a0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13a6:	00 00 00 00 10 44 	            nop\.b 0x0
    13ac:	60 ec ff 78       	            brp\.loop 0x0,0x13c0;;
    13b0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13b6:	00 00 00 00 10 24 	            nop\.b 0x0
    13bc:	50 ec ff 7c       	            brp\.loop\.imp 0x0,0x13c0;;
    13c0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13c6:	00 00 00 00 10 48 	            nop\.b 0x0
    13cc:	40 ec ff 78       	            brp\.dptk 0x0,0x13e0;;
    13d0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13d6:	00 00 00 00 10 28 	            nop\.b 0x0
    13dc:	30 ec ff 7c       	            brp\.dptk\.imp 0x0,0x13e0;;
    13e0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13e6:	00 00 00 00 10 4c 	            nop\.b 0x0
    13ec:	20 ec ff 78       	            brp\.exit 0x0,0x1400;;
    13f0:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    13f6:	00 00 00 00 10 2c 	            nop\.b 0x0
    13fc:	10 ec ff 7c       	            brp\.exit\.imp 0x0,0x1400;;
    1400:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1406:	00 00 00 00 10 40 	            nop\.b 0x0
    140c:	30 00 40 20       	            brp\.sptk b3,0x1420;;
    1410:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1416:	00 00 00 00 10 20 	            nop\.b 0x0
    141c:	30 00 40 24       	            brp\.sptk\.imp b3,0x1420;;
    1420:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1426:	00 00 00 00 10 48 	            nop\.b 0x0
    142c:	30 00 40 20       	            brp\.dptk b3,0x1440;;
    1430:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1436:	00 00 00 00 10 28 	            nop\.b 0x0
    143c:	30 00 40 24       	            brp\.dptk.imp b3,0x1440;;
    1440:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1446:	00 00 00 00 10 40 	            nop\.b 0x0
    144c:	30 00 44 20       	            brp\.ret\.sptk b3,0x1460;;
    1450:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1456:	00 00 00 00 10 20 	            nop\.b 0x0
    145c:	30 00 44 24       	            brp\.ret\.sptk\.imp b3,0x1460;;
    1460:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1466:	00 00 00 00 10 48 	            nop\.b 0x0
    146c:	30 00 44 20       	            brp\.ret\.dptk b3,0x1480;;
    1470:	17 00 00 00 00 00 	\[BBB\]       break\.b 0x0
    1476:	00 00 00 00 10 28 	            nop\.b 0x0
    147c:	30 00 44 24       	            brp\.ret\.dptk.imp b3,0x1480;;
	\.\.\.
    2b80:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2b86:	00 00 00 00 10 00 	            nop\.b 0x0
    2b8c:	00 00 08 00       	            cover;;
    2b90:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2b96:	00 00 00 00 10 00 	            nop\.b 0x0
    2b9c:	00 00 10 00       	            clrrrb;;
    2ba0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2ba6:	00 00 00 00 10 00 	            nop\.b 0x0
    2bac:	00 00 14 00       	            clrrrb\.pr;;
    2bb0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2bb6:	00 00 00 00 10 00 	            nop\.b 0x0
    2bbc:	00 00 20 00       	            rfi;;
    2bc0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2bc6:	00 00 00 00 10 00 	            nop\.b 0x0
    2bcc:	00 00 30 00       	            bsw\.0;;
    2bd0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2bd6:	00 00 00 00 10 00 	            nop\.b 0x0
    2bdc:	00 00 34 00       	            bsw\.1;;
    2be0:	17 00 00 00 00 08 	\[BBB\]       nop\.b 0x0
    2be6:	00 00 00 00 10 00 	            nop\.b 0x0
    2bec:	00 00 40 00       	            epc;;
    2bf0:	16 f8 ff 0f 00 00 	\[BBB\]       break\.b 0x1ffff
    2bf6:	00 00 00 02 10 e0 	            hint\.b 0x0
    2bfc:	ff 3f 04 20       	            hint\.b 0x1ffff
    2c00:	17 f8 ff 0f 00 08 	\[BBB\]       nop\.b 0x1ffff
    2c06:	00 00 00 30 00 00 	            vmsw.0
    2c0c:	00 00 64 00       	            vmsw.1;;
