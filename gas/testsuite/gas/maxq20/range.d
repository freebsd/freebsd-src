#objdump:-dw
#name: limit checks for maxq immediate data

.*: +file format .*

Disassembly of section .text:
0+000 <.text>:
   0:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
   2:	ff 09 [ 	]*MOVE  A\[0\], #ffh
   4:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
   6:	ff 08 [ 	]*MOVE  AP, #ffh
   8:	01 09 [ 	]*MOVE  A\[0\], #01h
   a:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
   c:	83 08 [ 	]*MOVE  AP, #83h
   e:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  10:	82 08 [ 	]*MOVE  AP, #82h
  12:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  14:	81 08 [ 	]*MOVE  AP, #81h
  16:	7d 09 [ 	]*MOVE  A\[0\], #7dh
  18:	7e 09 [ 	]*MOVE  A\[0\], #7eh
  1a:	80 09 [ 	]*MOVE  A\[0\], #80h
  1c:	fe 09 [ 	]*MOVE  A\[0\], #feh
  1e:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  20:	ff 0d [ 	]*MOVE  @\+\+SP, #ffh
  22:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  24:	82 0d [ 	]*MOVE  @\+\+SP, #82h
  26:	fe 0d [ 	]*MOVE  @\+\+SP, #feh
  28:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  2a:	81 0d [ 	]*MOVE  @\+\+SP, #81h
  2c:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  2e:	80 0d [ 	]*MOVE  @\+\+SP, #80h
  30:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  32:	ff 4a [ 	]*ADD  #ffh
  34:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  36:	81 4a [ 	]*ADD  #81h
  38:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  3a:	7f 4a [ 	]*ADD  #7fh
  3c:	7f 4a [ 	]*ADD  #7fh
  3e:	80 4a [ 	]*ADD  #80h
  40:	81 4a [ 	]*ADD  #81h
  42:	fe 4a [ 	]*ADD  #feh
  44:	ff 4a [ 	]*ADD  #ffh
  46:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  48:	02 4a [ 	]*ADD  #02h
  4a:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  4c:	81 4a [ 	]*ADD  #81h
  4e:	ff 0b [ 	]*MOVE  PFX\[0\], #ffh
  50:	7f 4a [ 	]*ADD  #7fh
	...
