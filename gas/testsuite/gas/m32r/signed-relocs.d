#as: -J
#objdump: -dr
#name: Signed relocs

.*: +file format .*

Disassembly of section .text:

0+0000 <relocs>:
   0:	d0 c0 87 65 	seth r0,#0x8765
   4:	80 a0 43 21 	add3 r0,r0,#17185
   8:	d0 c0 87 65 	seth r0,#0x8765
   c:	80 a0 43 21 	add3 r0,r0,#17185
  10:	d0 c0 12 35 	seth r0,#0x1235
  14:	80 a0 ff ff 	add3 r0,r0,#-1
  18:	d0 c0 12 35 	seth r0,#0x1235
  1c:	80 a0 ff ff 	add3 r0,r0,#-1
  20:	d0 c0 87 65 	seth r0,#0x8765
  24:	80 e0 43 21 	or3 r0,r0,#0x4321
  28:	d0 c0 87 65 	seth r0,#0x8765
  2c:	80 e0 43 21 	or3 r0,r0,#0x4321
  30:	d0 c0 12 34 	seth r0,#0x1234
  34:	80 e0 ff ff 	or3 r0,r0,#0xffff
  38:	d0 c0 12 34 	seth r0,#0x1234
  3c:	80 e0 ff ff 	or3 r0,r0,#0xffff
  40:	d0 c0 87 65 	seth r0,#0x8765
  44:	a0 c0 43 20 	ld r0,@\(17184,r0\)
  48:	d0 c0 87 65 	seth r0,#0x8765
  4c:	a0 a0 43 20 	ldh r0,@\(17184,r0\)
  50:	d0 c0 87 65 	seth r0,#0x8765
  54:	a0 b0 43 20 	lduh r0,@\(17184,r0\)
  58:	d0 c0 87 65 	seth r0,#0x8765
  5c:	a0 80 43 20 	ldb r0,@\(17184,r0\)
  60:	d0 c0 87 65 	seth r0,#0x8765
  64:	a0 90 43 20 	ldub r0,@\(17184,r0\)
  68:	d0 c0 12 35 	seth r0,#0x1235
  6c:	a0 c0 ff f0 	ld r0,@\(-16,r0\)
  70:	d0 c0 12 35 	seth r0,#0x1235
  74:	a0 a0 ff f0 	ldh r0,@\(-16,r0\)
  78:	d0 c0 12 35 	seth r0,#0x1235
  7c:	a0 b0 ff f0 	lduh r0,@\(-16,r0\)
  80:	d0 c0 12 35 	seth r0,#0x1235
  84:	a0 80 ff f0 	ldb r0,@\(-16,r0\)
  88:	d0 c0 12 35 	seth r0,#0x1235
  8c:	a0 90 ff f0 	ldub r0,@\(-16,r0\)
  90:	d0 c0 87 65 	seth r0,#0x8765
  94:	a0 c0 43 20 	ld r0,@\(17184,r0\)
  98:	d0 c0 87 65 	seth r0,#0x8765
  9c:	a0 a0 43 20 	ldh r0,@\(17184,r0\)
  a0:	d0 c0 87 65 	seth r0,#0x8765
  a4:	a0 b0 43 20 	lduh r0,@\(17184,r0\)
  a8:	d0 c0 87 65 	seth r0,#0x8765
  ac:	a0 80 43 20 	ldb r0,@\(17184,r0\)
  b0:	d0 c0 87 65 	seth r0,#0x8765
  b4:	a0 90 43 20 	ldub r0,@\(17184,r0\)
  b8:	d0 c0 12 35 	seth r0,#0x1235
  bc:	a0 c0 ff f0 	ld r0,@\(-16,r0\)
  c0:	d0 c0 87 65 	seth r0,#0x8765
  c4:	a0 40 43 20 	st r0,@\(17184,r0\)
  c8:	d0 c0 87 65 	seth r0,#0x8765
  cc:	a0 20 43 20 	sth r0,@\(17184,r0\)
  d0:	d0 c0 87 65 	seth r0,#0x8765
  d4:	a0 00 43 20 	stb r0,@\(17184,r0\)
  d8:	d0 c0 12 35 	seth r0,#0x1235
  dc:	a0 40 ff f0 	st r0,@\(-16,r0\)
  e0:	d0 c0 12 35 	seth r0,#0x1235
  e4:	a0 20 ff f0 	sth r0,@\(-16,r0\)
  e8:	d0 c0 12 35 	seth r0,#0x1235
  ec:	a0 00 ff f0 	stb r0,@\(-16,r0\)
  f0:	d0 c0 87 65 	seth r0,#0x8765
  f4:	a0 40 43 20 	st r0,@\(17184,r0\)
  f8:	d0 c0 87 65 	seth r0,#0x8765
  fc:	a0 20 43 20 	sth r0,@\(17184,r0\)
 100:	d0 c0 87 65 	seth r0,#0x8765
 104:	a0 00 43 20 	stb r0,@\(17184,r0\)
 108:	d0 c0 12 35 	seth r0,#0x1235
 10c:	a0 40 ff f0 	st r0,@\(-16,r0\)
