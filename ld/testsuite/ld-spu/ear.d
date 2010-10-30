#as:
#objdump: -Dr
#name: ear

.*: +file format .*

Disassembly of section \.text:

0+00 <_start>:
   0:	32 00 00 00 	br	0
			0: SPU_REL16	_start

Disassembly of section \.data:

0+00 <_EAR_main>:
	\.\.\.

0+20 <_EAR_foo>:
	\.\.\.
Disassembly of section \.toe:

0+00 <_EAR_>:
	\.\.\.

0+10 <_EAR_bar>:
	\.\.\.
Disassembly of section \.data\.blah:

0+00 <_EAR_blah>:
	\.\.\.
