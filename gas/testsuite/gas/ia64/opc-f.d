# as: -xnone -mtune=itanium1
# objdump: -d --disassemble-zeroes
# name: ia64 opc-f

.*: +file format .*

Disassembly of section \.text:

0+000 <_start>:
       0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
       6:	40 38 14 0c 40 00 	            fma\.s0 f4=f5,f6,f7
       c:	00 00 00 20       	            nop\.b 0x0
      10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      16:	40 38 14 0c 40 00 	            fma\.s0 f4=f5,f6,f7
      1c:	00 00 00 20       	            nop\.b 0x0
      20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      26:	40 38 14 0c 41 00 	            fma\.s1 f4=f5,f6,f7
      2c:	00 00 00 20       	            nop\.b 0x0
      30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      36:	40 38 14 0c 42 00 	            fma\.s2 f4=f5,f6,f7
      3c:	00 00 00 20       	            nop\.b 0x0
      40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      46:	40 38 14 0c 43 00 	            fma\.s3 f4=f5,f6,f7
      4c:	00 00 00 20       	            nop\.b 0x0
      50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      56:	40 38 14 0c 44 00 	            fma\.s\.s0 f4=f5,f6,f7
      5c:	00 00 00 20       	            nop\.b 0x0
      60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      66:	40 38 14 0c 44 00 	            fma\.s\.s0 f4=f5,f6,f7
      6c:	00 00 00 20       	            nop\.b 0x0
      70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      76:	40 38 14 0c 45 00 	            fma\.s\.s1 f4=f5,f6,f7
      7c:	00 00 00 20       	            nop\.b 0x0
      80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      86:	40 38 14 0c 46 00 	            fma\.s\.s2 f4=f5,f6,f7
      8c:	00 00 00 20       	            nop\.b 0x0
      90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      96:	40 38 14 0c 47 00 	            fma\.s\.s3 f4=f5,f6,f7
      9c:	00 00 00 20       	            nop\.b 0x0
      a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      a6:	40 38 14 0c 48 00 	            fma\.d\.s0 f4=f5,f6,f7
      ac:	00 00 00 20       	            nop\.b 0x0
      b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      b6:	40 38 14 0c 48 00 	            fma\.d\.s0 f4=f5,f6,f7
      bc:	00 00 00 20       	            nop\.b 0x0
      c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      c6:	40 38 14 0c 49 00 	            fma\.d\.s1 f4=f5,f6,f7
      cc:	00 00 00 20       	            nop\.b 0x0
      d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      d6:	40 38 14 0c 4a 00 	            fma\.d\.s2 f4=f5,f6,f7
      dc:	00 00 00 20       	            nop\.b 0x0
      e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      e6:	40 38 14 0c 4b 00 	            fma\.d\.s3 f4=f5,f6,f7
      ec:	00 00 00 20       	            nop\.b 0x0
      f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
      f6:	40 38 14 0c 4c 00 	            fpma\.s0 f4=f5,f6,f7
      fc:	00 00 00 20       	            nop\.b 0x0
     100:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     106:	40 38 14 0c 4c 00 	            fpma\.s0 f4=f5,f6,f7
     10c:	00 00 00 20       	            nop\.b 0x0
     110:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     116:	40 38 14 0c 4d 00 	            fpma\.s1 f4=f5,f6,f7
     11c:	00 00 00 20       	            nop\.b 0x0
     120:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     126:	40 38 14 0c 4e 00 	            fpma\.s2 f4=f5,f6,f7
     12c:	00 00 00 20       	            nop\.b 0x0
     130:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     136:	40 38 14 0c 4f 00 	            fpma\.s3 f4=f5,f6,f7
     13c:	00 00 00 20       	            nop\.b 0x0
     140:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     146:	40 38 14 0c 50 00 	            fms\.s0 f4=f5,f6,f7
     14c:	00 00 00 20       	            nop\.b 0x0
     150:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     156:	40 38 14 0c 50 00 	            fms\.s0 f4=f5,f6,f7
     15c:	00 00 00 20       	            nop\.b 0x0
     160:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     166:	40 38 14 0c 51 00 	            fms\.s1 f4=f5,f6,f7
     16c:	00 00 00 20       	            nop\.b 0x0
     170:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     176:	40 38 14 0c 52 00 	            fms\.s2 f4=f5,f6,f7
     17c:	00 00 00 20       	            nop\.b 0x0
     180:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     186:	40 38 14 0c 53 00 	            fms\.s3 f4=f5,f6,f7
     18c:	00 00 00 20       	            nop\.b 0x0
     190:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     196:	40 38 14 0c 54 00 	            fms\.s\.s0 f4=f5,f6,f7
     19c:	00 00 00 20       	            nop\.b 0x0
     1a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1a6:	40 38 14 0c 54 00 	            fms\.s\.s0 f4=f5,f6,f7
     1ac:	00 00 00 20       	            nop\.b 0x0
     1b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1b6:	40 38 14 0c 55 00 	            fms\.s\.s1 f4=f5,f6,f7
     1bc:	00 00 00 20       	            nop\.b 0x0
     1c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1c6:	40 38 14 0c 56 00 	            fms\.s\.s2 f4=f5,f6,f7
     1cc:	00 00 00 20       	            nop\.b 0x0
     1d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1d6:	40 38 14 0c 57 00 	            fms\.s\.s3 f4=f5,f6,f7
     1dc:	00 00 00 20       	            nop\.b 0x0
     1e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1e6:	40 38 14 0c 58 00 	            fms\.d\.s0 f4=f5,f6,f7
     1ec:	00 00 00 20       	            nop\.b 0x0
     1f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     1f6:	40 38 14 0c 58 00 	            fms\.d\.s0 f4=f5,f6,f7
     1fc:	00 00 00 20       	            nop\.b 0x0
     200:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     206:	40 38 14 0c 59 00 	            fms\.d\.s1 f4=f5,f6,f7
     20c:	00 00 00 20       	            nop\.b 0x0
     210:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     216:	40 38 14 0c 5a 00 	            fms\.d\.s2 f4=f5,f6,f7
     21c:	00 00 00 20       	            nop\.b 0x0
     220:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     226:	40 38 14 0c 5b 00 	            fms\.d\.s3 f4=f5,f6,f7
     22c:	00 00 00 20       	            nop\.b 0x0
     230:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     236:	40 38 14 0c 5c 00 	            fpms\.s0 f4=f5,f6,f7
     23c:	00 00 00 20       	            nop\.b 0x0
     240:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     246:	40 38 14 0c 5c 00 	            fpms\.s0 f4=f5,f6,f7
     24c:	00 00 00 20       	            nop\.b 0x0
     250:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     256:	40 38 14 0c 5d 00 	            fpms\.s1 f4=f5,f6,f7
     25c:	00 00 00 20       	            nop\.b 0x0
     260:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     266:	40 38 14 0c 5e 00 	            fpms\.s2 f4=f5,f6,f7
     26c:	00 00 00 20       	            nop\.b 0x0
     270:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     276:	40 38 14 0c 5f 00 	            fpms\.s3 f4=f5,f6,f7
     27c:	00 00 00 20       	            nop\.b 0x0
     280:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     286:	40 38 14 0c 60 00 	            fnma\.s0 f4=f5,f6,f7
     28c:	00 00 00 20       	            nop\.b 0x0
     290:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     296:	40 38 14 0c 60 00 	            fnma\.s0 f4=f5,f6,f7
     29c:	00 00 00 20       	            nop\.b 0x0
     2a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2a6:	40 38 14 0c 61 00 	            fnma\.s1 f4=f5,f6,f7
     2ac:	00 00 00 20       	            nop\.b 0x0
     2b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2b6:	40 38 14 0c 62 00 	            fnma\.s2 f4=f5,f6,f7
     2bc:	00 00 00 20       	            nop\.b 0x0
     2c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2c6:	40 38 14 0c 63 00 	            fnma\.s3 f4=f5,f6,f7
     2cc:	00 00 00 20       	            nop\.b 0x0
     2d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2d6:	40 38 14 0c 64 00 	            fnma\.s\.s0 f4=f5,f6,f7
     2dc:	00 00 00 20       	            nop\.b 0x0
     2e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2e6:	40 38 14 0c 64 00 	            fnma\.s\.s0 f4=f5,f6,f7
     2ec:	00 00 00 20       	            nop\.b 0x0
     2f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     2f6:	40 38 14 0c 65 00 	            fnma\.s\.s1 f4=f5,f6,f7
     2fc:	00 00 00 20       	            nop\.b 0x0
     300:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     306:	40 38 14 0c 66 00 	            fnma\.s\.s2 f4=f5,f6,f7
     30c:	00 00 00 20       	            nop\.b 0x0
     310:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     316:	40 38 14 0c 67 00 	            fnma\.s\.s3 f4=f5,f6,f7
     31c:	00 00 00 20       	            nop\.b 0x0
     320:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     326:	40 38 14 0c 68 00 	            fnma\.d\.s0 f4=f5,f6,f7
     32c:	00 00 00 20       	            nop\.b 0x0
     330:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     336:	40 38 14 0c 68 00 	            fnma\.d\.s0 f4=f5,f6,f7
     33c:	00 00 00 20       	            nop\.b 0x0
     340:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     346:	40 38 14 0c 69 00 	            fnma\.d\.s1 f4=f5,f6,f7
     34c:	00 00 00 20       	            nop\.b 0x0
     350:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     356:	40 38 14 0c 6a 00 	            fnma\.d\.s2 f4=f5,f6,f7
     35c:	00 00 00 20       	            nop\.b 0x0
     360:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     366:	40 38 14 0c 6b 00 	            fnma\.d\.s3 f4=f5,f6,f7
     36c:	00 00 00 20       	            nop\.b 0x0
     370:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     376:	40 38 14 0c 6c 00 	            fpnma\.s0 f4=f5,f6,f7
     37c:	00 00 00 20       	            nop\.b 0x0
     380:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     386:	40 38 14 0c 6c 00 	            fpnma\.s0 f4=f5,f6,f7
     38c:	00 00 00 20       	            nop\.b 0x0
     390:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     396:	40 38 14 0c 6d 00 	            fpnma\.s1 f4=f5,f6,f7
     39c:	00 00 00 20       	            nop\.b 0x0
     3a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3a6:	40 38 14 0c 6e 00 	            fpnma\.s2 f4=f5,f6,f7
     3ac:	00 00 00 20       	            nop\.b 0x0
     3b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3b6:	40 38 14 0c 6f 00 	            fpnma\.s3 f4=f5,f6,f7
     3bc:	00 00 00 20       	            nop\.b 0x0
     3c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3c6:	40 00 14 0c 40 00 	            fmpy\.s0 f4=f5,f6
     3cc:	00 00 00 20       	            nop\.b 0x0
     3d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3d6:	40 00 14 0c 40 00 	            fmpy\.s0 f4=f5,f6
     3dc:	00 00 00 20       	            nop\.b 0x0
     3e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3e6:	40 00 14 0c 41 00 	            fmpy\.s1 f4=f5,f6
     3ec:	00 00 00 20       	            nop\.b 0x0
     3f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     3f6:	40 00 14 0c 42 00 	            fmpy\.s2 f4=f5,f6
     3fc:	00 00 00 20       	            nop\.b 0x0
     400:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     406:	40 00 14 0c 43 00 	            fmpy\.s3 f4=f5,f6
     40c:	00 00 00 20       	            nop\.b 0x0
     410:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     416:	40 00 14 0c 44 00 	            fmpy\.s\.s0 f4=f5,f6
     41c:	00 00 00 20       	            nop\.b 0x0
     420:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     426:	40 00 14 0c 44 00 	            fmpy\.s\.s0 f4=f5,f6
     42c:	00 00 00 20       	            nop\.b 0x0
     430:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     436:	40 00 14 0c 45 00 	            fmpy\.s\.s1 f4=f5,f6
     43c:	00 00 00 20       	            nop\.b 0x0
     440:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     446:	40 00 14 0c 46 00 	            fmpy\.s\.s2 f4=f5,f6
     44c:	00 00 00 20       	            nop\.b 0x0
     450:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     456:	40 00 14 0c 47 00 	            fmpy\.s\.s3 f4=f5,f6
     45c:	00 00 00 20       	            nop\.b 0x0
     460:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     466:	40 00 14 0c 48 00 	            fmpy\.d\.s0 f4=f5,f6
     46c:	00 00 00 20       	            nop\.b 0x0
     470:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     476:	40 00 14 0c 48 00 	            fmpy\.d\.s0 f4=f5,f6
     47c:	00 00 00 20       	            nop\.b 0x0
     480:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     486:	40 00 14 0c 49 00 	            fmpy\.d\.s1 f4=f5,f6
     48c:	00 00 00 20       	            nop\.b 0x0
     490:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     496:	40 00 14 0c 4a 00 	            fmpy\.d\.s2 f4=f5,f6
     49c:	00 00 00 20       	            nop\.b 0x0
     4a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4a6:	40 00 14 0c 4b 00 	            fmpy\.d\.s3 f4=f5,f6
     4ac:	00 00 00 20       	            nop\.b 0x0
     4b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4b6:	40 00 14 0c 4c 00 	            fpmpy\.s0 f4=f5,f6
     4bc:	00 00 00 20       	            nop\.b 0x0
     4c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4c6:	40 00 14 0c 4c 00 	            fpmpy\.s0 f4=f5,f6
     4cc:	00 00 00 20       	            nop\.b 0x0
     4d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4d6:	40 00 14 0c 4d 00 	            fpmpy\.s1 f4=f5,f6
     4dc:	00 00 00 20       	            nop\.b 0x0
     4e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4e6:	40 00 14 0c 4e 00 	            fpmpy\.s2 f4=f5,f6
     4ec:	00 00 00 20       	            nop\.b 0x0
     4f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     4f6:	40 00 14 0c 4f 00 	            fpmpy\.s3 f4=f5,f6
     4fc:	00 00 00 20       	            nop\.b 0x0
     500:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     506:	40 30 14 02 40 00 	            fadd\.s0 f4=f5,f6
     50c:	00 00 00 20       	            nop\.b 0x0
     510:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     516:	40 30 14 02 40 00 	            fadd\.s0 f4=f5,f6
     51c:	00 00 00 20       	            nop\.b 0x0
     520:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     526:	40 30 14 02 41 00 	            fadd\.s1 f4=f5,f6
     52c:	00 00 00 20       	            nop\.b 0x0
     530:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     536:	40 30 14 02 42 00 	            fadd\.s2 f4=f5,f6
     53c:	00 00 00 20       	            nop\.b 0x0
     540:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     546:	40 30 14 02 43 00 	            fadd\.s3 f4=f5,f6
     54c:	00 00 00 20       	            nop\.b 0x0
     550:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     556:	40 30 14 02 44 00 	            fadd\.s\.s0 f4=f5,f6
     55c:	00 00 00 20       	            nop\.b 0x0
     560:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     566:	40 30 14 02 44 00 	            fadd\.s\.s0 f4=f5,f6
     56c:	00 00 00 20       	            nop\.b 0x0
     570:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     576:	40 30 14 02 45 00 	            fadd\.s\.s1 f4=f5,f6
     57c:	00 00 00 20       	            nop\.b 0x0
     580:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     586:	40 30 14 02 46 00 	            fadd\.s\.s2 f4=f5,f6
     58c:	00 00 00 20       	            nop\.b 0x0
     590:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     596:	40 30 14 02 47 00 	            fadd\.s\.s3 f4=f5,f6
     59c:	00 00 00 20       	            nop\.b 0x0
     5a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5a6:	40 30 14 02 48 00 	            fadd\.d\.s0 f4=f5,f6
     5ac:	00 00 00 20       	            nop\.b 0x0
     5b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5b6:	40 30 14 02 48 00 	            fadd\.d\.s0 f4=f5,f6
     5bc:	00 00 00 20       	            nop\.b 0x0
     5c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5c6:	40 30 14 02 49 00 	            fadd\.d\.s1 f4=f5,f6
     5cc:	00 00 00 20       	            nop\.b 0x0
     5d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5d6:	40 30 14 02 4a 00 	            fadd\.d\.s2 f4=f5,f6
     5dc:	00 00 00 20       	            nop\.b 0x0
     5e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5e6:	40 30 14 02 4b 00 	            fadd\.d\.s3 f4=f5,f6
     5ec:	00 00 00 20       	            nop\.b 0x0
     5f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     5f6:	40 30 14 02 50 00 	            fsub\.s0 f4=f5,f6
     5fc:	00 00 00 20       	            nop\.b 0x0
     600:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     606:	40 30 14 02 50 00 	            fsub\.s0 f4=f5,f6
     60c:	00 00 00 20       	            nop\.b 0x0
     610:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     616:	40 30 14 02 51 00 	            fsub\.s1 f4=f5,f6
     61c:	00 00 00 20       	            nop\.b 0x0
     620:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     626:	40 30 14 02 52 00 	            fsub\.s2 f4=f5,f6
     62c:	00 00 00 20       	            nop\.b 0x0
     630:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     636:	40 30 14 02 53 00 	            fsub\.s3 f4=f5,f6
     63c:	00 00 00 20       	            nop\.b 0x0
     640:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     646:	40 30 14 02 54 00 	            fsub\.s\.s0 f4=f5,f6
     64c:	00 00 00 20       	            nop\.b 0x0
     650:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     656:	40 30 14 02 54 00 	            fsub\.s\.s0 f4=f5,f6
     65c:	00 00 00 20       	            nop\.b 0x0
     660:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     666:	40 30 14 02 55 00 	            fsub\.s\.s1 f4=f5,f6
     66c:	00 00 00 20       	            nop\.b 0x0
     670:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     676:	40 30 14 02 56 00 	            fsub\.s\.s2 f4=f5,f6
     67c:	00 00 00 20       	            nop\.b 0x0
     680:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     686:	40 30 14 02 57 00 	            fsub\.s\.s3 f4=f5,f6
     68c:	00 00 00 20       	            nop\.b 0x0
     690:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     696:	40 30 14 02 58 00 	            fsub\.d\.s0 f4=f5,f6
     69c:	00 00 00 20       	            nop\.b 0x0
     6a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6a6:	40 30 14 02 58 00 	            fsub\.d\.s0 f4=f5,f6
     6ac:	00 00 00 20       	            nop\.b 0x0
     6b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6b6:	40 30 14 02 59 00 	            fsub\.d\.s1 f4=f5,f6
     6bc:	00 00 00 20       	            nop\.b 0x0
     6c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6c6:	40 30 14 02 5a 00 	            fsub\.d\.s2 f4=f5,f6
     6cc:	00 00 00 20       	            nop\.b 0x0
     6d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6d6:	40 30 14 02 5b 00 	            fsub\.d\.s3 f4=f5,f6
     6dc:	00 00 00 20       	            nop\.b 0x0
     6e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6e6:	40 00 14 0c 60 00 	            fnmpy\.s0 f4=f5,f6
     6ec:	00 00 00 20       	            nop\.b 0x0
     6f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     6f6:	40 00 14 0c 60 00 	            fnmpy\.s0 f4=f5,f6
     6fc:	00 00 00 20       	            nop\.b 0x0
     700:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     706:	40 00 14 0c 61 00 	            fnmpy\.s1 f4=f5,f6
     70c:	00 00 00 20       	            nop\.b 0x0
     710:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     716:	40 00 14 0c 62 00 	            fnmpy\.s2 f4=f5,f6
     71c:	00 00 00 20       	            nop\.b 0x0
     720:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     726:	40 00 14 0c 63 00 	            fnmpy\.s3 f4=f5,f6
     72c:	00 00 00 20       	            nop\.b 0x0
     730:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     736:	40 00 14 0c 64 00 	            fnmpy\.s\.s0 f4=f5,f6
     73c:	00 00 00 20       	            nop\.b 0x0
     740:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     746:	40 00 14 0c 64 00 	            fnmpy\.s\.s0 f4=f5,f6
     74c:	00 00 00 20       	            nop\.b 0x0
     750:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     756:	40 00 14 0c 65 00 	            fnmpy\.s\.s1 f4=f5,f6
     75c:	00 00 00 20       	            nop\.b 0x0
     760:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     766:	40 00 14 0c 66 00 	            fnmpy\.s\.s2 f4=f5,f6
     76c:	00 00 00 20       	            nop\.b 0x0
     770:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     776:	40 00 14 0c 67 00 	            fnmpy\.s\.s3 f4=f5,f6
     77c:	00 00 00 20       	            nop\.b 0x0
     780:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     786:	40 00 14 0c 68 00 	            fnmpy\.d\.s0 f4=f5,f6
     78c:	00 00 00 20       	            nop\.b 0x0
     790:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     796:	40 00 14 0c 68 00 	            fnmpy\.d\.s0 f4=f5,f6
     79c:	00 00 00 20       	            nop\.b 0x0
     7a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7a6:	40 00 14 0c 69 00 	            fnmpy\.d\.s1 f4=f5,f6
     7ac:	00 00 00 20       	            nop\.b 0x0
     7b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7b6:	40 00 14 0c 6a 00 	            fnmpy\.d\.s2 f4=f5,f6
     7bc:	00 00 00 20       	            nop\.b 0x0
     7c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7c6:	40 00 14 0c 6b 00 	            fnmpy\.d\.s3 f4=f5,f6
     7cc:	00 00 00 20       	            nop\.b 0x0
     7d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7d6:	40 00 14 0c 6c 00 	            fpnmpy\.s0 f4=f5,f6
     7dc:	00 00 00 20       	            nop\.b 0x0
     7e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7e6:	40 00 14 0c 6c 00 	            fpnmpy\.s0 f4=f5,f6
     7ec:	00 00 00 20       	            nop\.b 0x0
     7f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     7f6:	40 00 14 0c 6d 00 	            fpnmpy\.s1 f4=f5,f6
     7fc:	00 00 00 20       	            nop\.b 0x0
     800:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     806:	40 00 14 0c 6e 00 	            fpnmpy\.s2 f4=f5,f6
     80c:	00 00 00 20       	            nop\.b 0x0
     810:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     816:	40 00 14 0c 6f 00 	            fpnmpy\.s3 f4=f5,f6
     81c:	00 00 00 20       	            nop\.b 0x0
     820:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     826:	40 00 14 02 40 00 	            fnorm\.s0 f4=f5
     82c:	00 00 00 20       	            nop\.b 0x0
     830:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     836:	40 00 14 02 40 00 	            fnorm\.s0 f4=f5
     83c:	00 00 00 20       	            nop\.b 0x0
     840:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     846:	40 00 14 02 41 00 	            fnorm\.s1 f4=f5
     84c:	00 00 00 20       	            nop\.b 0x0
     850:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     856:	40 00 14 02 42 00 	            fnorm\.s2 f4=f5
     85c:	00 00 00 20       	            nop\.b 0x0
     860:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     866:	40 00 14 02 43 00 	            fnorm\.s3 f4=f5
     86c:	00 00 00 20       	            nop\.b 0x0
     870:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     876:	40 00 14 02 44 00 	            fnorm\.s\.s0 f4=f5
     87c:	00 00 00 20       	            nop\.b 0x0
     880:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     886:	40 00 14 02 44 00 	            fnorm\.s\.s0 f4=f5
     88c:	00 00 00 20       	            nop\.b 0x0
     890:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     896:	40 00 14 02 45 00 	            fnorm\.s\.s1 f4=f5
     89c:	00 00 00 20       	            nop\.b 0x0
     8a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8a6:	40 00 14 02 46 00 	            fnorm\.s\.s2 f4=f5
     8ac:	00 00 00 20       	            nop\.b 0x0
     8b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8b6:	40 00 14 02 47 00 	            fnorm\.s\.s3 f4=f5
     8bc:	00 00 00 20       	            nop\.b 0x0
     8c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8c6:	40 00 14 02 48 00 	            fnorm\.d\.s0 f4=f5
     8cc:	00 00 00 20       	            nop\.b 0x0
     8d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8d6:	40 00 14 02 48 00 	            fnorm\.d\.s0 f4=f5
     8dc:	00 00 00 20       	            nop\.b 0x0
     8e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8e6:	40 00 14 02 49 00 	            fnorm\.d\.s1 f4=f5
     8ec:	00 00 00 20       	            nop\.b 0x0
     8f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     8f6:	40 00 14 02 4a 00 	            fnorm\.d\.s2 f4=f5
     8fc:	00 00 00 20       	            nop\.b 0x0
     900:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     906:	40 00 14 02 4b 00 	            fnorm\.d\.s3 f4=f5
     90c:	00 00 00 20       	            nop\.b 0x0
     910:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     916:	40 38 14 0c 74 00 	            xma\.l f4=f5,f6,f7
     91c:	00 00 00 20       	            nop\.b 0x0
     920:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     926:	40 38 14 0c 74 00 	            xma\.l f4=f5,f6,f7
     92c:	00 00 00 20       	            nop\.b 0x0
     930:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     936:	40 38 14 0c 77 00 	            xma\.h f4=f5,f6,f7
     93c:	00 00 00 20       	            nop\.b 0x0
     940:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     946:	40 38 14 0c 76 00 	            xma\.hu f4=f5,f6,f7
     94c:	00 00 00 20       	            nop\.b 0x0
     950:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     956:	40 00 14 0c 74 00 	            xmpy\.l f4=f5,f6
     95c:	00 00 00 20       	            nop\.b 0x0
     960:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     966:	40 00 14 0c 74 00 	            xmpy\.l f4=f5,f6
     96c:	00 00 00 20       	            nop\.b 0x0
     970:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     976:	40 00 14 0c 77 00 	            xmpy\.h f4=f5,f6
     97c:	00 00 00 20       	            nop\.b 0x0
     980:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     986:	40 00 14 0c 76 00 	            xmpy\.hu f4=f5,f6
     98c:	00 00 00 20       	            nop\.b 0x0
     990:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     996:	40 38 14 0c 70 00 	            fselect f4=f5,f6,f7
     99c:	00 00 00 20       	            nop\.b 0x0
     9a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9a6:	30 20 14 08 20 00 	            fcmp\.eq\.s0 p3,p4=f4,f5
     9ac:	00 00 00 20       	            nop\.b 0x0
     9b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9b6:	30 20 14 08 20 00 	            fcmp\.eq\.s0 p3,p4=f4,f5
     9bc:	00 00 00 20       	            nop\.b 0x0
     9c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9c6:	30 20 14 08 21 00 	            fcmp\.eq\.s1 p3,p4=f4,f5
     9cc:	00 00 00 20       	            nop\.b 0x0
     9d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9d6:	30 20 14 08 22 00 	            fcmp\.eq\.s2 p3,p4=f4,f5
     9dc:	00 00 00 20       	            nop\.b 0x0
     9e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9e6:	30 20 14 08 23 00 	            fcmp\.eq\.s3 p3,p4=f4,f5
     9ec:	00 00 00 20       	            nop\.b 0x0
     9f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     9f6:	30 24 14 08 20 00 	            fcmp\.eq\.unc\.s0 p3,p4=f4,f5
     9fc:	00 00 00 20       	            nop\.b 0x0
     a00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a06:	30 24 14 08 20 00 	            fcmp\.eq\.unc\.s0 p3,p4=f4,f5
     a0c:	00 00 00 20       	            nop\.b 0x0
     a10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a16:	30 24 14 08 21 00 	            fcmp\.eq\.unc\.s1 p3,p4=f4,f5
     a1c:	00 00 00 20       	            nop\.b 0x0
     a20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a26:	30 24 14 08 22 00 	            fcmp\.eq\.unc\.s2 p3,p4=f4,f5
     a2c:	00 00 00 20       	            nop\.b 0x0
     a30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a36:	30 24 14 08 23 00 	            fcmp\.eq\.unc\.s3 p3,p4=f4,f5
     a3c:	00 00 00 20       	            nop\.b 0x0
     a40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a46:	30 20 14 08 24 00 	            fcmp\.lt\.s0 p3,p4=f4,f5
     a4c:	00 00 00 20       	            nop\.b 0x0
     a50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a56:	30 20 14 08 24 00 	            fcmp\.lt\.s0 p3,p4=f4,f5
     a5c:	00 00 00 20       	            nop\.b 0x0
     a60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a66:	30 20 14 08 25 00 	            fcmp\.lt\.s1 p3,p4=f4,f5
     a6c:	00 00 00 20       	            nop\.b 0x0
     a70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a76:	30 20 14 08 26 00 	            fcmp\.lt\.s2 p3,p4=f4,f5
     a7c:	00 00 00 20       	            nop\.b 0x0
     a80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a86:	30 20 14 08 27 00 	            fcmp\.lt\.s3 p3,p4=f4,f5
     a8c:	00 00 00 20       	            nop\.b 0x0
     a90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     a96:	30 24 14 08 24 00 	            fcmp\.lt\.unc\.s0 p3,p4=f4,f5
     a9c:	00 00 00 20       	            nop\.b 0x0
     aa0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     aa6:	30 24 14 08 24 00 	            fcmp\.lt\.unc\.s0 p3,p4=f4,f5
     aac:	00 00 00 20       	            nop\.b 0x0
     ab0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ab6:	30 24 14 08 25 00 	            fcmp\.lt\.unc\.s1 p3,p4=f4,f5
     abc:	00 00 00 20       	            nop\.b 0x0
     ac0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ac6:	30 24 14 08 26 00 	            fcmp\.lt\.unc\.s2 p3,p4=f4,f5
     acc:	00 00 00 20       	            nop\.b 0x0
     ad0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ad6:	30 24 14 08 27 00 	            fcmp\.lt\.unc\.s3 p3,p4=f4,f5
     adc:	00 00 00 20       	            nop\.b 0x0
     ae0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ae6:	30 20 14 88 20 00 	            fcmp\.le\.s0 p3,p4=f4,f5
     aec:	00 00 00 20       	            nop\.b 0x0
     af0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     af6:	30 20 14 88 20 00 	            fcmp\.le\.s0 p3,p4=f4,f5
     afc:	00 00 00 20       	            nop\.b 0x0
     b00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b06:	30 20 14 88 21 00 	            fcmp\.le\.s1 p3,p4=f4,f5
     b0c:	00 00 00 20       	            nop\.b 0x0
     b10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b16:	30 20 14 88 22 00 	            fcmp\.le\.s2 p3,p4=f4,f5
     b1c:	00 00 00 20       	            nop\.b 0x0
     b20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b26:	30 20 14 88 23 00 	            fcmp\.le\.s3 p3,p4=f4,f5
     b2c:	00 00 00 20       	            nop\.b 0x0
     b30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b36:	30 24 14 88 20 00 	            fcmp\.le\.unc\.s0 p3,p4=f4,f5
     b3c:	00 00 00 20       	            nop\.b 0x0
     b40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b46:	30 24 14 88 20 00 	            fcmp\.le\.unc\.s0 p3,p4=f4,f5
     b4c:	00 00 00 20       	            nop\.b 0x0
     b50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b56:	30 24 14 88 21 00 	            fcmp\.le\.unc\.s1 p3,p4=f4,f5
     b5c:	00 00 00 20       	            nop\.b 0x0
     b60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b66:	30 24 14 88 22 00 	            fcmp\.le\.unc\.s2 p3,p4=f4,f5
     b6c:	00 00 00 20       	            nop\.b 0x0
     b70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b76:	30 24 14 88 23 00 	            fcmp\.le\.unc\.s3 p3,p4=f4,f5
     b7c:	00 00 00 20       	            nop\.b 0x0
     b80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b86:	30 20 14 88 24 00 	            fcmp\.unord\.s0 p3,p4=f4,f5
     b8c:	00 00 00 20       	            nop\.b 0x0
     b90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     b96:	30 20 14 88 24 00 	            fcmp\.unord\.s0 p3,p4=f4,f5
     b9c:	00 00 00 20       	            nop\.b 0x0
     ba0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ba6:	30 20 14 88 25 00 	            fcmp\.unord\.s1 p3,p4=f4,f5
     bac:	00 00 00 20       	            nop\.b 0x0
     bb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     bb6:	30 20 14 88 26 00 	            fcmp\.unord\.s2 p3,p4=f4,f5
     bbc:	00 00 00 20       	            nop\.b 0x0
     bc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     bc6:	30 20 14 88 27 00 	            fcmp\.unord\.s3 p3,p4=f4,f5
     bcc:	00 00 00 20       	            nop\.b 0x0
     bd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     bd6:	30 24 14 88 24 00 	            fcmp\.unord\.unc\.s0 p3,p4=f4,f5
     bdc:	00 00 00 20       	            nop\.b 0x0
     be0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     be6:	30 24 14 88 24 00 	            fcmp\.unord\.unc\.s0 p3,p4=f4,f5
     bec:	00 00 00 20       	            nop\.b 0x0
     bf0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     bf6:	30 24 14 88 25 00 	            fcmp\.unord\.unc\.s1 p3,p4=f4,f5
     bfc:	00 00 00 20       	            nop\.b 0x0
     c00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c06:	30 24 14 88 26 00 	            fcmp\.unord\.unc\.s2 p3,p4=f4,f5
     c0c:	00 00 00 20       	            nop\.b 0x0
     c10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c16:	30 24 14 88 27 00 	            fcmp\.unord\.unc\.s3 p3,p4=f4,f5
     c1c:	00 00 00 20       	            nop\.b 0x0
     c20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c26:	30 28 10 08 24 00 	            fcmp\.lt\.s0 p3,p4=f5,f4
     c2c:	00 00 00 20       	            nop\.b 0x0
     c30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c36:	30 28 10 08 24 00 	            fcmp\.lt\.s0 p3,p4=f5,f4
     c3c:	00 00 00 20       	            nop\.b 0x0
     c40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c46:	30 28 10 08 25 00 	            fcmp\.lt\.s1 p3,p4=f5,f4
     c4c:	00 00 00 20       	            nop\.b 0x0
     c50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c56:	30 28 10 08 26 00 	            fcmp\.lt\.s2 p3,p4=f5,f4
     c5c:	00 00 00 20       	            nop\.b 0x0
     c60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c66:	30 28 10 08 27 00 	            fcmp\.lt\.s3 p3,p4=f5,f4
     c6c:	00 00 00 20       	            nop\.b 0x0
     c70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c76:	30 2c 10 08 24 00 	            fcmp\.lt\.unc\.s0 p3,p4=f5,f4
     c7c:	00 00 00 20       	            nop\.b 0x0
     c80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c86:	30 2c 10 08 24 00 	            fcmp\.lt\.unc\.s0 p3,p4=f5,f4
     c8c:	00 00 00 20       	            nop\.b 0x0
     c90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     c96:	30 2c 10 08 25 00 	            fcmp\.lt\.unc\.s1 p3,p4=f5,f4
     c9c:	00 00 00 20       	            nop\.b 0x0
     ca0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ca6:	30 2c 10 08 26 00 	            fcmp\.lt\.unc\.s2 p3,p4=f5,f4
     cac:	00 00 00 20       	            nop\.b 0x0
     cb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     cb6:	30 2c 10 08 27 00 	            fcmp\.lt\.unc\.s3 p3,p4=f5,f4
     cbc:	00 00 00 20       	            nop\.b 0x0
     cc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     cc6:	30 28 10 88 20 00 	            fcmp\.le\.s0 p3,p4=f5,f4
     ccc:	00 00 00 20       	            nop\.b 0x0
     cd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     cd6:	30 28 10 88 20 00 	            fcmp\.le\.s0 p3,p4=f5,f4
     cdc:	00 00 00 20       	            nop\.b 0x0
     ce0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ce6:	30 28 10 88 21 00 	            fcmp\.le\.s1 p3,p4=f5,f4
     cec:	00 00 00 20       	            nop\.b 0x0
     cf0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     cf6:	30 28 10 88 22 00 	            fcmp\.le\.s2 p3,p4=f5,f4
     cfc:	00 00 00 20       	            nop\.b 0x0
     d00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d06:	30 28 10 88 23 00 	            fcmp\.le\.s3 p3,p4=f5,f4
     d0c:	00 00 00 20       	            nop\.b 0x0
     d10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d16:	30 2c 10 88 20 00 	            fcmp\.le\.unc\.s0 p3,p4=f5,f4
     d1c:	00 00 00 20       	            nop\.b 0x0
     d20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d26:	30 2c 10 88 20 00 	            fcmp\.le\.unc\.s0 p3,p4=f5,f4
     d2c:	00 00 00 20       	            nop\.b 0x0
     d30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d36:	30 2c 10 88 21 00 	            fcmp\.le\.unc\.s1 p3,p4=f5,f4
     d3c:	00 00 00 20       	            nop\.b 0x0
     d40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d46:	30 2c 10 88 22 00 	            fcmp\.le\.unc\.s2 p3,p4=f5,f4
     d4c:	00 00 00 20       	            nop\.b 0x0
     d50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d56:	30 2c 10 88 23 00 	            fcmp\.le\.unc\.s3 p3,p4=f5,f4
     d5c:	00 00 00 20       	            nop\.b 0x0
     d60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d66:	40 20 14 06 20 00 	            fcmp\.eq\.s0 p4,p3=f4,f5
     d6c:	00 00 00 20       	            nop\.b 0x0
     d70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d76:	40 20 14 06 20 00 	            fcmp\.eq\.s0 p4,p3=f4,f5
     d7c:	00 00 00 20       	            nop\.b 0x0
     d80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d86:	40 20 14 06 21 00 	            fcmp\.eq\.s1 p4,p3=f4,f5
     d8c:	00 00 00 20       	            nop\.b 0x0
     d90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     d96:	40 20 14 06 22 00 	            fcmp\.eq\.s2 p4,p3=f4,f5
     d9c:	00 00 00 20       	            nop\.b 0x0
     da0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     da6:	40 20 14 06 23 00 	            fcmp\.eq\.s3 p4,p3=f4,f5
     dac:	00 00 00 20       	            nop\.b 0x0
     db0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     db6:	40 24 14 06 20 00 	            fcmp\.eq\.unc\.s0 p4,p3=f4,f5
     dbc:	00 00 00 20       	            nop\.b 0x0
     dc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     dc6:	40 24 14 06 20 00 	            fcmp\.eq\.unc\.s0 p4,p3=f4,f5
     dcc:	00 00 00 20       	            nop\.b 0x0
     dd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     dd6:	40 24 14 06 21 00 	            fcmp\.eq\.unc\.s1 p4,p3=f4,f5
     ddc:	00 00 00 20       	            nop\.b 0x0
     de0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     de6:	40 24 14 06 22 00 	            fcmp\.eq\.unc\.s2 p4,p3=f4,f5
     dec:	00 00 00 20       	            nop\.b 0x0
     df0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     df6:	40 24 14 06 23 00 	            fcmp\.eq\.unc\.s3 p4,p3=f4,f5
     dfc:	00 00 00 20       	            nop\.b 0x0
     e00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e06:	40 20 14 06 24 00 	            fcmp\.lt\.s0 p4,p3=f4,f5
     e0c:	00 00 00 20       	            nop\.b 0x0
     e10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e16:	40 20 14 06 24 00 	            fcmp\.lt\.s0 p4,p3=f4,f5
     e1c:	00 00 00 20       	            nop\.b 0x0
     e20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e26:	40 20 14 06 25 00 	            fcmp\.lt\.s1 p4,p3=f4,f5
     e2c:	00 00 00 20       	            nop\.b 0x0
     e30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e36:	40 20 14 06 26 00 	            fcmp\.lt\.s2 p4,p3=f4,f5
     e3c:	00 00 00 20       	            nop\.b 0x0
     e40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e46:	40 20 14 06 27 00 	            fcmp\.lt\.s3 p4,p3=f4,f5
     e4c:	00 00 00 20       	            nop\.b 0x0
     e50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e56:	40 24 14 06 24 00 	            fcmp\.lt\.unc\.s0 p4,p3=f4,f5
     e5c:	00 00 00 20       	            nop\.b 0x0
     e60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e66:	40 24 14 06 24 00 	            fcmp\.lt\.unc\.s0 p4,p3=f4,f5
     e6c:	00 00 00 20       	            nop\.b 0x0
     e70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e76:	40 24 14 06 25 00 	            fcmp\.lt\.unc\.s1 p4,p3=f4,f5
     e7c:	00 00 00 20       	            nop\.b 0x0
     e80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e86:	40 24 14 06 26 00 	            fcmp\.lt\.unc\.s2 p4,p3=f4,f5
     e8c:	00 00 00 20       	            nop\.b 0x0
     e90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     e96:	40 24 14 06 27 00 	            fcmp\.lt\.unc\.s3 p4,p3=f4,f5
     e9c:	00 00 00 20       	            nop\.b 0x0
     ea0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ea6:	40 20 14 86 20 00 	            fcmp\.le\.s0 p4,p3=f4,f5
     eac:	00 00 00 20       	            nop\.b 0x0
     eb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     eb6:	40 20 14 86 20 00 	            fcmp\.le\.s0 p4,p3=f4,f5
     ebc:	00 00 00 20       	            nop\.b 0x0
     ec0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ec6:	40 20 14 86 21 00 	            fcmp\.le\.s1 p4,p3=f4,f5
     ecc:	00 00 00 20       	            nop\.b 0x0
     ed0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ed6:	40 20 14 86 22 00 	            fcmp\.le\.s2 p4,p3=f4,f5
     edc:	00 00 00 20       	            nop\.b 0x0
     ee0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ee6:	40 20 14 86 23 00 	            fcmp\.le\.s3 p4,p3=f4,f5
     eec:	00 00 00 20       	            nop\.b 0x0
     ef0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ef6:	40 24 14 86 20 00 	            fcmp\.le\.unc\.s0 p4,p3=f4,f5
     efc:	00 00 00 20       	            nop\.b 0x0
     f00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f06:	40 24 14 86 20 00 	            fcmp\.le\.unc\.s0 p4,p3=f4,f5
     f0c:	00 00 00 20       	            nop\.b 0x0
     f10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f16:	40 24 14 86 21 00 	            fcmp\.le\.unc\.s1 p4,p3=f4,f5
     f1c:	00 00 00 20       	            nop\.b 0x0
     f20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f26:	40 24 14 86 22 00 	            fcmp\.le\.unc\.s2 p4,p3=f4,f5
     f2c:	00 00 00 20       	            nop\.b 0x0
     f30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f36:	40 24 14 86 23 00 	            fcmp\.le\.unc\.s3 p4,p3=f4,f5
     f3c:	00 00 00 20       	            nop\.b 0x0
     f40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f46:	40 28 10 06 24 00 	            fcmp\.lt\.s0 p4,p3=f5,f4
     f4c:	00 00 00 20       	            nop\.b 0x0
     f50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f56:	40 28 10 06 24 00 	            fcmp\.lt\.s0 p4,p3=f5,f4
     f5c:	00 00 00 20       	            nop\.b 0x0
     f60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f66:	40 28 10 06 25 00 	            fcmp\.lt\.s1 p4,p3=f5,f4
     f6c:	00 00 00 20       	            nop\.b 0x0
     f70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f76:	40 28 10 06 26 00 	            fcmp\.lt\.s2 p4,p3=f5,f4
     f7c:	00 00 00 20       	            nop\.b 0x0
     f80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f86:	40 28 10 06 27 00 	            fcmp\.lt\.s3 p4,p3=f5,f4
     f8c:	00 00 00 20       	            nop\.b 0x0
     f90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     f96:	40 2c 10 06 24 00 	            fcmp\.lt\.unc\.s0 p4,p3=f5,f4
     f9c:	00 00 00 20       	            nop\.b 0x0
     fa0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     fa6:	40 2c 10 06 24 00 	            fcmp\.lt\.unc\.s0 p4,p3=f5,f4
     fac:	00 00 00 20       	            nop\.b 0x0
     fb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     fb6:	40 2c 10 06 25 00 	            fcmp\.lt\.unc\.s1 p4,p3=f5,f4
     fbc:	00 00 00 20       	            nop\.b 0x0
     fc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     fc6:	40 2c 10 06 26 00 	            fcmp\.lt\.unc\.s2 p4,p3=f5,f4
     fcc:	00 00 00 20       	            nop\.b 0x0
     fd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     fd6:	40 2c 10 06 27 00 	            fcmp\.lt\.unc\.s3 p4,p3=f5,f4
     fdc:	00 00 00 20       	            nop\.b 0x0
     fe0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     fe6:	40 28 10 86 20 00 	            fcmp\.le\.s0 p4,p3=f5,f4
     fec:	00 00 00 20       	            nop\.b 0x0
     ff0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
     ff6:	40 28 10 86 20 00 	            fcmp\.le\.s0 p4,p3=f5,f4
     ffc:	00 00 00 20       	            nop\.b 0x0
    1000:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1006:	40 28 10 86 21 00 	            fcmp\.le\.s1 p4,p3=f5,f4
    100c:	00 00 00 20       	            nop\.b 0x0
    1010:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1016:	40 28 10 86 22 00 	            fcmp\.le\.s2 p4,p3=f5,f4
    101c:	00 00 00 20       	            nop\.b 0x0
    1020:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1026:	40 28 10 86 23 00 	            fcmp\.le\.s3 p4,p3=f5,f4
    102c:	00 00 00 20       	            nop\.b 0x0
    1030:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1036:	40 2c 10 86 20 00 	            fcmp\.le\.unc\.s0 p4,p3=f5,f4
    103c:	00 00 00 20       	            nop\.b 0x0
    1040:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1046:	40 2c 10 86 20 00 	            fcmp\.le\.unc\.s0 p4,p3=f5,f4
    104c:	00 00 00 20       	            nop\.b 0x0
    1050:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1056:	40 2c 10 86 21 00 	            fcmp\.le\.unc\.s1 p4,p3=f5,f4
    105c:	00 00 00 20       	            nop\.b 0x0
    1060:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1066:	40 2c 10 86 22 00 	            fcmp\.le\.unc\.s2 p4,p3=f5,f4
    106c:	00 00 00 20       	            nop\.b 0x0
    1070:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1076:	40 2c 10 86 23 00 	            fcmp\.le\.unc\.s3 p4,p3=f5,f4
    107c:	00 00 00 20       	            nop\.b 0x0
    1080:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1086:	40 20 14 86 24 00 	            fcmp\.unord\.s0 p4,p3=f4,f5
    108c:	00 00 00 20       	            nop\.b 0x0
    1090:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1096:	40 20 14 86 24 00 	            fcmp\.unord\.s0 p4,p3=f4,f5
    109c:	00 00 00 20       	            nop\.b 0x0
    10a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10a6:	40 20 14 86 25 00 	            fcmp\.unord\.s1 p4,p3=f4,f5
    10ac:	00 00 00 20       	            nop\.b 0x0
    10b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10b6:	40 20 14 86 26 00 	            fcmp\.unord\.s2 p4,p3=f4,f5
    10bc:	00 00 00 20       	            nop\.b 0x0
    10c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10c6:	40 20 14 86 27 00 	            fcmp\.unord\.s3 p4,p3=f4,f5
    10cc:	00 00 00 20       	            nop\.b 0x0
    10d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10d6:	40 24 14 86 24 00 	            fcmp\.unord\.unc\.s0 p4,p3=f4,f5
    10dc:	00 00 00 20       	            nop\.b 0x0
    10e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10e6:	40 24 14 86 24 00 	            fcmp\.unord\.unc\.s0 p4,p3=f4,f5
    10ec:	00 00 00 20       	            nop\.b 0x0
    10f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    10f6:	40 24 14 86 25 00 	            fcmp\.unord\.unc\.s1 p4,p3=f4,f5
    10fc:	00 00 00 20       	            nop\.b 0x0
    1100:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1106:	40 24 14 86 26 00 	            fcmp\.unord\.unc\.s2 p4,p3=f4,f5
    110c:	00 00 00 20       	            nop\.b 0x0
    1110:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1116:	40 24 14 86 27 00 	            fcmp\.unord\.unc\.s3 p4,p3=f4,f5
    111c:	00 00 00 20       	            nop\.b 0x0
    1120:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1126:	30 20 00 09 28 00 	            fclass\.m p3,p4=f4,0x100
    112c:	00 00 00 20       	            nop\.b 0x0
    1130:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1136:	40 20 00 07 28 00 	            fclass\.m p4,p3=f4,0x100
    113c:	00 00 00 20       	            nop\.b 0x0
    1140:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1146:	30 20 80 08 28 00 	            fclass\.m p3,p4=f4,0x80
    114c:	00 00 00 20       	            nop\.b 0x0
    1150:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1156:	40 20 80 06 28 00 	            fclass\.m p4,p3=f4,0x80
    115c:	00 00 00 20       	            nop\.b 0x0
    1160:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1166:	30 20 40 08 28 00 	            fclass\.m p3,p4=f4,0x40
    116c:	00 00 00 20       	            nop\.b 0x0
    1170:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1176:	40 20 40 06 28 00 	            fclass\.m p4,p3=f4,0x40
    117c:	00 00 00 20       	            nop\.b 0x0
    1180:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1186:	30 20 00 88 28 00 	            fclass\.m p3,p4=f4,0x1
    118c:	00 00 00 20       	            nop\.b 0x0
    1190:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1196:	40 20 00 86 28 00 	            fclass\.m p4,p3=f4,0x1
    119c:	00 00 00 20       	            nop\.b 0x0
    11a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11a6:	30 20 00 08 29 00 	            fclass\.m p3,p4=f4,0x2
    11ac:	00 00 00 20       	            nop\.b 0x0
    11b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11b6:	40 20 00 06 29 00 	            fclass\.m p4,p3=f4,0x2
    11bc:	00 00 00 20       	            nop\.b 0x0
    11c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11c6:	30 20 08 88 29 00 	            fclass\.m p3,p4=f4,0xb
    11cc:	00 00 00 20       	            nop\.b 0x0
    11d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11d6:	40 20 08 86 29 00 	            fclass\.m p4,p3=f4,0xb
    11dc:	00 00 00 20       	            nop\.b 0x0
    11e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11e6:	30 20 10 88 29 00 	            fclass\.m p3,p4=f4,0x13
    11ec:	00 00 00 20       	            nop\.b 0x0
    11f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    11f6:	40 20 10 86 29 00 	            fclass\.m p4,p3=f4,0x13
    11fc:	00 00 00 20       	            nop\.b 0x0
    1200:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1206:	30 20 20 88 29 00 	            fclass\.m p3,p4=f4,0x23
    120c:	00 00 00 20       	            nop\.b 0x0
    1210:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1216:	40 20 20 86 29 00 	            fclass\.m p4,p3=f4,0x23
    121c:	00 00 00 20       	            nop\.b 0x0
    1220:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1226:	30 20 fc 89 29 00 	            fclass\.m p3,p4=f4,0x1ff
    122c:	00 00 00 20       	            nop\.b 0x0
    1230:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1236:	40 20 fc 87 29 00 	            fclass\.m p4,p3=f4,0x1ff
    123c:	00 00 00 20       	            nop\.b 0x0
    1240:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1246:	30 24 00 09 28 00 	            fclass\.m\.unc p3,p4=f4,0x100
    124c:	00 00 00 20       	            nop\.b 0x0
    1250:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1256:	40 24 00 07 28 00 	            fclass\.m\.unc p4,p3=f4,0x100
    125c:	00 00 00 20       	            nop\.b 0x0
    1260:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1266:	30 24 80 08 28 00 	            fclass\.m\.unc p3,p4=f4,0x80
    126c:	00 00 00 20       	            nop\.b 0x0
    1270:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1276:	40 24 80 06 28 00 	            fclass\.m\.unc p4,p3=f4,0x80
    127c:	00 00 00 20       	            nop\.b 0x0
    1280:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1286:	30 24 40 08 28 00 	            fclass\.m\.unc p3,p4=f4,0x40
    128c:	00 00 00 20       	            nop\.b 0x0
    1290:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1296:	40 24 40 06 28 00 	            fclass\.m\.unc p4,p3=f4,0x40
    129c:	00 00 00 20       	            nop\.b 0x0
    12a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12a6:	30 24 00 88 28 00 	            fclass\.m\.unc p3,p4=f4,0x1
    12ac:	00 00 00 20       	            nop\.b 0x0
    12b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12b6:	40 24 00 86 28 00 	            fclass\.m\.unc p4,p3=f4,0x1
    12bc:	00 00 00 20       	            nop\.b 0x0
    12c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12c6:	30 24 00 08 29 00 	            fclass\.m\.unc p3,p4=f4,0x2
    12cc:	00 00 00 20       	            nop\.b 0x0
    12d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12d6:	40 24 00 06 29 00 	            fclass\.m\.unc p4,p3=f4,0x2
    12dc:	00 00 00 20       	            nop\.b 0x0
    12e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12e6:	30 24 08 88 29 00 	            fclass\.m\.unc p3,p4=f4,0xb
    12ec:	00 00 00 20       	            nop\.b 0x0
    12f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    12f6:	40 24 08 86 29 00 	            fclass\.m\.unc p4,p3=f4,0xb
    12fc:	00 00 00 20       	            nop\.b 0x0
    1300:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1306:	30 24 10 88 29 00 	            fclass\.m\.unc p3,p4=f4,0x13
    130c:	00 00 00 20       	            nop\.b 0x0
    1310:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1316:	40 24 10 86 29 00 	            fclass\.m\.unc p4,p3=f4,0x13
    131c:	00 00 00 20       	            nop\.b 0x0
    1320:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1326:	30 24 20 88 29 00 	            fclass\.m\.unc p3,p4=f4,0x23
    132c:	00 00 00 20       	            nop\.b 0x0
    1330:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1336:	40 24 20 86 29 00 	            fclass\.m\.unc p4,p3=f4,0x23
    133c:	00 00 00 20       	            nop\.b 0x0
    1340:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1346:	30 24 fc 89 29 00 	            fclass\.m\.unc p3,p4=f4,0x1ff
    134c:	00 00 00 20       	            nop\.b 0x0
    1350:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1356:	40 24 fc 87 29 00 	            fclass\.m\.unc p4,p3=f4,0x1ff
    135c:	00 00 00 20       	            nop\.b 0x0
    1360:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1366:	40 30 1c 8a 00 00 	            frcpa\.s0 f4,p5=f6,f7
    136c:	00 00 00 20       	            nop\.b 0x0
    1370:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1376:	40 30 1c 8a 00 00 	            frcpa\.s0 f4,p5=f6,f7
    137c:	00 00 00 20       	            nop\.b 0x0
    1380:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1386:	40 30 1c 8a 01 00 	            frcpa\.s1 f4,p5=f6,f7
    138c:	00 00 00 20       	            nop\.b 0x0
    1390:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1396:	40 30 1c 8a 02 00 	            frcpa\.s2 f4,p5=f6,f7
    139c:	00 00 00 20       	            nop\.b 0x0
    13a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13a6:	40 30 1c 8a 03 00 	            frcpa\.s3 f4,p5=f6,f7
    13ac:	00 00 00 20       	            nop\.b 0x0
    13b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13b6:	40 30 1c 8a 08 00 	            fprcpa\.s0 f4,p5=f6,f7
    13bc:	00 00 00 20       	            nop\.b 0x0
    13c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13c6:	40 30 1c 8a 08 00 	            fprcpa\.s0 f4,p5=f6,f7
    13cc:	00 00 00 20       	            nop\.b 0x0
    13d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13d6:	40 30 1c 8a 09 00 	            fprcpa\.s1 f4,p5=f6,f7
    13dc:	00 00 00 20       	            nop\.b 0x0
    13e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13e6:	40 30 1c 8a 0a 00 	            fprcpa\.s2 f4,p5=f6,f7
    13ec:	00 00 00 20       	            nop\.b 0x0
    13f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    13f6:	40 30 1c 8a 0b 00 	            fprcpa\.s3 f4,p5=f6,f7
    13fc:	00 00 00 20       	            nop\.b 0x0
    1400:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1406:	40 00 18 8a 04 00 	            frsqrta\.s0 f4,p5=f6
    140c:	00 00 00 20       	            nop\.b 0x0
    1410:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1416:	40 00 18 8a 04 00 	            frsqrta\.s0 f4,p5=f6
    141c:	00 00 00 20       	            nop\.b 0x0
    1420:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1426:	40 00 18 8a 05 00 	            frsqrta\.s1 f4,p5=f6
    142c:	00 00 00 20       	            nop\.b 0x0
    1430:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1436:	40 00 18 8a 06 00 	            frsqrta\.s2 f4,p5=f6
    143c:	00 00 00 20       	            nop\.b 0x0
    1440:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1446:	40 00 18 8a 07 00 	            frsqrta\.s3 f4,p5=f6
    144c:	00 00 00 20       	            nop\.b 0x0
    1450:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1456:	40 00 18 8a 0c 00 	            fprsqrta\.s0 f4,p5=f6
    145c:	00 00 00 20       	            nop\.b 0x0
    1460:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1466:	40 00 18 8a 0c 00 	            fprsqrta\.s0 f4,p5=f6
    146c:	00 00 00 20       	            nop\.b 0x0
    1470:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1476:	40 00 18 8a 0d 00 	            fprsqrta\.s1 f4,p5=f6
    147c:	00 00 00 20       	            nop\.b 0x0
    1480:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1486:	40 00 18 8a 0e 00 	            fprsqrta\.s2 f4,p5=f6
    148c:	00 00 00 20       	            nop\.b 0x0
    1490:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1496:	40 00 18 8a 0f 00 	            fprsqrta\.s3 f4,p5=f6
    149c:	00 00 00 20       	            nop\.b 0x0
    14a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14a6:	40 28 18 28 00 00 	            fmin\.s0 f4=f5,f6
    14ac:	00 00 00 20       	            nop\.b 0x0
    14b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14b6:	40 28 18 28 00 00 	            fmin\.s0 f4=f5,f6
    14bc:	00 00 00 20       	            nop\.b 0x0
    14c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14c6:	40 28 18 28 01 00 	            fmin\.s1 f4=f5,f6
    14cc:	00 00 00 20       	            nop\.b 0x0
    14d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14d6:	40 28 18 28 02 00 	            fmin\.s2 f4=f5,f6
    14dc:	00 00 00 20       	            nop\.b 0x0
    14e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14e6:	40 28 18 28 03 00 	            fmin\.s3 f4=f5,f6
    14ec:	00 00 00 20       	            nop\.b 0x0
    14f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    14f6:	40 28 18 2a 00 00 	            fmax\.s0 f4=f5,f6
    14fc:	00 00 00 20       	            nop\.b 0x0
    1500:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1506:	40 28 18 2a 00 00 	            fmax\.s0 f4=f5,f6
    150c:	00 00 00 20       	            nop\.b 0x0
    1510:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1516:	40 28 18 2a 01 00 	            fmax\.s1 f4=f5,f6
    151c:	00 00 00 20       	            nop\.b 0x0
    1520:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1526:	40 28 18 2a 02 00 	            fmax\.s2 f4=f5,f6
    152c:	00 00 00 20       	            nop\.b 0x0
    1530:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1536:	40 28 18 2a 03 00 	            fmax\.s3 f4=f5,f6
    153c:	00 00 00 20       	            nop\.b 0x0
    1540:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1546:	40 28 18 2c 00 00 	            famin\.s0 f4=f5,f6
    154c:	00 00 00 20       	            nop\.b 0x0
    1550:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1556:	40 28 18 2c 00 00 	            famin\.s0 f4=f5,f6
    155c:	00 00 00 20       	            nop\.b 0x0
    1560:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1566:	40 28 18 2c 01 00 	            famin\.s1 f4=f5,f6
    156c:	00 00 00 20       	            nop\.b 0x0
    1570:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1576:	40 28 18 2c 02 00 	            famin\.s2 f4=f5,f6
    157c:	00 00 00 20       	            nop\.b 0x0
    1580:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1586:	40 28 18 2c 03 00 	            famin\.s3 f4=f5,f6
    158c:	00 00 00 20       	            nop\.b 0x0
    1590:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1596:	40 28 18 2e 00 00 	            famax\.s0 f4=f5,f6
    159c:	00 00 00 20       	            nop\.b 0x0
    15a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15a6:	40 28 18 2e 00 00 	            famax\.s0 f4=f5,f6
    15ac:	00 00 00 20       	            nop\.b 0x0
    15b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15b6:	40 28 18 2e 01 00 	            famax\.s1 f4=f5,f6
    15bc:	00 00 00 20       	            nop\.b 0x0
    15c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15c6:	40 28 18 2e 02 00 	            famax\.s2 f4=f5,f6
    15cc:	00 00 00 20       	            nop\.b 0x0
    15d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15d6:	40 28 18 2e 03 00 	            famax\.s3 f4=f5,f6
    15dc:	00 00 00 20       	            nop\.b 0x0
    15e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15e6:	40 28 18 28 08 00 	            fpmin\.s0 f4=f5,f6
    15ec:	00 00 00 20       	            nop\.b 0x0
    15f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    15f6:	40 28 18 28 08 00 	            fpmin\.s0 f4=f5,f6
    15fc:	00 00 00 20       	            nop\.b 0x0
    1600:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1606:	40 28 18 28 09 00 	            fpmin\.s1 f4=f5,f6
    160c:	00 00 00 20       	            nop\.b 0x0
    1610:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1616:	40 28 18 28 0a 00 	            fpmin\.s2 f4=f5,f6
    161c:	00 00 00 20       	            nop\.b 0x0
    1620:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1626:	40 28 18 28 0b 00 	            fpmin\.s3 f4=f5,f6
    162c:	00 00 00 20       	            nop\.b 0x0
    1630:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1636:	40 28 18 2a 08 00 	            fpmax\.s0 f4=f5,f6
    163c:	00 00 00 20       	            nop\.b 0x0
    1640:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1646:	40 28 18 2a 08 00 	            fpmax\.s0 f4=f5,f6
    164c:	00 00 00 20       	            nop\.b 0x0
    1650:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1656:	40 28 18 2a 09 00 	            fpmax\.s1 f4=f5,f6
    165c:	00 00 00 20       	            nop\.b 0x0
    1660:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1666:	40 28 18 2a 0a 00 	            fpmax\.s2 f4=f5,f6
    166c:	00 00 00 20       	            nop\.b 0x0
    1670:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1676:	40 28 18 2a 0b 00 	            fpmax\.s3 f4=f5,f6
    167c:	00 00 00 20       	            nop\.b 0x0
    1680:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1686:	40 28 18 2c 08 00 	            fpamin\.s0 f4=f5,f6
    168c:	00 00 00 20       	            nop\.b 0x0
    1690:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1696:	40 28 18 2c 08 00 	            fpamin\.s0 f4=f5,f6
    169c:	00 00 00 20       	            nop\.b 0x0
    16a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16a6:	40 28 18 2c 09 00 	            fpamin\.s1 f4=f5,f6
    16ac:	00 00 00 20       	            nop\.b 0x0
    16b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16b6:	40 28 18 2c 0a 00 	            fpamin\.s2 f4=f5,f6
    16bc:	00 00 00 20       	            nop\.b 0x0
    16c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16c6:	40 28 18 2c 0b 00 	            fpamin\.s3 f4=f5,f6
    16cc:	00 00 00 20       	            nop\.b 0x0
    16d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16d6:	40 28 18 2e 08 00 	            fpamax\.s0 f4=f5,f6
    16dc:	00 00 00 20       	            nop\.b 0x0
    16e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16e6:	40 28 18 2e 08 00 	            fpamax\.s0 f4=f5,f6
    16ec:	00 00 00 20       	            nop\.b 0x0
    16f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    16f6:	40 28 18 2e 09 00 	            fpamax\.s1 f4=f5,f6
    16fc:	00 00 00 20       	            nop\.b 0x0
    1700:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1706:	40 28 18 2e 0a 00 	            fpamax\.s2 f4=f5,f6
    170c:	00 00 00 20       	            nop\.b 0x0
    1710:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1716:	40 28 18 2e 0b 00 	            fpamax\.s3 f4=f5,f6
    171c:	00 00 00 20       	            nop\.b 0x0
    1720:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1726:	30 20 14 60 08 00 	            fpcmp\.eq\.s0 f3=f4,f5
    172c:	00 00 00 20       	            nop\.b 0x0
    1730:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1736:	30 20 14 60 08 00 	            fpcmp\.eq\.s0 f3=f4,f5
    173c:	00 00 00 20       	            nop\.b 0x0
    1740:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1746:	30 20 14 60 09 00 	            fpcmp\.eq\.s1 f3=f4,f5
    174c:	00 00 00 20       	            nop\.b 0x0
    1750:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1756:	30 20 14 60 0a 00 	            fpcmp\.eq\.s2 f3=f4,f5
    175c:	00 00 00 20       	            nop\.b 0x0
    1760:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1766:	30 20 14 60 0b 00 	            fpcmp\.eq\.s3 f3=f4,f5
    176c:	00 00 00 20       	            nop\.b 0x0
    1770:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1776:	30 20 14 62 08 00 	            fpcmp\.lt\.s0 f3=f4,f5
    177c:	00 00 00 20       	            nop\.b 0x0
    1780:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1786:	30 20 14 62 08 00 	            fpcmp\.lt\.s0 f3=f4,f5
    178c:	00 00 00 20       	            nop\.b 0x0
    1790:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1796:	30 20 14 62 09 00 	            fpcmp\.lt\.s1 f3=f4,f5
    179c:	00 00 00 20       	            nop\.b 0x0
    17a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17a6:	30 20 14 62 0a 00 	            fpcmp\.lt\.s2 f3=f4,f5
    17ac:	00 00 00 20       	            nop\.b 0x0
    17b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17b6:	30 20 14 62 0b 00 	            fpcmp\.lt\.s3 f3=f4,f5
    17bc:	00 00 00 20       	            nop\.b 0x0
    17c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17c6:	30 20 14 64 08 00 	            fpcmp\.le\.s0 f3=f4,f5
    17cc:	00 00 00 20       	            nop\.b 0x0
    17d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17d6:	30 20 14 64 08 00 	            fpcmp\.le\.s0 f3=f4,f5
    17dc:	00 00 00 20       	            nop\.b 0x0
    17e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17e6:	30 20 14 64 09 00 	            fpcmp\.le\.s1 f3=f4,f5
    17ec:	00 00 00 20       	            nop\.b 0x0
    17f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    17f6:	30 20 14 64 0a 00 	            fpcmp\.le\.s2 f3=f4,f5
    17fc:	00 00 00 20       	            nop\.b 0x0
    1800:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1806:	30 20 14 64 0b 00 	            fpcmp\.le\.s3 f3=f4,f5
    180c:	00 00 00 20       	            nop\.b 0x0
    1810:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1816:	30 20 14 66 08 00 	            fpcmp\.unord\.s0 f3=f4,f5
    181c:	00 00 00 20       	            nop\.b 0x0
    1820:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1826:	30 20 14 66 08 00 	            fpcmp\.unord\.s0 f3=f4,f5
    182c:	00 00 00 20       	            nop\.b 0x0
    1830:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1836:	30 20 14 66 09 00 	            fpcmp\.unord\.s1 f3=f4,f5
    183c:	00 00 00 20       	            nop\.b 0x0
    1840:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1846:	30 20 14 66 0a 00 	            fpcmp\.unord\.s2 f3=f4,f5
    184c:	00 00 00 20       	            nop\.b 0x0
    1850:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1856:	30 20 14 66 0b 00 	            fpcmp\.unord\.s3 f3=f4,f5
    185c:	00 00 00 20       	            nop\.b 0x0
    1860:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1866:	30 28 10 62 08 00 	            fpcmp\.lt\.s0 f3=f5,f4
    186c:	00 00 00 20       	            nop\.b 0x0
    1870:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1876:	30 28 10 62 08 00 	            fpcmp\.lt\.s0 f3=f5,f4
    187c:	00 00 00 20       	            nop\.b 0x0
    1880:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1886:	30 28 10 62 09 00 	            fpcmp\.lt\.s1 f3=f5,f4
    188c:	00 00 00 20       	            nop\.b 0x0
    1890:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1896:	30 28 10 62 0a 00 	            fpcmp\.lt\.s2 f3=f5,f4
    189c:	00 00 00 20       	            nop\.b 0x0
    18a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18a6:	30 28 10 62 0b 00 	            fpcmp\.lt\.s3 f3=f5,f4
    18ac:	00 00 00 20       	            nop\.b 0x0
    18b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18b6:	30 28 10 64 08 00 	            fpcmp\.le\.s0 f3=f5,f4
    18bc:	00 00 00 20       	            nop\.b 0x0
    18c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18c6:	30 28 10 64 08 00 	            fpcmp\.le\.s0 f3=f5,f4
    18cc:	00 00 00 20       	            nop\.b 0x0
    18d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18d6:	30 28 10 64 09 00 	            fpcmp\.le\.s1 f3=f5,f4
    18dc:	00 00 00 20       	            nop\.b 0x0
    18e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18e6:	30 28 10 64 0a 00 	            fpcmp\.le\.s2 f3=f5,f4
    18ec:	00 00 00 20       	            nop\.b 0x0
    18f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    18f6:	30 28 10 64 0b 00 	            fpcmp\.le\.s3 f3=f5,f4
    18fc:	00 00 00 20       	            nop\.b 0x0
    1900:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1906:	30 20 14 68 08 00 	            fpcmp\.neq\.s0 f3=f4,f5
    190c:	00 00 00 20       	            nop\.b 0x0
    1910:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1916:	30 20 14 68 08 00 	            fpcmp\.neq\.s0 f3=f4,f5
    191c:	00 00 00 20       	            nop\.b 0x0
    1920:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1926:	30 20 14 68 09 00 	            fpcmp\.neq\.s1 f3=f4,f5
    192c:	00 00 00 20       	            nop\.b 0x0
    1930:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1936:	30 20 14 68 0a 00 	            fpcmp\.neq\.s2 f3=f4,f5
    193c:	00 00 00 20       	            nop\.b 0x0
    1940:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1946:	30 20 14 68 0b 00 	            fpcmp\.neq\.s3 f3=f4,f5
    194c:	00 00 00 20       	            nop\.b 0x0
    1950:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1956:	30 20 14 6a 08 00 	            fpcmp\.nlt\.s0 f3=f4,f5
    195c:	00 00 00 20       	            nop\.b 0x0
    1960:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1966:	30 20 14 6a 08 00 	            fpcmp\.nlt\.s0 f3=f4,f5
    196c:	00 00 00 20       	            nop\.b 0x0
    1970:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1976:	30 20 14 6a 09 00 	            fpcmp\.nlt\.s1 f3=f4,f5
    197c:	00 00 00 20       	            nop\.b 0x0
    1980:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1986:	30 20 14 6a 0a 00 	            fpcmp\.nlt\.s2 f3=f4,f5
    198c:	00 00 00 20       	            nop\.b 0x0
    1990:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1996:	30 20 14 6a 0b 00 	            fpcmp\.nlt\.s3 f3=f4,f5
    199c:	00 00 00 20       	            nop\.b 0x0
    19a0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19a6:	30 20 14 6c 08 00 	            fpcmp\.nle\.s0 f3=f4,f5
    19ac:	00 00 00 20       	            nop\.b 0x0
    19b0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19b6:	30 20 14 6c 08 00 	            fpcmp\.nle\.s0 f3=f4,f5
    19bc:	00 00 00 20       	            nop\.b 0x0
    19c0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19c6:	30 20 14 6c 09 00 	            fpcmp\.nle\.s1 f3=f4,f5
    19cc:	00 00 00 20       	            nop\.b 0x0
    19d0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19d6:	30 20 14 6c 0a 00 	            fpcmp\.nle\.s2 f3=f4,f5
    19dc:	00 00 00 20       	            nop\.b 0x0
    19e0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19e6:	30 20 14 6c 0b 00 	            fpcmp\.nle\.s3 f3=f4,f5
    19ec:	00 00 00 20       	            nop\.b 0x0
    19f0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    19f6:	30 28 10 6a 08 00 	            fpcmp\.nlt\.s0 f3=f5,f4
    19fc:	00 00 00 20       	            nop\.b 0x0
    1a00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a06:	30 28 10 6a 08 00 	            fpcmp\.nlt\.s0 f3=f5,f4
    1a0c:	00 00 00 20       	            nop\.b 0x0
    1a10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a16:	30 28 10 6a 09 00 	            fpcmp\.nlt\.s1 f3=f5,f4
    1a1c:	00 00 00 20       	            nop\.b 0x0
    1a20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a26:	30 28 10 6a 0a 00 	            fpcmp\.nlt\.s2 f3=f5,f4
    1a2c:	00 00 00 20       	            nop\.b 0x0
    1a30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a36:	30 28 10 6a 0b 00 	            fpcmp\.nlt\.s3 f3=f5,f4
    1a3c:	00 00 00 20       	            nop\.b 0x0
    1a40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a46:	30 28 10 6c 08 00 	            fpcmp\.nle\.s0 f3=f5,f4
    1a4c:	00 00 00 20       	            nop\.b 0x0
    1a50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a56:	30 28 10 6c 08 00 	            fpcmp\.nle\.s0 f3=f5,f4
    1a5c:	00 00 00 20       	            nop\.b 0x0
    1a60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a66:	30 28 10 6c 09 00 	            fpcmp\.nle\.s1 f3=f5,f4
    1a6c:	00 00 00 20       	            nop\.b 0x0
    1a70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a76:	30 28 10 6c 0a 00 	            fpcmp\.nle\.s2 f3=f5,f4
    1a7c:	00 00 00 20       	            nop\.b 0x0
    1a80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a86:	30 28 10 6c 0b 00 	            fpcmp\.nle\.s3 f3=f5,f4
    1a8c:	00 00 00 20       	            nop\.b 0x0
    1a90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1a96:	30 20 14 6e 08 00 	            fpcmp\.ord\.s0 f3=f4,f5
    1a9c:	00 00 00 20       	            nop\.b 0x0
    1aa0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1aa6:	30 20 14 6e 08 00 	            fpcmp\.ord\.s0 f3=f4,f5
    1aac:	00 00 00 20       	            nop\.b 0x0
    1ab0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ab6:	30 20 14 6e 09 00 	            fpcmp\.ord\.s1 f3=f4,f5
    1abc:	00 00 00 20       	            nop\.b 0x0
    1ac0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ac6:	30 20 14 6e 0a 00 	            fpcmp\.ord\.s2 f3=f4,f5
    1acc:	00 00 00 20       	            nop\.b 0x0
    1ad0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ad6:	30 20 14 6e 0b 00 	            fpcmp\.ord\.s3 f3=f4,f5
    1adc:	00 00 00 20       	            nop\.b 0x0
    1ae0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ae6:	40 28 18 20 00 00 	            fmerge\.s f4=f5,f6
    1aec:	00 00 00 20       	            nop\.b 0x0
    1af0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1af6:	40 28 18 22 00 00 	            fmerge\.ns f4=f5,f6
    1afc:	00 00 00 20       	            nop\.b 0x0
    1b00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b06:	40 28 18 24 00 00 	            fmerge\.se f4=f5,f6
    1b0c:	00 00 00 20       	            nop\.b 0x0
    1b10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b16:	40 28 18 72 00 00 	            fmix\.lr f4=f5,f6
    1b1c:	00 00 00 20       	            nop\.b 0x0
    1b20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b26:	40 28 18 74 00 00 	            fmix\.r f4=f5,f6
    1b2c:	00 00 00 20       	            nop\.b 0x0
    1b30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b36:	40 28 18 76 00 00 	            fmix\.l f4=f5,f6
    1b3c:	00 00 00 20       	            nop\.b 0x0
    1b40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b46:	40 28 18 7a 00 00 	            fsxt\.l f4=f5,f6
    1b4c:	00 00 00 20       	            nop\.b 0x0
    1b50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b56:	40 28 18 50 00 00 	            fpack f4=f5,f6
    1b5c:	00 00 00 20       	            nop\.b 0x0
    1b60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b66:	40 28 18 68 00 00 	            fswap f4=f5,f6
    1b6c:	00 00 00 20       	            nop\.b 0x0
    1b70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b76:	40 28 18 6a 00 00 	            fswap\.nl f4=f5,f6
    1b7c:	00 00 00 20       	            nop\.b 0x0
    1b80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b86:	40 28 18 6c 00 00 	            fswap\.nr f4=f5,f6
    1b8c:	00 00 00 20       	            nop\.b 0x0
    1b90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1b96:	40 28 18 58 00 00 	            fand f4=f5,f6
    1b9c:	00 00 00 20       	            nop\.b 0x0
    1ba0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ba6:	40 28 18 5a 00 00 	            fandcm f4=f5,f6
    1bac:	00 00 00 20       	            nop\.b 0x0
    1bb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1bb6:	40 28 18 5c 00 00 	            for f4=f5,f6
    1bbc:	00 00 00 20       	            nop\.b 0x0
    1bc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1bc6:	40 28 18 5e 00 00 	            fxor f4=f5,f6
    1bcc:	00 00 00 20       	            nop\.b 0x0
    1bd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1bd6:	40 28 18 20 08 00 	            fpmerge\.s f4=f5,f6
    1bdc:	00 00 00 20       	            nop\.b 0x0
    1be0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1be6:	40 28 18 22 08 00 	            fpmerge\.ns f4=f5,f6
    1bec:	00 00 00 20       	            nop\.b 0x0
    1bf0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1bf6:	40 28 18 24 08 00 	            fpmerge\.se f4=f5,f6
    1bfc:	00 00 00 20       	            nop\.b 0x0
    1c00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c06:	40 00 14 20 00 00 	            fabs f4=f5
    1c0c:	00 00 00 20       	            nop\.b 0x0
    1c10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c16:	40 28 14 22 00 00 	            fneg f4=f5
    1c1c:	00 00 00 20       	            nop\.b 0x0
    1c20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c26:	40 00 14 22 00 00 	            fnegabs f4=f5
    1c2c:	00 00 00 20       	            nop\.b 0x0
    1c30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c36:	40 00 14 20 08 00 	            fpabs f4=f5
    1c3c:	00 00 00 20       	            nop\.b 0x0
    1c40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c46:	40 28 14 22 08 00 	            fpneg f4=f5
    1c4c:	00 00 00 20       	            nop\.b 0x0
    1c50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c56:	40 00 14 22 08 00 	            fpnegabs f4=f5
    1c5c:	00 00 00 20       	            nop\.b 0x0
    1c60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c66:	40 28 00 30 00 00 	            fcvt\.fx\.s0 f4=f5
    1c6c:	00 00 00 20       	            nop\.b 0x0
    1c70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c76:	40 28 00 30 00 00 	            fcvt\.fx\.s0 f4=f5
    1c7c:	00 00 00 20       	            nop\.b 0x0
    1c80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c86:	40 28 00 30 01 00 	            fcvt\.fx\.s1 f4=f5
    1c8c:	00 00 00 20       	            nop\.b 0x0
    1c90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1c96:	40 28 00 30 02 00 	            fcvt\.fx\.s2 f4=f5
    1c9c:	00 00 00 20       	            nop\.b 0x0
    1ca0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ca6:	40 28 00 30 03 00 	            fcvt\.fx\.s3 f4=f5
    1cac:	00 00 00 20       	            nop\.b 0x0
    1cb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1cb6:	40 28 00 34 00 00 	            fcvt\.fx\.trunc\.s0 f4=f5
    1cbc:	00 00 00 20       	            nop\.b 0x0
    1cc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1cc6:	40 28 00 34 00 00 	            fcvt\.fx\.trunc\.s0 f4=f5
    1ccc:	00 00 00 20       	            nop\.b 0x0
    1cd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1cd6:	40 28 00 34 01 00 	            fcvt\.fx\.trunc\.s1 f4=f5
    1cdc:	00 00 00 20       	            nop\.b 0x0
    1ce0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ce6:	40 28 00 34 02 00 	            fcvt\.fx\.trunc\.s2 f4=f5
    1cec:	00 00 00 20       	            nop\.b 0x0
    1cf0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1cf6:	40 28 00 34 03 00 	            fcvt\.fx\.trunc\.s3 f4=f5
    1cfc:	00 00 00 20       	            nop\.b 0x0
    1d00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d06:	40 28 00 32 00 00 	            fcvt\.fxu\.s0 f4=f5
    1d0c:	00 00 00 20       	            nop\.b 0x0
    1d10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d16:	40 28 00 32 00 00 	            fcvt\.fxu\.s0 f4=f5
    1d1c:	00 00 00 20       	            nop\.b 0x0
    1d20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d26:	40 28 00 32 01 00 	            fcvt\.fxu\.s1 f4=f5
    1d2c:	00 00 00 20       	            nop\.b 0x0
    1d30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d36:	40 28 00 32 02 00 	            fcvt\.fxu\.s2 f4=f5
    1d3c:	00 00 00 20       	            nop\.b 0x0
    1d40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d46:	40 28 00 32 03 00 	            fcvt\.fxu\.s3 f4=f5
    1d4c:	00 00 00 20       	            nop\.b 0x0
    1d50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d56:	40 28 00 36 00 00 	            fcvt\.fxu\.trunc\.s0 f4=f5
    1d5c:	00 00 00 20       	            nop\.b 0x0
    1d60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d66:	40 28 00 36 00 00 	            fcvt\.fxu\.trunc\.s0 f4=f5
    1d6c:	00 00 00 20       	            nop\.b 0x0
    1d70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d76:	40 28 00 36 01 00 	            fcvt\.fxu\.trunc\.s1 f4=f5
    1d7c:	00 00 00 20       	            nop\.b 0x0
    1d80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d86:	40 28 00 36 02 00 	            fcvt\.fxu\.trunc\.s2 f4=f5
    1d8c:	00 00 00 20       	            nop\.b 0x0
    1d90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1d96:	40 28 00 36 03 00 	            fcvt\.fxu\.trunc\.s3 f4=f5
    1d9c:	00 00 00 20       	            nop\.b 0x0
    1da0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1da6:	40 28 00 30 08 00 	            fpcvt\.fx\.s0 f4=f5
    1dac:	00 00 00 20       	            nop\.b 0x0
    1db0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1db6:	40 28 00 30 08 00 	            fpcvt\.fx\.s0 f4=f5
    1dbc:	00 00 00 20       	            nop\.b 0x0
    1dc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1dc6:	40 28 00 30 09 00 	            fpcvt\.fx\.s1 f4=f5
    1dcc:	00 00 00 20       	            nop\.b 0x0
    1dd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1dd6:	40 28 00 30 0a 00 	            fpcvt\.fx\.s2 f4=f5
    1ddc:	00 00 00 20       	            nop\.b 0x0
    1de0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1de6:	40 28 00 30 0b 00 	            fpcvt\.fx\.s3 f4=f5
    1dec:	00 00 00 20       	            nop\.b 0x0
    1df0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1df6:	40 28 00 34 08 00 	            fpcvt\.fx\.trunc\.s0 f4=f5
    1dfc:	00 00 00 20       	            nop\.b 0x0
    1e00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e06:	40 28 00 34 08 00 	            fpcvt\.fx\.trunc\.s0 f4=f5
    1e0c:	00 00 00 20       	            nop\.b 0x0
    1e10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e16:	40 28 00 34 09 00 	            fpcvt\.fx\.trunc\.s1 f4=f5
    1e1c:	00 00 00 20       	            nop\.b 0x0
    1e20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e26:	40 28 00 34 0a 00 	            fpcvt\.fx\.trunc\.s2 f4=f5
    1e2c:	00 00 00 20       	            nop\.b 0x0
    1e30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e36:	40 28 00 34 0b 00 	            fpcvt\.fx\.trunc\.s3 f4=f5
    1e3c:	00 00 00 20       	            nop\.b 0x0
    1e40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e46:	40 28 00 32 08 00 	            fpcvt\.fxu\.s0 f4=f5
    1e4c:	00 00 00 20       	            nop\.b 0x0
    1e50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e56:	40 28 00 32 08 00 	            fpcvt\.fxu\.s0 f4=f5
    1e5c:	00 00 00 20       	            nop\.b 0x0
    1e60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e66:	40 28 00 32 09 00 	            fpcvt\.fxu\.s1 f4=f5
    1e6c:	00 00 00 20       	            nop\.b 0x0
    1e70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e76:	40 28 00 32 0a 00 	            fpcvt\.fxu\.s2 f4=f5
    1e7c:	00 00 00 20       	            nop\.b 0x0
    1e80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e86:	40 28 00 32 0b 00 	            fpcvt\.fxu\.s3 f4=f5
    1e8c:	00 00 00 20       	            nop\.b 0x0
    1e90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1e96:	40 28 00 36 08 00 	            fpcvt\.fxu\.trunc\.s0 f4=f5
    1e9c:	00 00 00 20       	            nop\.b 0x0
    1ea0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ea6:	40 28 00 36 08 00 	            fpcvt\.fxu\.trunc\.s0 f4=f5
    1eac:	00 00 00 20       	            nop\.b 0x0
    1eb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1eb6:	40 28 00 36 09 00 	            fpcvt\.fxu\.trunc\.s1 f4=f5
    1ebc:	00 00 00 20       	            nop\.b 0x0
    1ec0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ec6:	40 28 00 36 0a 00 	            fpcvt\.fxu\.trunc\.s2 f4=f5
    1ecc:	00 00 00 20       	            nop\.b 0x0
    1ed0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ed6:	40 28 00 36 0b 00 	            fpcvt\.fxu\.trunc\.s3 f4=f5
    1edc:	00 00 00 20       	            nop\.b 0x0
    1ee0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ee6:	40 28 00 38 00 00 	            fcvt\.xf f4=f5
    1eec:	00 00 00 20       	            nop\.b 0x0
    1ef0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ef6:	40 00 14 02 40 00 	            fnorm\.s0 f4=f5
    1efc:	00 00 00 20       	            nop\.b 0x0
    1f00:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f06:	00 00 00 08 00 00 	            fsetc\.s0 0x0,0x0
    1f0c:	00 00 00 20       	            nop\.b 0x0
    1f10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f16:	00 f8 fd 08 00 00 	            fsetc\.s0 0x3f,0x3f
    1f1c:	00 00 00 20       	            nop\.b 0x0
    1f20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f26:	00 00 00 08 00 00 	            fsetc\.s0 0x0,0x0
    1f2c:	00 00 00 20       	            nop\.b 0x0
    1f30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f36:	00 f8 fd 08 00 00 	            fsetc\.s0 0x3f,0x3f
    1f3c:	00 00 00 20       	            nop\.b 0x0
    1f40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f46:	00 00 00 08 01 00 	            fsetc\.s1 0x0,0x0
    1f4c:	00 00 00 20       	            nop\.b 0x0
    1f50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f56:	00 f8 fd 08 01 00 	            fsetc\.s1 0x3f,0x3f
    1f5c:	00 00 00 20       	            nop\.b 0x0
    1f60:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f66:	00 00 00 08 02 00 	            fsetc\.s2 0x0,0x0
    1f6c:	00 00 00 20       	            nop\.b 0x0
    1f70:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f76:	00 f8 fd 08 02 00 	            fsetc\.s2 0x3f,0x3f
    1f7c:	00 00 00 20       	            nop\.b 0x0
    1f80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f86:	00 00 00 08 03 00 	            fsetc\.s3 0x0,0x0
    1f8c:	00 00 00 20       	            nop\.b 0x0
    1f90:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1f96:	00 f8 fd 08 03 00 	            fsetc\.s3 0x3f,0x3f
    1f9c:	00 00 00 20       	            nop\.b 0x0
    1fa0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1fa6:	00 00 00 0a 00 00 	            fclrf\.s0
    1fac:	00 00 00 20       	            nop\.b 0x0
    1fb0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1fb6:	00 00 00 0a 00 00 	            fclrf\.s0
    1fbc:	00 00 00 20       	            nop\.b 0x0
    1fc0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1fc6:	00 00 00 0a 01 00 	            fclrf\.s1
    1fcc:	00 00 00 20       	            nop\.b 0x0
    1fd0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1fd6:	00 00 00 0a 02 00 	            fclrf\.s2
    1fdc:	00 00 00 20       	            nop\.b 0x0
    1fe0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1fe6:	00 00 00 0a 03 00 	            fclrf\.s3
    1fec:	00 00 00 20       	            nop\.b 0x0
    1ff0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    1ff6:	10 e0 ff 10 04 00 	            fchkf\.s0 0 <_start>
    1ffc:	00 00 00 20       	            nop\.b 0x0
    2000:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2006:	00 e0 ff 10 04 00 	            fchkf\.s0 0 <_start>
    200c:	00 00 00 20       	            nop\.b 0x0
    2010:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2016:	f0 df ff 10 05 00 	            fchkf\.s1 0 <_start>
    201c:	00 00 00 20       	            nop\.b 0x0
    2020:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2026:	e0 df ff 10 06 00 	            fchkf\.s2 0 <_start>
    202c:	00 00 00 20       	            nop\.b 0x0
    2030:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2036:	d0 df ff 10 07 00 	            fchkf\.s3 0 <_start>
    203c:	00 00 00 20       	            nop\.b 0x0
    2040:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2046:	00 00 00 00 00 00 	            break\.f 0x0
    204c:	00 00 00 20       	            nop\.b 0x0
    2050:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2056:	00 00 00 02 00 00 	            nop\.f 0x0
    205c:	00 00 00 20       	            nop\.b 0x0;;
    2060:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2066:	00 00 00 03 00 00 	            hint\.f 0x0
    206c:	00 00 00 20       	            nop\.b 0x0
    2070:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2076:	00 00 00 03 00 00 	            hint\.f 0x0
    207c:	00 00 00 20       	            nop\.b 0x0
    2080:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
    2086:	f0 ff 1f 03 00 00 	            hint\.f 0x1ffff
    208c:	00 00 00 20       	            nop\.b 0x0;;
