#name: s390x opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
.*:	b3 70 00 62 [	 ]*lpdfr	%f6,%f2
.*:	b3 71 00 62 [	 ]*lndfr	%f6,%f2
.*:	b3 72 10 62 [	 ]*cpsdr	%f6,%f1,%f2
.*:	b3 73 00 62 [	 ]*lcdfr	%f6,%f2
.*:	b3 c1 00 62 [	 ]*ldgr	%f6,%r2
.*:	b3 cd 00 26 [	 ]*lgdr	%r2,%f6
.*:	b3 d2 40 62 [	 ]*adtr	%f6,%f2,%f4
.*:	b3 da 40 62 [	 ]*axtr	%f6,%f2,%f4
.*:	b3 e4 00 62 [	 ]*cdtr	%f6,%f2
.*:	b3 ec 00 62 [	 ]*cxtr	%f6,%f2
.*:	b3 e0 00 62 [	 ]*kdtr	%f6,%f2
.*:	b3 e8 00 62 [	 ]*kxtr	%f6,%f2
.*:	b3 f4 00 62 [	 ]*cedtr	%f6,%f2
.*:	b3 fc 00 62 [	 ]*cextr	%f6,%f2
.*:	b3 f1 00 62 [	 ]*cdgtr	%f6,%r2
.*:	b3 f9 00 62 [	 ]*cxgtr	%f6,%r2
.*:	b3 f3 00 62 [	 ]*cdstr	%f6,%r2
.*:	b3 fb 00 62 [	 ]*cxstr	%f6,%r2
.*:	b3 f2 00 62 [	 ]*cdutr	%f6,%r2
.*:	b3 fa 00 62 [	 ]*cxutr	%f6,%r2
.*:	b3 e1 10 26 [	 ]*cgdtr	%r2,1,%f6
.*:	b3 e9 10 26 [	 ]*cgxtr	%r2,1,%f6
.*:	b3 e3 00 26 [	 ]*csdtr	%r2,%f6
.*:	b3 eb 00 26 [	 ]*csxtr	%r2,%f6
.*:	b3 e2 00 26 [	 ]*cudtr	%r2,%f6
.*:	b3 ea 00 26 [	 ]*cuxtr	%r2,%f6
.*:	b3 d1 40 62 [	 ]*ddtr	%f6,%f2,%f4
.*:	b3 d9 40 62 [	 ]*dxtr	%f6,%f2,%f4
.*:	b3 e5 00 26 [	 ]*eedtr	%r2,%f6
.*:	b3 ed 00 26 [	 ]*eextr	%r2,%f6
.*:	b3 e7 00 26 [	 ]*esdtr	%r2,%f6
.*:	b3 ef 00 26 [	 ]*esxtr	%r2,%f6
.*:	b3 f6 20 64 [	 ]*iedtr	%f6,%f2,%r4
.*:	b3 fe 20 64 [	 ]*iextr	%f6,%f2,%r4
.*:	b3 d6 00 62 [	 ]*ltdtr	%f6,%f2
.*:	b3 de 00 62 [	 ]*ltxtr	%f6,%f2
.*:	b3 d7 13 62 [	 ]*fidtr	%f6,1,%f2,3
.*:	b3 df 13 62 [	 ]*fixtr	%f6,1,%f2,3
.*:	b2 bd 10 03 [	 ]*lfas	3\(%r1\)
.*:	b3 d4 01 62 [	 ]*ldetr	%f6,%f2,1
.*:	b3 dc 01 62 [	 ]*lxdtr	%f6,%f2,1
.*:	b3 d5 13 62 [	 ]*ledtr	%f6,1,%f2,3
.*:	b3 dd 13 62 [	 ]*ldxtr	%f6,1,%f2,3
.*:	b3 d0 40 62 [	 ]*mdtr	%f6,%f2,%f4
.*:	b3 d8 40 62 [	 ]*mxtr	%f6,%f2,%f4
.*:	b3 f5 21 64 [	 ]*qadtr	%f6,%f2,%f4,1
.*:	b3 fd 21 64 [	 ]*qaxtr	%f6,%f2,%f4,1
.*:	b3 f7 21 64 [	 ]*rrdtr	%f6,%f2,%f4,1
.*:	b3 ff 21 64 [	 ]*rrxtr	%f6,%f2,%f4,1
.*:	b2 b9 10 03 [	 ]*srnmt	3\(%r1\)
.*:	b3 85 00 20 [	 ]*sfasr	%r2
.*:	ed 21 40 03 60 40 [	 ]*sldt	%f6,%f2,3\(%r1,%r4\)
.*:	ed 21 40 03 60 48 [	 ]*slxt	%f6,%f2,3\(%r1,%r4\)
.*:	ed 21 40 03 60 41 [	 ]*srdt	%f6,%f2,3\(%r1,%r4\)
.*:	ed 21 40 03 60 49 [	 ]*srxt	%f6,%f2,3\(%r1,%r4\)
.*:	b3 d3 40 62 [	 ]*sdtr	%f6,%f2,%f4
.*:	b3 db 40 62 [	 ]*sxtr	%f6,%f2,%f4
.*:	ed 61 20 03 00 50 [	 ]*tcet	%f6,3\(%r1,%r2\)
.*:	ed 61 20 03 00 54 [	 ]*tcdt	%f6,3\(%r1,%r2\)
.*:	ed 61 20 03 00 58 [	 ]*tcxt	%f6,3\(%r1,%r2\)
.*:	ed 61 20 03 00 51 [	 ]*tget	%f6,3\(%r1,%r2\)
.*:	ed 61 20 03 00 55 [	 ]*tgdt	%f6,3\(%r1,%r2\)
.*:	ed 61 20 03 00 59 [	 ]*tgxt	%f6,3\(%r1,%r2\)
.*:	01 0a [	 ]*pfpo
.*:	c8 31 10 0a 20 14 [	 ]*ectg	10\(%r1\),20\(%r2\),%r3
.*:	c8 32 10 0a 20 14 [	 ]*csst	10\(%r1\),20\(%r2\),%r3
# Expect 2 bytes of padding.
.*:	07 07 [	 ]*bcr	0,%r7
