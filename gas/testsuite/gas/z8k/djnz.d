#as:
#objdump: -dr
#name: djnz/dbjnz

.*: +file format coff-z8k

Disassembly of section \.text:

0*00000000 <label1>:
   0:	8d07           	nop	
	\.\.\.
  fa:	f0fe           	djnz	r0,0x0
  fc:	f87f           	dbjnz	rl0,0x0
  fe:	8d07           	nop	

0*00000100 <label2>:
 100:	8d07           	nop	
	\.\.\.
 1fa:	f87e           	dbjnz	rl0,0x100
 1fc:	f0ff           	djnz	r0,0x100
 1fe:	8d07           	nop	
