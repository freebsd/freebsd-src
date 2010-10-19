#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
       0:	0ce0                	ba c <a\+0xc>
       2:	b005                	nop 
       4:	bf0e 0980 0000      	ba 800d <b1>
       a:	b005                	nop 
       c:	f930                	beq 4 <a\+0x4>
       e:	ff2d ff7f           	bne 800d <b1>
      12:	0000                	bcc \.
	\.\.\.

0000800d <b1>:
    800d:	ff0d 0201           	bhs 810f <b2>
    8011:	fe90                	bhi 810f <b2>
    8013:	0000                	bcc \.
	\.\.\.

0000810f <b2>:
	\.\.\.
    820f:	0110                	bcs 810f <b2>
    8211:	ff1d fefe           	blo 810f <b2>

00008215 <b3>:
	\.\.\.
   10215:	ff8d 0080           	bls 8215 <b3>
   10219:	0ce0                	ba 10225 <b3\+0x8010>
   1021b:	b005                	nop 
   1021d:	bf0e f87f ffff      	ba 8215 <b3>
   10223:	b005                	nop 
   10225:	f9f0                	bsb 1021d <b3\+0x8008>

00010227 <b4>:
	\.\.\.
