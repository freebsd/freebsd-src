#objdump: -dw
#name: bit opp

.*: +file format .*

Disassembly of section .text:
0+000 <foo>:
   0:	0a ea [ 	]*MOVE C,Acc.0
   2:	1a ea [ 	]*MOVE C,Acc.1
   4:	2a ea [ 	]*MOVE C,Acc.2
   6:	3a ea [ 	]*MOVE C,Acc.3
   8:	4a ea [ 	]*MOVE C,Acc.4
   a:	5a ea [ 	]*MOVE C,Acc.5
   c:	6a ea [ 	]*MOVE C,Acc.6
   e:	7a ea [ 	]*MOVE C,Acc.7
  10:	8a ea [ 	]*MOVE C,Acc.8
  12:	9a ea [ 	]*MOVE C,Acc.9
  14:	aa ea [ 	]*MOVE C,Acc.10
  16:	ba ea [ 	]*MOVE C,Acc.11
  18:	ca ea [ 	]*MOVE C,Acc.12
  1a:	da ea [ 	]*MOVE C,Acc.13
  1c:	ea ea [ 	]*MOVE C,Acc.14
  1e:	fa ea [ 	]*MOVE C,Acc.15
  20:	0a da [ 	]*MOVE C,#0
  22:	1a da [ 	]*MOVE C,#1
  24:	0a fa [ 	]*MOVE Acc.0,C
  26:	1a fa [ 	]*MOVE Acc.1,C
  28:	2a fa [ 	]*MOVE Acc.2,C
  2a:	3a fa [ 	]*MOVE Acc.3,C
  2c:	4a fa [ 	]*MOVE Acc.4,C
  2e:	5a fa [ 	]*MOVE Acc.5,C
  30:	6a fa [ 	]*MOVE Acc.6,C
  32:	7a fa [ 	]*MOVE Acc.7,C
  34:	8a fa [ 	]*MOVE Acc.8,C
  36:	9a fa [ 	]*MOVE Acc.9,C
  38:	aa fa [ 	]*MOVE Acc.10,C
  3a:	ba fa [ 	]*MOVE Acc.11,C
  3c:	ca fa [ 	]*MOVE Acc.12,C
  3e:	da fa [ 	]*MOVE Acc.13,C
  40:	ea fa [ 	]*MOVE Acc.14,C
  42:	fa fa [ 	]*MOVE Acc.15,C
  44:	2a da [ 	]*CPL C
  46:	0a 9a [ 	]*AND Acc.0
  48:	1a 9a [ 	]*AND Acc.1
  4a:	2a 9a [ 	]*AND Acc.2
  4c:	3a 9a [ 	]*AND Acc.3
  4e:	4a 9a [ 	]*AND Acc.4
  50:	5a 9a [ 	]*AND Acc.5
  52:	6a 9a [ 	]*AND Acc.6
  54:	7a 9a [ 	]*AND Acc.7
  56:	8a 9a [ 	]*AND Acc.8
  58:	9a 9a [ 	]*AND Acc.9
  5a:	aa 9a [ 	]*AND Acc.10
  5c:	ba 9a [ 	]*AND Acc.11
  5e:	ca 9a [ 	]*AND Acc.12
  60:	da 9a [ 	]*AND Acc.13
  62:	ea 9a [ 	]*AND Acc.14
  64:	fa 9a [ 	]*AND Acc.15
  66:	0a aa [ 	]*OR Acc.0
  68:	1a aa [ 	]*OR Acc.1
  6a:	2a aa [ 	]*OR Acc.2
  6c:	3a aa [ 	]*OR Acc.3
  6e:	4a aa [ 	]*OR Acc.4
  70:	5a aa [ 	]*OR Acc.5
  72:	6a aa [ 	]*OR Acc.6
  74:	7a aa [ 	]*OR Acc.7
  76:	8a aa [ 	]*OR Acc.8
  78:	9a aa [ 	]*OR Acc.9
  7a:	aa aa [ 	]*OR Acc.10
  7c:	ba aa [ 	]*OR Acc.11
  7e:	ca aa [ 	]*OR Acc.12
  80:	da aa [ 	]*OR Acc.13
  82:	ea aa [ 	]*OR Acc.14
  84:	fa aa [ 	]*OR Acc.15
  86:	0a ba [ 	]*XOR Acc.0
  88:	1a ba [ 	]*XOR Acc.1
  8a:	2a ba [ 	]*XOR Acc.2
  8c:	3a ba [ 	]*XOR Acc.3
  8e:	4a ba [ 	]*XOR Acc.4
  90:	5a ba [ 	]*XOR Acc.5
  92:	6a ba [ 	]*XOR Acc.6
  94:	7a ba [ 	]*XOR Acc.7
  96:	8a ba [ 	]*XOR Acc.8
  98:	9a ba [ 	]*XOR Acc.9
  9a:	aa ba [ 	]*XOR Acc.10
  9c:	ba ba [ 	]*XOR Acc.11
  9e:	ca ba [ 	]*XOR Acc.12
  a0:	da ba [ 	]*XOR Acc.13
  a2:	ea ba [ 	]*XOR Acc.14
  a4:	fa ba [ 	]*XOR Acc.15
  a6:	88 97 [ 	]*MOVE C , SC.1
  a8:	68 87 [ 	]*MOVE C , IMR.0
  aa:	58 87 [ 	]*MOVE C , IC.0
  ac:	48 87 [ 	]*MOVE C , PSF.0
	...
