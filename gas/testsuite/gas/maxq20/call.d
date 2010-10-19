#objdump: -dw
#name: call operations

.*: +file format .*

Disassembly of section .text:
0+000 <foo>:
   0:	02 3d [ 	]*CALL  #02h
   2:	04 0b [ 	]*MOVE  PFX\[0\], #04h
   4:	28 3d [ 	]*CALL  #28h

0+006 <SmallCall>:
   6:	0d 8c [ 	]*RET 
   8:	0d ac [ 	]*RET C
   a:	0d 9c [ 	]*RET Z
   c:	0d dc [ 	]*RET NZ
   e:	0d cc [ 	]*RET S
  10:	8d 8c [ 	]*RETI 
  12:	8d ac [ 	]*RETI C
  14:	8d 9c [ 	]*RETI Z
  16:	8d dc [ 	]*RETI NZ
  18:	8d cc [ 	]*RETI S
  1a:	10 7d [ 	]*MOVE  LC\[1\], #10h

0+01c <LoopTop>:
  1c:	ff 3d [ 	]*CALL  #ffh
  1e:	fe 5d [ 	]*DJNZ  LC\[1\], #feh
  20:	10 7d [ 	]*MOVE  LC\[1\], #10h

0+022 <LoopTop1>:
  22:	ff 3d [ 	]*CALL  #ffh
	...
 424:	00 0b [ 	]*MOVE  PFX\[0\], #00h
 426:	1c 5d [ 	]*DJNZ  LC\[1\], #1ch

0+428 <LongCall>:
 428:	8d 8c [ 	]*RETI 
 42a:	8d ac [ 	]*RETI C
 42c:	8d 9c [ 	]*RETI Z
 42e:	8d dc [ 	]*RETI NZ
 430:	8d cc [ 	]*RETI S
	...
