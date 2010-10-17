#as: -64 -Av9
#objdump: -dr
#name: sparc64 rdpr

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	83 50 00 00 	rdpr  %tpc, %g1
   4:	85 50 40 00 	rdpr  %tnpc, %g2
   8:	87 50 80 00 	rdpr  %tstate, %g3
   c:	89 50 c0 00 	rdpr  %tt, %g4
  10:	8b 51 00 00 	rdpr  %tick, %g5
  14:	8d 51 40 00 	rdpr  %tba, %g6
  18:	8f 51 80 00 	rdpr  %pstate, %g7
  1c:	91 51 c0 00 	rdpr  %tl, %o0
  20:	93 52 00 00 	rdpr  %pil, %o1
  24:	95 52 40 00 	rdpr  %cwp, %o2
  28:	97 52 80 00 	rdpr  %cansave, %o3
  2c:	99 52 c0 00 	rdpr  %canrestore, %o4
  30:	9b 53 00 00 	rdpr  %cleanwin, %o5
  34:	9d 53 40 00 	rdpr  %otherwin, %sp
  38:	9f 53 80 00 	rdpr  %wstate, %o7
  3c:	a1 53 c0 00 	rdpr  %fq, %l0
  40:	a3 57 c0 00 	rdpr  %ver, %l1
