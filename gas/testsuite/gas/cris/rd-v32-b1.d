#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
       0:	bf0e 0580 0000      	ba 8005 <b1>
       6:	ffed ff7f           	ba 8005 <b1>
       a:	0000                	bcc \.
	\.\.\.

00008005 <b1>:
    8005:	ffed 0201           	ba 8107 <b2>
    8009:	fee0                	ba 8107 <b2>
    800b:	0000                	bcc \.
	\.\.\.

00008107 <b2>:
	\.\.\.
    8207:	01e0                	ba 8107 <b2>
    8209:	ffed fefe           	ba 8107 <b2>

0000820d <b3>:
	\.\.\.
   1020d:	ffed 0080           	ba 820d <b3>
   10211:	bf0e fc7f ffff      	ba 820d <b3>

00010217 <b4>:
	\.\.\.
