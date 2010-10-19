#objdump: -dw
#name: call operations

.*: +file format .*

Disassembly of section .text:

00000000 <foo>:
   0:	0a ea [ 	]*MOVE C,Acc.0
   2:	1a ea [ 	]*MOVE C,Acc.1
   4:	2a ea [ 	]*MOVE C,Acc.2
   6:	3a ea [ 	]*MOVE C,Acc.3
   8:	4a ea [ 	]*MOVE C,Acc.4
   a:	5a ea [ 	]*MOVE C,Acc.5
   c:	6a ea [ 	]*MOVE C,Acc.6
   e:	7a ea [ 	]*MOVE C,Acc.7
  10:	0a da [ 	]*MOVE C,#0
  12:	1a da [ 	]*MOVE C,#1
  14:	0a fa  [ 	]*MOVE Acc.0,C
  16:	1a fa  [ 	]*MOVE Acc.1,C
  18:	2a fa [ 	]*MOVE Acc.2,C
  1a:	3a fa  [ 	]*MOVE Acc.3,C
  1c:	4a fa [ 	]*MOVE Acc.4,C
  1e:	5a fa  [ 	]*MOVE Acc.5,C
  20:	6a fa  [ 	]*MOVE Acc.6,C
  22:	7a fa  [ 	]*MOVE Acc.7,C
  24:	2a da [ 	]*CPL C
  26:	0a 9a [ 	]*AND Acc.0
  28:	1a 9a [ 	]*AND Acc.1
  2a:	2a 9a [ 	]*AND Acc.2
  2c:	3a 9a [ 	]*AND Acc.3
  2e:	4a 9a [ 	]*AND Acc.4
  30:	5a 9a [ 	]*AND Acc.5
  32:	6a 9a [ 	]*AND Acc.6
  34:	7a 9a [ 	]*AND Acc.7
  36:	0a aa [ 	]*OR Acc.0
  38:	1a aa [ 	]*OR Acc.1
  3a:	2a aa [ 	]*OR Acc.2
  3c:	3a aa [ 	]*OR Acc.3
  3e:	4a aa [ 	]*OR Acc.4
  40:	5a aa [ 	]*OR Acc.5
  42:	6a aa [ 	]*OR Acc.6
  44:	7a aa [ 	]*OR Acc.7
  46:	0a ba [ 	]*XOR Acc.0
  48:	1a ba [ 	]*XOR Acc.1
  4a:	2a ba [ 	]*XOR Acc.2
  4c:	3a ba [ 	]*XOR Acc.3
  4e:	4a ba [ 	]*XOR Acc.4
  50:	5a ba [ 	]*XOR Acc.5
  52:	6a ba [ 	]*XOR Acc.6
  54:	7a ba [ 	]*XOR Acc.7
  56:	88 97 [ 	]*MOVE C , SC.1
  58:	68 87 [ 	]*MOVE C , IMR.0
  5a:	58 87 [ 	]*MOVE C , IC.0
  5c:	48 87 [ 	]*MOVE C , PSF.0
	...
