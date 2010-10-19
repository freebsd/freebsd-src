#as: --underscore --em=criself --march=common_v10_v32
#objdump: -dr

# Check that branch offsets are computed as for v32.  The
# compiler is supposed to generate four nop-type insns after
# every label to make sure the offset-by-2 or 4 doesn't matter.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
       0:	ffed ff7f           	ba .*
       4:	0000                	bcc \.\+2
	\.\.\.

00007fff <b1>:
    7fff:	ffed 0201           	ba .*
    8003:	fee0                	ba .*
    8005:	0000                	bcc \.\+2
	\.\.\.

00008101 <b2>:
	\.\.\.
    8201:	01e0                	ba .*
    8203:	ffed fefe           	ba .*

00008207 <b3>:
	\.\.\.
   10203:	ffed 0480           	ba .*

00010207 <b4>:
   10207:	b005                	setf 

00010209 <aa>:
   10209:	ff3d ff7f           	beq .*
   1020d:	0000                	bcc \.\+2
	\.\.\.

00018208 <bb1>:
   18208:	ff3d 0201           	beq .*
   1820c:	fe30                	beq .*
   1820e:	0000                	bcc \.\+2
	\.\.\.

0001830a <bb2>:
	\.\.\.
   1840a:	0130                	beq .*
   1840c:	ff3d fefe           	beq .*

00018410 <bb3>:
	\.\.\.
   2040c:	ff3d 0480           	beq .*
