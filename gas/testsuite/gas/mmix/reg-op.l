GAS for MMIX .*/reg-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 10170C43 	Main	FMUL X,Y,Z
   4 0004 01200C43 		FCMP \$32,Y,Z
   5 0008 040C2043 		FADD Y,\$32,Z
   6 000c 02E88543 		FUN \$232,\$133,Z
   7 0010 03170C49 		FEQL X,Y,\$73
   8 0014 161F0CE9 		FREM \$31,Y,\$233
   9 0018 061726D4 		FSUB X,\$38,\$212
  10 001c 1304AFB5 		FEQLE \$4,\$175,\$181
  11              	X IS \$23
  12              	Y IS \$12
  13              	Z IS \$67
GAS for MMIX .*/reg-op\.s 			page 2


DEFINED SYMBOLS
.*/reg-op\.s:3      \.text:0000000000000000 Main
                            \*REG\*:0000000000000017 X
                            \*REG\*:000000000000000c Y
                            \*REG\*:0000000000000043 Z

NO UNDEFINED SYMBOLS
