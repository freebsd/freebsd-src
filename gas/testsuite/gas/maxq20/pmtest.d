#objdump: -dw
#name: MaC supoprt check

.*: +file format .*

Disassembly of section .text:
0+000 <.text>:
   0:	05 13 [ 	]*MOVE  13h, #05h
   2:	e9 53 [ 	]*MOVE  53h, #e9h
   4:	09 e3 [ 	]*MOVE  63h, A\[0\]
   6:	12 14 [ 	]*MOVE  14h, #12h
   8:	12 44 [ 	]*MOVE  44h, #12h
   a:	00 2b [ 	]*MOVE  PFX\[2\], #00h
   c:	09 84 [ 	]*MOVE  04h, A\[0\]
   e:	7b 15 [ 	]*MOVE  15h, #7bh
  10:	13 25 [ 	]*MOVE  25h, #13h
  12:	d9 e5 [ 	]*MOVE  65h, A\[13\]
  14:	13 15 [ 	]*MOVE  15h, #13h
  16:	13 a5 [ 	]*MOVE  25h, 13h
  18:	12 13 [ 	]*MOVE  13h, #12h
  1a:	12 2b [ 	]*MOVE  PFX\[2\], #12h
  1c:	34 59 [ 	]*MOVE  A\[5\], #34h
  1e:	04 2b [ 	]*MOVE  PFX\[2\], #04h
  20:	d2 79 [ 	]*MOVE  A\[7\], #d2h
	...
