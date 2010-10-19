#objdump: -dw
#name: Jump immediate operations

.*: +file format .*

Disassembly of section .text:
0+0000 <_main>:
   0:	03 2c [ 	]*JUMP C , #03h
   2:	03 4c [ 	]*JUMP S , #03h
   4:	0f 0b [ 	]*MOVE  PFX\[0\], #0fh
   6:	ff 1c [ 	]*JUMP Z , #ffh
   8:	03 5c [ 	]*JUMP NZ , #03h
   a:	03 2c [ 	]*JUMP C , #03h
   c:	03 4c [ 	]*JUMP S , #03h
   e:	0f 0b [ 	]*MOVE  PFX\[0\], #0fh
  10:	ff 1c [ 	]*JUMP Z , #ffh
  12:	03 5c [ 	]*JUMP NZ , #03h
  14:	00 0b [ 	]*MOVE  PFX\[0\], #00h
  16:	03 2c [ 	]*JUMP C , #03h
  18:	00 0b [ 	]*MOVE  PFX\[0\], #00h
  1a:	03 4c [ 	]*JUMP S , #03h
  1c:	00 0b [ 	]*MOVE  PFX\[0\], #00h
  1e:	0f 0b [ 	]*MOVE  PFX\[0\], #0fh
  20:	ff 1c [ 	]*JUMP Z , #ffh
  22:	00 0b [ 	]*MOVE  PFX\[0\], #00h
  24:	03 5c [ 	]*JUMP NZ , #03h
	...
