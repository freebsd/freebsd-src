#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <x>:
   0:	7259                	lapcq 4 <y>,r5
   2:	b005                	nop 

0+4 <y>:
   4:	bfbe fcff ffff      	bsr 0 <x>
   a:	b005                	nop 
