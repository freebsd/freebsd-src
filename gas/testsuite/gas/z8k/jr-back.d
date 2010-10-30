#as:
#objdump: -dr
#name: jr backward

.*: +file format coff-z8k

Disassembly of section \.text:

0*00000000 <start>:
   0:	8d07           	nop	
	\.\.\.
  fa:	e882           	jr	t,0x0
  fc:	e881           	jr	t,0x0
  fe:	e880           	jr	t,0x0
 100:	8d07           	nop	
