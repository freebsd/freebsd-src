#as:
#objdump: -dr
#name: jr forward

.*: +file format coff-z8k

Disassembly of section \.text:

0*00000000 <.text>:
   0:	e87f           	jr	t,0x100
   2:	e87e           	jr	t,0x100
   4:	e87d           	jr	t,0x100
	\.\.\.

0*00000100 <dest>:
 100:	8d07           	nop	
