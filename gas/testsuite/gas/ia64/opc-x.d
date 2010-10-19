#as: -xnone
#objdump: -d
#name: ia64 opc-x

.*: +file format .*

Disassembly of section .text:

0+000 <_start>:
   0:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
   6:	00 00 00 00 00 00 	            break\.x 0x0
   c:	00 00 00 00 
  10:	04 00 00 00 01 c0 	\[MLX\]       nop\.m 0x0
  16:	ff ff ff ff 7f e0 	            break\.x 0x3fffffffffffffff
  1c:	ff ff 01 08 
  20:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
  26:	00 00 00 00 00 00 	            nop\.x 0x0
  2c:	00 00 04 00 
  30:	04 00 00 00 01 c0 	\[MLX\]       nop\.m 0x0
  36:	ff ff ff ff 7f e0 	            nop\.x 0x3fffffffffffffff
  3c:	ff ff 05 08 
  40:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
  46:	00 00 00 00 00 80 	            movl r4=0x0
  4c:	00 00 00 60 
  50:	04 00 00 00 01 c0 	\[MLX\]       nop\.m 0x0
  56:	ff ff ff ff 7f 80 	            movl r4=0xffffffffffffffff
  5c:	f0 f7 ff 6f 
  60:	04 00 00 00 01 80 	\[MLX\]       nop\.m 0x0
  66:	90 78 56 34 12 80 	            movl r4=0x1234567890abcdef
  6c:	f0 76 6d 66 
  70:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
  76:	00 00 00 00 00 00 	            hint\.x 0x0
  7c:	00 00 06 00 
  80:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
  86:	00 00 00 00 00 00 	            hint\.x 0x0
  8c:	00 00 06 00 
  90:	05 00 00 00 01 c0 	\[MLX\]       nop\.m 0x0
  96:	ff ff ff ff 7f e0 	            hint\.x 0x3fffffffffffffff;;
  9c:	ff ff 07 08 
