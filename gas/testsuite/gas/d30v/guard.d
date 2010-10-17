#objdump: -dr
#name: D30V guarded execution test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

0+0000 <.text>:
   0:	08001083 88001083 	add.s	r1, r2, r3	->	add.s	r1, r2, r3
   8:	18001083 a8001083 	add.s/tx	r1, r2, r3	->	add.s/fx	r1, r2, r3
  10:	38001083 c8001083 	add.s/xt	r1, r2, r3	->	add.s/xf	r1, r2, r3
  18:	58001083 e8001083 	add.s/tt	r1, r2, r3	->	add.s/tf	r1, r2, r3
  20:	08001083 88001083 	add.s	r1, r2, r3	->	add.s	r1, r2, r3
  28:	18001083 a8001083 	add.s/tx	r1, r2, r3	->	add.s/fx	r1, r2, r3
  30:	38001083 c8001083 	add.s/xt	r1, r2, r3	->	add.s/xf	r1, r2, r3
  38:	58001083 e8001083 	add.s/tt	r1, r2, r3	->	add.s/tf	r1, r2, r3
