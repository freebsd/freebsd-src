#as:
#objdump: -d
#name: jmp cc

.*: +file format coff-z8k

Disassembly of section \.text:

00000000 <\.text>:
   0:	e01f           	jr	f,0x40
   2:	e11e           	jr	lt,0x40
   4:	e21d           	jr	le,0x40
   6:	5e03 0040      	jp	ule,0x40
   a:	5e04 0040      	jp	ov/pe,0x40
   e:	e418           	jr	ov/pe,0x40
  10:	e517           	jr	mi,0x40
  12:	e616           	jr	eq,0x40
  14:	e615           	jr	eq,0x40
  16:	e714           	jr	c/ult,0x40
  18:	e713           	jr	c/ult,0x40
  1a:	e812           	jr	t,0x40
  1c:	e911           	jr	ge,0x40
  1e:	ea10           	jr	gt,0x40
  20:	eb0f           	jr	ugt,0x40
  22:	5e0c 0040      	jp	nov/po,0x40
  26:	ec0c           	jr	nov/po,0x40
  28:	ed0b           	jr	pl,0x40
  2a:	ee0a           	jr	ne,0x40
  2c:	ee09           	jr	ne,0x40
  2e:	e408           	jr	ov/pe,0x40
  30:	e707           	jr	c/ult,0x40
  32:	ec06           	jr	nov/po,0x40
  34:	ef05           	jr	nc/uge,0x40
  36:	ee04           	jr	ne,0x40
  38:	ef03           	jr	nc/uge,0x40
  3a:	ef02           	jr	nc/uge,0x40
  3c:	e801           	jr	t,0x40
  3e:	e800           	jr	t,0x40

00000040 <dd>:
  40:	e8ff           	jr	t,0x40
  42:	e8fe           	jr	t,0x40
  44:	8d07           	nop	
  46:	8d07           	nop	
