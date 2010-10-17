#as:
#objdump: -d
#name: jmp cc

.*: +file format coff-z8k

Disassembly of section \.text:

00000000 <\.text>:
   0:	9e00           	ret	f
   2:	9e01           	ret	lt
   4:	9e02           	ret	le
   6:	9e03           	ret	ule
   8:	9e04           	ret	ov/pe
   a:	9e04           	ret	ov/pe
   c:	9e05           	ret	mi
   e:	9e06           	ret	eq
  10:	9e06           	ret	eq
  12:	9e07           	ret	c/ult
  14:	9e07           	ret	c/ult
  16:	9e08           	ret	t
  18:	9e09           	ret	ge
  1a:	9e0a           	ret	gt
  1c:	9e0b           	ret	ugt
  1e:	9e0c           	ret	nov/po
  20:	9e0c           	ret	nov/po
  22:	9e0c           	ret	nov/po
  24:	9e0d           	ret	pl
  26:	9e0e           	ret	ne
  28:	9e0e           	ret	ne
  2a:	9e0f           	ret	nc/uge
  2c:	9e0f           	ret	nc/uge
  2e:	9e04           	ret	ov/pe
  30:	9e07           	ret	c/ult
  32:	9e0c           	ret	nov/po
  34:	9e0f           	ret	nc/uge
  36:	9e08           	ret	t
  38:	9e08           	ret	t

0000003a <dd>:
  3a:	e8ff           	jr	t,0x3a
  3c:	e8fe           	jr	t,0x3a
  3e:	8d07           	nop	
  40:	8d07           	nop	
