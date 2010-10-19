#as:
#objdump: -dr
#name: misc

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	00 12 00 00 	add R0,R1,R2
   4:	00 12 00 00 	add R0,R1,R2
   8:	00 23 10 00 	add R1,R2,R3
   c:	00 33 10 00 	add R1,R3,R3
  10:	00 56 40 00 	add R4,R5,R6
  14:	00 89 70 00 	add R7,R8,R9
  18:	00 bc a0 00 	add R10,R11,R12
  1c:	00 ef d0 00 	add R13,R14,R15
  20:	03 dc 00 01 	addui R12,R13,#\$1
  24:	03 fe 00 01 	addui R14,R15,#\$1
  28:	03 10 00 00 	addui R0,R1,#\$0
  2c:	03 10 ff ff 	addui R0,R1,#\$ffff
