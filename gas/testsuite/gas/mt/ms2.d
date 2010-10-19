#as: -march=ms2
#objdump: -dr
#name: ms2

.*: +file format .*

Disassembly of section .text:

00000000 <code>:
   0:	3e 10 00 05 	loop R1,1c <label>
   4:	3f 00 10 04 	loopi #\$10,1c <label>
   8:	83 ff ff ff 	dfbc #\$7,#\$7,#\$ffffffff,#\$ffffffff,#\$1,#\$1,#\$3f
   c:	87 ff ff 7f 	dwfb #\$7,#\$7,#\$ffffffff,#\$ffffffff,#\$1,#\$3f
  10:	8b ff ff ff 	fbwfb #\$7,#\$7,#\$ffffffff,#\$ffffffff,#\$1,#\$1,#\$3f
  14:	8f f0 ff ff 	dfbr #\$7,#\$7,R0,#\$7,#\$7,#\$7,#\$1,#\$3f
  18:	12 00 00 00 	nop
0000001c <label>:
  1c:	f0 00 00 00 	fbcbincs #\$0,#\$0,#\$0,#\$0,#\$0,#\$0,#\$0,#\$0,#\$0,#\$0
