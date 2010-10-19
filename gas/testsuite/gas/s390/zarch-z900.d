#name: s390x opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
.*:	e3 95 af ff 00 08 [ 	]*ag	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 18 [ 	]*agf	%r9,4095\(%r5,%r10\)
.*:	b9 18 00 96 [ 	]*agfr	%r9,%r6
.*:	a7 9b 80 01 [ 	]*aghi	%r9,-32767
.*:	b9 08 00 96 [ 	]*agr	%r9,%r6
.*:	e3 95 af ff 00 88 [ 	]*alcg	%r9,4095\(%r5,%r10\)
.*:	b9 88 00 96 [ 	]*alcgr	%r9,%r6
.*:	e3 95 af ff 00 0a [ 	]*alg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 1a [ 	]*algf	%r9,4095\(%r5,%r10\)
.*:	b9 1a 00 96 [ 	]*algfr	%r9,%r6
.*:	b9 0a 00 96 [ 	]*algr	%r9,%r6
.*:	e3 95 af ff 00 46 [ 	]*bctg	%r9,4095\(%r5,%r10\)
.*:	b9 46 00 96 [ 	]*bctgr	%r9,%r6
.*:	a7 97 00 00 [	 ]*brctg	%r9,40 \<foo\+0x40\>
.*:	ec 96 00 00 00 44 [ 	]*brxhg	%r9,%r6,44 <foo\+0x44>
.*:	ec 96 00 00 00 45 [ 	]*brxlg	%r9,%r6,4a <foo\+0x4a>
.*:	eb 96 5f ff 00 44 [ 	]*bxhg	%r9,%r6,4095\(%r5\)
.*:	eb 96 5f ff 00 45 [ 	]*bxleg	%r9,%r6,4095\(%r5\)
.*:	b3 a5 00 96 [ 	]*cdgbr	%r9,%r6
.*:	b3 c5 00 96 [ 	]*cdgr	%r9,%r6
.*:	eb 96 5f ff 00 3e [ 	]*cdsg	%r9,%r6,4095\(%r5\)
.*:	b3 a4 00 96 [ 	]*cegbr	%r9,%r6
.*:	b3 c4 00 96 [ 	]*cegr	%r9,%r6
.*:	b3 b9 90 65 [	 ]*cfdr	%f6,9,%r5
.*:	b3 b8 90 65 [	 ]*cfer	%f6,9,%r5
.*:	b3 ba 90 65 [	 ]*cfxr	%f6,9,%r5
.*:	e3 95 af ff 00 20 [ 	]*cg	%r9,4095\(%r5,%r10\)
.*:	b3 a9 f0 65 [ 	]*cgdbr	%f6,15,%r5
.*:	b3 c9 f0 65 [ 	]*cgdr	%f6,15,%r5
.*:	b3 a8 f0 65 [ 	]*cgebr	%f6,15,%r5
.*:	b3 c8 f0 65 [ 	]*cger	%f6,15,%r5
.*:	e3 95 af ff 00 30 [ 	]*cgf	%r9,4095\(%r5,%r10\)
.*:	b9 30 00 96 [ 	]*cgfr	%r9,%r6
.*:	a7 9f 80 01 [ 	]*cghi	%r9,-32767
.*:	b9 20 00 96 [ 	]*cgr	%r9,%r6
.*:	b3 aa f0 65 [ 	]*cgxbr	%f6,15,%r5
.*:	b3 ca f0 65 [ 	]*cgxr	%f6,15,%r5
.*:	e3 95 af ff 00 21 [ 	]*clg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 31 [ 	]*clgf	%r9,4095\(%r5,%r10\)
.*:	b9 31 00 96 [ 	]*clgfr	%r9,%r6
.*:	b9 21 00 96 [ 	]*clgr	%r9,%r6
.*:	eb 9a 5f ff 00 20 [ 	]*clmh	%r9,10,4095\(%r5\)
.*:	eb 96 5f ff 00 30 [ 	]*csg	%r9,%r6,4095\(%r5\)
.*:	e3 95 af ff 00 0e [ 	]*cvbg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 2e [ 	]*cvdg	%r9,4095\(%r5,%r10\)
.*:	b3 a6 00 96 [ 	]*cxgbr	%r9,%r6
.*:	b3 c6 00 96 [ 	]*cxgr	%r9,%r6
.*:	e3 95 af ff 00 87 [ 	]*dlg	%r9,4095\(%r5,%r10\)
.*:	b9 87 00 96 [ 	]*dlgr	%r9,%r6
.*:	e3 95 af ff 00 0d [ 	]*dsg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 1d [ 	]*dsgf	%r9,4095\(%r5,%r10\)
.*:	b9 1d 00 96 [ 	]*dsgfr	%r9,%r6
.*:	b9 0d 00 96 [ 	]*dsgr	%r9,%r6
.*:	b9 0e 00 96 [ 	]*eregg	%r9,%r6
.*:	b9 9d 00 90 [ 	]*esea	%r9
.*:	eb 9a 5f ff 00 80 [ 	]*icmh	%r9,10,4095\(%r5\)
.*:	a5 90 ff ff [ 	]*iihh	%r9,65535
.*:	a5 91 ff ff [ 	]*iihl	%r9,65535
.*:	a5 92 ff ff [ 	]*iilh	%r9,65535
.*:	a5 93 ff ff [ 	]*iill	%r9,65535
.*:	b9 13 00 96 [ 	]*lcgfr	%r9,%r6
.*:	b9 03 00 96 [ 	]*lcgr	%r9,%r6
.*:	eb 96 5f ff 00 2f [ 	]*lctlg	%c9,%c6,4095\(%r5\)
.*:	e3 95 af ff 00 04 [ 	]*lg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 14 [ 	]*lgf	%r9,4095\(%r5,%r10\)
.*:	b9 14 00 96 [ 	]*lgfr	%r9,%r6
.*:	e3 95 af ff 00 15 [ 	]*lgh	%r9,4095\(%r5,%r10\)
.*:	a7 99 80 01 [ 	]*lghi	%r9,-32767
.*:	b9 04 00 96 [ 	]*lgr	%r9,%r6
.*:	e3 95 af ff 00 90 [ 	]*llgc	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 16 [ 	]*llgf	%r9,4095\(%r5,%r10\)
.*:	b9 16 00 96 [ 	]*llgfr	%r9,%r6
.*:	e3 95 af ff 00 91 [ 	]*llgh	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 17 [ 	]*llgt	%r9,4095\(%r5,%r10\)
.*:	b9 17 00 96 [ 	]*llgtr	%r9,%r6
.*:	a5 9c ff ff [ 	]*llihh	%r9,65535
.*:	a5 9d ff ff [ 	]*llihl	%r9,65535
.*:	a5 9e ff ff [ 	]*llilh	%r9,65535
.*:	a5 9f ff ff [ 	]*llill	%r9,65535
.*:	ef 96 5f ff af ff [ 	]*lmd	%r9,%r6,4095\(%r5\),4095\(%r10\)
.*:	eb 96 5f ff 00 04 [ 	]*lmg	%r9,%r6,4095\(%r5\)
.*:	eb 96 5f ff 00 96 [ 	]*lmh	%r9,%r6,4095\(%r5\)
.*:	b9 11 00 96 [ 	]*lngfr	%r9,%r6
.*:	b9 01 00 96 [ 	]*lngr	%r9,%r6
.*:	b9 10 00 96 [ 	]*lpgfr	%r9,%r6
.*:	b9 00 00 96 [ 	]*lpgr	%r9,%r6
.*:	e3 95 af ff 00 8f [ 	]*lpq	%r9,4095\(%r5,%r10\)
.*:	b2 b2 5f ff [ 	]*lpswe	4095\(%r5\)
.*:	e3 95 af ff 00 03 [ 	]*lrag	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 0f [ 	]*lrvg	%r9,4095\(%r5,%r10\)
.*:	b9 0f 00 96 [ 	]*lrvgr	%r9,%r6
.*:	b9 12 00 96 [ 	]*ltgfr	%r9,%r6
.*:	b9 02 00 96 [ 	]*ltgr	%r9,%r6
.*:	b9 05 00 96 [ 	]*lurag	%r9,%r6
.*:	a7 9d 80 01 [ 	]*mghi	%r9,-32767
.*:	e3 95 af ff 00 86 [ 	]*mlg	%r9,4095\(%r5,%r10\)
.*:	b9 86 00 96 [ 	]*mlgr	%r9,%r6
.*:	e3 95 af ff 00 0c [ 	]*msg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 1c [ 	]*msgf	%r9,4095\(%r5,%r10\)
.*:	b9 1c 00 96 [ 	]*msgfr	%r9,%r6
.*:	b9 0c 00 96 [ 	]*msgr	%r9,%r6
.*:	e3 95 af ff 00 80 [ 	]*ng	%r9,4095\(%r5,%r10\)
.*:	b9 80 00 96 [ 	]*ngr	%r9,%r6
.*:	a5 94 ff ff [ 	]*nihh	%r9,65535
.*:	a5 95 ff ff [ 	]*nihl	%r9,65535
.*:	a5 96 ff ff [ 	]*nilh	%r9,65535
.*:	a5 97 ff ff [ 	]*nill	%r9,65535
.*:	e3 95 af ff 00 81 [ 	]*og	%r9,4095\(%r5,%r10\)
.*:	b9 81 00 96 [ 	]*ogr	%r9,%r6
.*:	a5 98 ff ff [ 	]*oihh	%r9,65535
.*:	a5 99 ff ff [ 	]*oihl	%r9,65535
.*:	a5 9a ff ff [ 	]*oilh	%r9,65535
.*:	a5 9b ff ff [ 	]*oill	%r9,65535
.*:	eb 96 5f ff 00 1c [ 	]*rllg	%r9,%r6,4095\(%r5\)
.*:	01 0e [ 	]*sam64
.*:	e3 95 af ff 00 09 [ 	]*sg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 19 [ 	]*sgf	%r9,4095\(%r5,%r10\)
.*:	b9 19 00 96 [ 	]*sgfr	%r9,%r6
.*:	b9 09 00 96 [ 	]*sgr	%r9,%r6
.*:	eb 96 5f ff 00 0b [ 	]*slag	%r9,%r6,4095\(%r5\)
.*:	e3 95 af ff 00 89 [ 	]*slbg	%r9,4095\(%r5,%r10\)
.*:	b9 89 00 96 [ 	]*slbgr	%r9,%r6
.*:	e3 95 af ff 00 0b [ 	]*slg	%r9,4095\(%r5,%r10\)
.*:	e3 95 af ff 00 1b [ 	]*slgf	%r9,4095\(%r5,%r10\)
.*:	b9 1b 00 96 [ 	]*slgfr	%r9,%r6
.*:	b9 0b 00 96 [ 	]*slgr	%r9,%r6
.*:	eb 96 5f ff 00 0d [ 	]*sllg	%r9,%r6,4095\(%r5\)
.*:	eb 96 5f ff 00 0a [ 	]*srag	%r9,%r6,4095\(%r5\)
.*:	eb 96 5f ff 00 0c [ 	]*srlg	%r9,%r6,4095\(%r5\)
.*:	eb 9a 5f ff 00 2c [ 	]*stcmh	%r9,10,4095\(%r5\)
.*:	eb 96 5f ff 00 25 [ 	]*stctg	%c9,%c6,4095\(%r5\)
.*:	e3 95 af ff 00 24 [ 	]*stg	%r9,4095\(%r5,%r10\)
.*:	eb 96 5f ff 00 24 [ 	]*stmg	%r9,%r6,4095\(%r5\)
.*:	eb 96 5f ff 00 26 [ 	]*stmh	%r9,%r6,4095\(%r5\)
.*:	e3 95 af ff 00 8e [ 	]*stpq	%r9,4095\(%r5,%r10\)
.*:	e5 00 5f ff 9f ff [ 	]*lasp	4095\(%r5\),4095\(%r9\)
.*:	e3 95 af ff 00 2f [ 	]*strvg	%r9,4095\(%r5,%r10\)
.*:	b9 25 00 96 [ 	]*sturg	%r9,%r6
.*:	a7 92 ff ff [ 	]*tmhh	%r9,65535
.*:	a7 93 ff ff [ 	]*tmhl	%r9,65535
.*:	eb 96 5f ff 00 0f [ 	]*tracg	%r9,%r6,4095\(%r5\)
.*:	e3 95 af ff 00 82 [ 	]*xg	%r9,4095\(%r5,%r10\)
.*:	b9 82 00 96 [ 	]*xgr	%r9,%r6
.*:	07 07 [	 ]*bcr	0,%r7