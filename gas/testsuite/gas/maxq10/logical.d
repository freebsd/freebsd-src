#objdump:-dw
#name: Jump operations

.*: +file format .*

Disassembly of section .text:
0+000 <foo>:
   0:	00 08 [ 	]*MOVE  AP, #00h
   2:	ff 1a [ 	]*AND  #ffh
   4:	f0 2a [ 	]*OR  #f0h
   6:	fe 3a [ 	]*XOR  #feh
   8:	1a 8a [ 	]*CPL 
   a:	9a 8a [ 	]*NEG 
   c:	2a 8a [ 	]*SLA 
   e:	3a 8a [ 	]*SLA2 
  10:	6a 8a [ 	]*SLA4 
  12:	4a 8a [ 	]*RL 
  14:	5a 8a [ 	]*RLC 
  16:	fa 8a [ 	]*SRA 
  18:	ea 8a [ 	]*SRA2 
  1a:	ba 8a [ 	]*SRA4 
  1c:	aa 8a [ 	]*SR 
  1e:	ca 8a [ 	]*RR 
  20:	da 8a [ 	]*RRC 
	...
