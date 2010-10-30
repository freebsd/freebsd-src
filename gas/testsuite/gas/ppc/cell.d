#as: -mcell
#objdump: -dr -Mcell
#name: Cell tests


.*: +file format elf(32)?(64)?-powerpc.*


Disassembly of section \.text:

0000000000000000 <.text>:
   0:	7c 01 14 0e 	lvlx    v0,r1,r2
   4:	7c 00 14 0e 	lvlx    v0,0,r2
   8:	7c 01 16 0e 	lvlxl   v0,r1,r2
   c:	7c 00 16 0e 	lvlxl   v0,0,r2
  10:	7c 01 14 4e 	lvrx    v0,r1,r2
  14:	7c 00 14 4e 	lvrx    v0,0,r2
  18:	7c 01 16 4e 	lvrxl   v0,r1,r2
  1c:	7c 00 16 4e 	lvrxl   v0,0,r2
  20:	7c 01 15 0e 	stvlx   v0,r1,r2
  24:	7c 00 15 0e 	stvlx   v0,0,r2
  28:	7c 01 17 0e 	stvlxl  v0,r1,r2
  2c:	7c 00 17 0e 	stvlxl  v0,0,r2
  30:	7c 01 15 4e 	stvrx   v0,r1,r2
  34:	7c 00 15 4e 	stvrx   v0,0,r2
  38:	7c 01 17 4e 	stvrxl  v0,r1,r2
  3c:	7c 00 17 4e 	stvrxl  v0,0,r2
  40:	7c 00 0c 28 	ldbrx   r0,0,r1
  44:	7c 01 14 28 	ldbrx   r0,r1,r2
  48:	7c 00 0d 28 	stdbrx  r0,0,r1
  4c:	7c 01 15 28 	stdbrx  r0,r1,r2
