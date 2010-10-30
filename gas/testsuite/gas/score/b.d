#as:
#objdump: -d
#source: b.s

.*: +file format .*

Disassembly of section \.text:

00000000 <L1>:
   0:	4f00      	b!		0 <L1>
   2:	4fff      	b!		0 <L1>
   4:	4ffe      	b!		0 <L1>
   6:	4ffd      	b!		0 <L1>
   8:	4ffc      	b!		0 <L1>
   a:	4ffb      	b!		0 <L1>
   c:	93ffbff4 	b		0 <L1>
  10:	8254e010 	add		r18, r20, r24
#pass
