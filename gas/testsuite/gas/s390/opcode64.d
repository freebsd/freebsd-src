#name: s390x opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
   0:	e3 95 af ff 00 08 [ 	]*ag	%r9,4095\(%r5,%r10\)
   6:	e3 95 af ff 00 18 [ 	]*agf	%r9,4095\(%r5,%r10\)
   c:	b9 18 00 96 [ 	]*agfr	%r9,%r6
  10:	a7 9b 80 01 [ 	]*aghi	%r9,-32767
  14:	b9 08 00 96 [ 	]*agr	%r9,%r6
  18:	e3 95 af ff 00 98 [ 	]*alc	%r9,4095\(%r5,%r10\)
  1e:	e3 95 af ff 00 88 [ 	]*alcg	%r9,4095\(%r5,%r10\)
  24:	b9 88 00 96 [ 	]*alcgr	%r9,%r6
  28:	b9 98 00 96 [ 	]*alcr	%r9,%r6
  2c:	e3 95 af ff 00 0a [ 	]*alg	%r9,4095\(%r5,%r10\)
  32:	e3 95 af ff 00 1a [ 	]*algf	%r9,4095\(%r5,%r10\)
  38:	b9 1a 00 96 [ 	]*algfr	%r9,%r6
  3c:	b9 0a 00 96 [ 	]*algr	%r9,%r6
  40:	e3 65 af ff 00 46 [ 	]*bctg	%r6,4095\(%r5,%r10\)
  46:	b9 46 00 69 [ 	]*bctgr	%r6,%r9
  4a:	c0 65 00 00 00 00 [ 	]*brasl	%r6,4a <foo\+0x4a>
  50:	c0 f4 00 00 00 00 [ 	]*jg	50 <foo\+0x50>
  56:	a7 67 00 00 [ 	]*brctg	%r6,56 <foo\+0x56>
  5a:	ec 69 00 00 00 44 [ 	]*brxhg	%r6,%r9,5a <foo\+0x5a>
  60:	ec 69 00 00 00 45 [ 	]*brxlg	%r6,%r9,60 <foo\+0x60>
  66:	eb 69 5f ff 00 44 [ 	]*bxhg	%r6,%r9,4095\(%r5\)
  6c:	eb 69 5f ff 00 45 [ 	]*bxleg	%r6,%r9,4095\(%r5\)
  72:	b3 a5 00 69 [ 	]*cdgbr	%r6,%r9
  76:	b3 c5 00 69 [ 	]*cdgr	%r6,%r9
  7a:	eb 69 5f ff 00 3e [ 	]*cdsg	%r6,%r9,4095\(%r5\)
  80:	b3 a4 00 69 [ 	]*cegbr	%r6,%r9
  84:	b3 c4 00 69 [ 	]*cegr	%r6,%r9
  88:	e3 65 af ff 00 20 [ 	]*cg	%r6,4095\(%r5,%r10\)
  8e:	b3 a9 f0 69 [ 	]*cgdbr	%f6,15,%r9
  92:	b3 c9 90 65 [ 	]*cgdr	%f6,9,%r5
  96:	b3 a8 f0 69 [ 	]*cgebr	%f6,15,%r9
  9a:	b3 c8 90 65 [ 	]*cger	%f6,9,%r5
  9e:	e3 65 af ff 00 30 [ 	]*cgf	%r6,4095\(%r5,%r10\)
  a4:	b9 30 00 69 [ 	]*cgfr	%r6,%r9
  a8:	a7 6f 80 01 [ 	]*cghi	%r6,-32767
  ac:	b9 20 00 69 [ 	]*cgr	%r6,%r9
  b0:	b3 aa f0 69 [ 	]*cgxbr	%f6,15,%r9
  b4:	b3 ca 90 65 [ 	]*cgxr	%f6,9,%r5
  b8:	e3 65 af ff 00 21 [ 	]*clg	%r6,4095\(%r5,%r10\)
  be:	e3 65 af ff 00 31 [ 	]*clgf	%r6,4095\(%r5,%r10\)
  c4:	b9 31 00 69 [ 	]*clgfr	%r6,%r9
  c8:	b9 21 00 69 [ 	]*clgr	%r6,%r9
  cc:	eb 6a 5f ff 00 20 [ 	]*clmh	%r6,10,4095\(%r5\)
  d2:	eb 69 5f ff 00 30 [ 	]*csg	%r6,%r9,4095\(%r5\)
  d8:	e3 65 af ff 00 0e [ 	]*cvbg	%r6,4095\(%r5,%r10\)
  de:	e3 65 af ff 00 2e [ 	]*cvdg	%r6,4095\(%r5,%r10\)
  e4:	b3 a6 00 69 [ 	]*cxgbr	%r6,%r9
  e8:	b3 c6 00 69 [ 	]*cxgr	%r6,%r9
  ec:	e3 65 af ff 00 97 [ 	]*dl	%r6,4095\(%r5,%r10\)
  f2:	e3 65 af ff 00 87 [ 	]*dlg	%r6,4095\(%r5,%r10\)
  f8:	b9 87 00 69 [ 	]*dlgr	%r6,%r9
  fc:	b9 97 00 69 [ 	]*dlr	%r6,%r9
 100:	e3 65 af ff 00 0d [ 	]*dsg	%r6,4095\(%r5,%r10\)
 106:	e3 65 af ff 00 1d [ 	]*dsgf	%r6,4095\(%r5,%r10\)
 10c:	b9 1d 00 69 [ 	]*dsgfr	%r6,%r9
 110:	b9 0d 00 69 [ 	]*dsgr	%r6,%r9
 114:	b9 8d 00 69 [ 	]*epsw	%r6,%r9
 118:	b9 0e 00 69 [ 	]*eregg	%r6,%r9
 11c:	b9 9d 00 60 [ 	]*esea	%r6
 120:	eb 6a 5f ff 00 80 [ 	]*icmh	%r6,10,4095\(%r5\)
 126:	a5 60 ff ff [ 	]*iihh	%r6,65535
 12a:	a5 61 ff ff [ 	]*iihl	%r6,65535
 12e:	a5 62 ff ff [ 	]*iilh	%r6,65535
 132:	a5 63 ff ff [ 	]*iill	%r6,65535
 136:	c0 f4 00 00 00 00 [ 	]*jg	136 <foo\+0x136>
 13c:	c0 84 00 00 00 00 [ 	]*jge	13c <foo\+0x13c>
 142:	c0 24 00 00 00 00 [ 	]*jgh	142 <foo\+0x142>
 148:	c0 a4 00 00 00 00 [ 	]*jghe	148 <foo\+0x148>
 14e:	c0 44 00 00 00 00 [ 	]*jgl	14e <foo\+0x14e>
 154:	c0 c4 00 00 00 00 [ 	]*jgle	154 <foo\+0x154>
 15a:	c0 64 00 00 00 00 [ 	]*jglh	15a <foo\+0x15a>
 160:	c0 44 00 00 00 00 [ 	]*jgl	160 <foo\+0x160>
 166:	c0 74 00 00 00 00 [ 	]*jgne	166 <foo\+0x166>
 16c:	c0 d4 00 00 00 00 [ 	]*jgnh	16c <foo\+0x16c>
 172:	c0 54 00 00 00 00 [ 	]*jgnhe	172 <foo\+0x172>
 178:	c0 b4 00 00 00 00 [ 	]*jgnl	178 <foo\+0x178>
 17e:	c0 34 00 00 00 00 [ 	]*jgnle	17e <foo\+0x17e>
 184:	c0 94 00 00 00 00 [ 	]*jgnlh	184 <foo\+0x184>
 18a:	c0 b4 00 00 00 00 [ 	]*jgnl	18a <foo\+0x18a>
 190:	c0 e4 00 00 00 00 [ 	]*jgno	190 <foo\+0x190>
 196:	c0 d4 00 00 00 00 [ 	]*jgnh	196 <foo\+0x196>
 19c:	c0 74 00 00 00 00 [ 	]*jgne	19c <foo\+0x19c>
 1a2:	c0 14 00 00 00 00 [ 	]*jgo	1a2 <foo\+0x1a2>
 1a8:	c0 24 00 00 00 00 [ 	]*jgh	1a8 <foo\+0x1a8>
 1ae:	c0 84 00 00 00 00 [ 	]*jge	1ae <foo\+0x1ae>
 1b4:	c0 60 00 00 00 00 [ 	]*larl	%r6,1b4 <foo\+0x1b4>
 1ba:	b9 13 00 69 [ 	]*lcgfr	%r6,%r9
 1be:	b9 03 00 69 [ 	]*lcgr	%r6,%r9
 1c2:	eb 69 5f ff 00 2f [ 	]*lctlg	%r6,%r9,4095\(%r5\)
 1c8:	e3 65 af ff 00 04 [ 	]*lg	%r6,4095\(%r5,%r10\)
 1ce:	e3 65 af ff 00 14 [ 	]*lgf	%r6,4095\(%r5,%r10\)
 1d4:	b9 14 00 69 [ 	]*lgfr	%r6,%r9
 1d8:	e3 65 af ff 00 15 [ 	]*lgh	%r6,4095\(%r5,%r10\)
 1de:	a7 69 80 01 [ 	]*lghi	%r6,-32767
 1e2:	b9 04 00 69 [ 	]*lgr	%r6,%r9
 1e6:	e3 65 af ff 00 90 [ 	]*llgc	%r6,4095\(%r5,%r10\)
 1ec:	e3 65 af ff 00 16 [ 	]*llgf	%r6,4095\(%r5,%r10\)
 1f2:	b9 16 00 69 [ 	]*llgfr	%r6,%r9
 1f6:	e3 65 af ff 00 91 [ 	]*llgh	%r6,4095\(%r5,%r10\)
 1fc:	e3 65 af ff 00 17 [ 	]*llgt	%r6,4095\(%r5,%r10\)
 202:	b9 17 00 69 [ 	]*llgtr	%r6,%r9
 206:	a5 6c ff ff [ 	]*llihh	%r6,65535
 20a:	a5 6d ff ff [ 	]*llihl	%r6,65535
 20e:	a5 6e ff ff [ 	]*llilh	%r6,65535
 212:	a5 6f ff ff [ 	]*llill	%r6,65535
 216:	ef 69 5f ff af ff [ 	]*lmd	%r6,%r9,4095\(%r5\),4095\(%r10\)
 21c:	eb 69 5f ff 00 04 [ 	]*lmg	%r6,%r9,4095\(%r5\)
 222:	eb 69 5f ff 00 96 [ 	]*lmh	%r6,%r9,4095\(%r5\)
 228:	b9 11 00 69 [ 	]*lngfr	%r6,%r9
 22c:	b9 01 00 69 [ 	]*lngr	%r6,%r9
 230:	b9 10 00 69 [ 	]*lpgfr	%r6,%r9
 234:	b9 00 00 69 [ 	]*lpgr	%r6,%r9
 238:	e3 65 af ff 00 8f [ 	]*lpq	%r6,4095\(%r5,%r10\)
 23e:	b2 b2 5f ff [ 	]*lpswe	4095\(%r5\)
 242:	e3 65 af ff 00 03 [ 	]*lrag	%r6,4095\(%r5,%r10\)
 248:	e3 65 af ff 00 1e [ 	]*lrv	%r6,4095\(%r5,%r10\)
 24e:	e3 65 af ff 00 0f [ 	]*lrvg	%r6,4095\(%r5,%r10\)
 254:	b9 0f 00 69 [ 	]*lrvgr	%r6,%r9
 258:	e3 65 af ff 00 1f [ 	]*lrvh	%r6,4095\(%r5,%r10\)
 25e:	b9 1f 00 69 [ 	]*lrvr	%r6,%r9
 262:	b9 12 00 69 [ 	]*ltgfr	%r6,%r9
 266:	b9 02 00 69 [ 	]*ltgr	%r6,%r9
 26a:	b9 05 00 69 [ 	]*lurag	%r6,%r9
 26e:	b3 75 00 60 [ 	]*lzdr	%r6
 272:	b3 74 00 60 [ 	]*lzer	%r6
 276:	b3 76 00 60 [ 	]*lzxr	%r6
 27a:	a7 6d 80 01 [ 	]*mghi	%r6,-32767
 27e:	e3 65 af ff 00 96 [ 	]*ml	%r6,4095\(%r5,%r10\)
 284:	e3 65 af ff 00 86 [ 	]*mlg	%r6,4095\(%r5,%r10\)
 28a:	b9 86 00 69 [ 	]*mlgr	%r6,%r9
 28e:	b9 96 00 69 [ 	]*mlr	%r6,%r9
 292:	e3 65 af ff 00 0c [ 	]*msg	%r6,4095\(%r5,%r10\)
 298:	e3 65 af ff 00 1c [ 	]*msgf	%r6,4095\(%r5,%r10\)
 29e:	b9 1c 00 69 [ 	]*msgfr	%r6,%r9
 2a2:	b9 0c 00 69 [ 	]*msgr	%r6,%r9
 2a6:	eb 69 5f ff 00 8e [ 	]*mvclu	%r6,%r9,4095\(%r5\)
 2ac:	e3 65 af ff 00 80 [ 	]*ng	%r6,4095\(%r5,%r10\)
 2b2:	b9 80 00 69 [ 	]*ngr	%r6,%r9
 2b6:	a5 64 ff ff [ 	]*nihh	%r6,65535
 2ba:	a5 65 ff ff [ 	]*nihl	%r6,65535
 2be:	a5 66 ff ff [ 	]*nilh	%r6,65535
 2c2:	a5 67 ff ff [ 	]*nill	%r6,65535
 2c6:	e3 65 af ff 00 81 [ 	]*og	%r6,4095\(%r5,%r10\)
 2cc:	b9 81 00 69 [ 	]*ogr	%r6,%r9
 2d0:	a5 68 ff ff [ 	]*oihh	%r6,65535
 2d4:	a5 69 ff ff [ 	]*oihl	%r6,65535
 2d8:	a5 6a ff ff [ 	]*oilh	%r6,65535
 2dc:	a5 6b ff ff [ 	]*oill	%r6,65535
 2e0:	e9 ff 5f ff af ff [ 	]*pka	4095\(256,%r5\),4095\(%r10\)
 2e6:	e1 ff 5f ff af ff [ 	]*pku	4095\(256,%r5\),4095\(%r10\)
 2ec:	eb 69 5f ff 00 1d [ 	]*rll	%r6,%r9,4095\(%r5\)
 2f2:	eb 69 5f ff 00 1c [ 	]*rllg	%r6,%r9,4095\(%r5\)
 2f8:	01 0c [ 	]*sam24
 2fa:	01 0d [ 	]*sam31
 2fc:	01 0e [ 	]*sam64
 2fe:	e3 65 af ff 00 09 [ 	]*sg	%r6,4095\(%r5,%r10\)
 304:	e3 65 af ff 00 19 [ 	]*sgf	%r6,4095\(%r5,%r10\)
 30a:	b9 19 00 69 [ 	]*sgfr	%r6,%r9
 30e:	b9 09 00 69 [ 	]*sgr	%r6,%r9
 312:	eb 69 5f ff 00 0b [ 	]*slag	%r6,%r9,4095\(%r5\)
 318:	e3 65 af ff 00 99 [ 	]*slb	%r6,4095\(%r5,%r10\)
 31e:	e3 65 af ff 00 89 [ 	]*slbg	%r6,4095\(%r5,%r10\)
 324:	b9 89 00 69 [ 	]*slbgr	%r6,%r9
 328:	b9 99 00 69 [ 	]*slbr	%r6,%r9
 32c:	e3 65 af ff 00 0b [ 	]*slg	%r6,4095\(%r5,%r10\)
 332:	e3 65 af ff 00 1b [ 	]*slgf	%r6,4095\(%r5,%r10\)
 338:	b9 1b 00 69 [ 	]*slgfr	%r6,%r9
 33c:	b9 0b 00 69 [ 	]*slgr	%r6,%r9
 340:	eb 69 5f ff 00 0d [ 	]*sllg	%r6,%r9,4095\(%r5\)
 346:	eb 69 5f ff 00 0a [ 	]*srag	%r6,%r9,4095\(%r5\)
 34c:	eb 69 5f ff 00 0c [ 	]*srlg	%r6,%r9,4095\(%r5\)
 352:	b2 78 5f ff [ 	]*stcke	4095\(%r5\)
 356:	eb 6a 5f ff 00 2c [ 	]*stcmh	%r6,10,4095\(%r5\)
 35c:	eb 69 5f ff 00 25 [ 	]*stctg	%r6,%r9,4095\(%r5\)
 362:	b2 b1 5f ff [ 	]*stfl	4095\(%r5\)
 366:	e3 65 af ff 00 24 [ 	]*stg	%r6,4095\(%r5,%r10\)
 36c:	eb 69 5f ff 00 24 [ 	]*stmg	%r6,%r9,4095\(%r5\)
 372:	eb 69 5f ff 00 26 [ 	]*stmh	%r6,%r9,4095\(%r5\)
 378:	e3 65 af ff 00 8e [ 	]*stpq	%r6,4095\(%r5,%r10\)
 37e:	e5 00 5f ff 9f ff [ 	]*lasp	4095\(%r5\),4095\(%r9\)
 384:	e3 65 af ff 00 3e [ 	]*strv	%r6,4095\(%r5,%r10\)
 38a:	e3 65 af ff 00 2f [ 	]*strvg	%r6,4095\(%r5,%r10\)
 390:	e3 65 af ff 00 3f [ 	]*strvh	%r6,4095\(%r5,%r10\)
 396:	b9 25 00 69 [ 	]*sturg	%r6,%r9
 39a:	01 0b [ 	]*tam
 39c:	b3 51 f0 69 [ 	]*tbdr	%f6,15,%f9
 3a0:	b3 50 f0 69 [ 	]*tbedr	%f6,15,%f9
 3a4:	b3 58 00 69 [ 	]*thder	%r6,%r9
 3a8:	b3 59 00 69 [ 	]*thdr	%r6,%r9
 3ac:	a7 62 ff ff [ 	]*tmhh	%r6,65535
 3b0:	a7 63 ff ff [ 	]*tmhl	%r6,65535
 3b4:	a7 60 ff ff [ 	]*tmh	%r6,65535
 3b8:	a7 61 ff ff [ 	]*tml	%r6,65535
 3bc:	eb 69 5f ff 00 0f [ 	]*tracg	%r6,%r9,4095\(%r5\)
 3c2:	b2 a5 00 69 [ 	]*tre	%r6,%r9
 3c6:	b9 93 00 69 [ 	]*troo	%r6,%r9
 3ca:	b9 92 00 69 [ 	]*trot	%r6,%r9
 3ce:	b9 91 00 69 [ 	]*trto	%r6,%r9
 3d2:	b9 90 00 69 [ 	]*trtt	%r6,%r9
 3d6:	ea ff 5f ff af ff [ 	]*unpka	4095\(256,%r5\),4095\(%r10\)
 3dc:	e2 ff 5f ff af ff [ 	]*unpku	4095\(256,%r5\),4095\(%r10\)
 3e2:	e3 65 af ff 00 82 [ 	]*xg	%r6,4095\(%r5,%r10\)
 3e8:	b9 82 00 69 [ 	]*xgr	%r6,%r9
