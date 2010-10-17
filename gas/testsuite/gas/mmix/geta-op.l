GAS for MMIX .*/geta-op\.s 			page 1


   1              	#.*
   2 0000 FD000000 	Main	SWYM 0,0,0
   3 0004 FD000000 	here	SWYM 0,0,0
   4 0008 F519FFFF 		GETA \$25,here
   5 000c F4200000 	at	GETA \$32,at
   6 0010 424E0008 		BZ \$78,there
   7 0014 F35BFFFC 		PUSHJ X0,here
   8 0018 F387FFFB 		PUSHJ X,here
   9 001c F4870005 		GETA X,there
  10 0020 F2870004 		PUSHJ X,there
  11 0024 F2490003 		PUSHJ \$73,there
  12 0028 F2380002 		PUSHJ 56,there
  13 002c 5F87FFF6 		PBEV X,here
  14 0030 FD000000 	there	SWYM 0,0,0
  15              	X IS \$135
  16              	X0 IS 91
GAS for MMIX .*/geta-op\.s 			page 2


DEFINED SYMBOLS
.*/geta-op\.s:2      \.text:0000000000000000 Main
.*/geta-op\.s:3      \.text:0000000000000004 here
.*/geta-op\.s:5      \.text:000000000000000c at
.*/geta-op\.s:14     \.text:0000000000000030 there
                            \*ABS\*:000000000000005b X0
                            \*REG\*:0000000000000087 X

NO UNDEFINED SYMBOLS
