#name: mcf-mov3q
#objdump: -d --architecture=m68k:5407
#as: -m5407

.*:     file format .*

Disassembly of section .text:

0+ <test_mov3q>:
   0:	a140           	mov3ql #-1,%d0
   2:	a349           	mov3ql #1,%a1
   4:	a552           	mov3ql #2,%a2@
   6:	a75b           	mov3ql #3,%a3@\+
   8:	a964           	mov3ql #4,%a4@-
   a:	ab6d 04d2      	mov3ql #5,%a5@\(1234\)
   e:	ad76 6803      	mov3ql #6,%fp@\(0+03,%d6:l\)
  12:	af78 1234      	mov3ql #7,1234 (<test_mov3q\+0x1234>|<.data\+0x1218>)
  16:	a179 1234 5678 	mov3ql #-1,12345678 (<test_mov3q\+0x12345678>|<.data\+0x1234565c>)
