#as:
#objdump: -dr
#name: calr

.*: +file format coff-z8k

Disassembly of section \.text:

0*00000000 <label1>:
       0:	d803           	calr	0xffc
       2:	d800           	calr	0x1004
	\.\.\.

0*00000ffc <label2>:
     ffc:	d7ff           	calr	0x0
     ffe:	8d07           	nop	
    1000:	8d07           	nop	
    1002:	8d07           	nop	

0*00001004 <label3>:
    1004:	8d07           	nop	
