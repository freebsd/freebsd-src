#as: -64 -Av9
#objdump: -dr
#name: sparc64 wrpr

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	81 90 40 00 	wrpr  %g1, %tpc
   4:	83 90 80 00 	wrpr  %g2, %tnpc
   8:	85 90 c0 00 	wrpr  %g3, %tstate
   c:	87 91 00 00 	wrpr  %g4, %tt
  10:	89 91 40 00 	wrpr  %g5, %tick
  14:	8b 91 80 00 	wrpr  %g6, %tba
  18:	8d 91 c0 00 	wrpr  %g7, %pstate
  1c:	8f 92 00 00 	wrpr  %o0, %tl
  20:	91 92 40 00 	wrpr  %o1, %pil
  24:	93 92 80 00 	wrpr  %o2, %cwp
  28:	95 92 c0 00 	wrpr  %o3, %cansave
  2c:	97 93 00 00 	wrpr  %o4, %canrestore
  30:	99 93 40 00 	wrpr  %o5, %cleanwin
  34:	9b 93 80 00 	wrpr  %sp, %otherwin
  38:	9d 93 c0 00 	wrpr  %o7, %wstate
  3c:	a1 94 00 00 	wrpr  %l0, %gl
