GAS for MMIX .*/set\.s 			page 1


   1              	#.*
   2 0000 E32D0463 	Main SET \$45,1123
   3 0004 C1394300 	 SET \$57,\$67
   4 0008 C14E1F00 	 SET \$78,X
   5 000c E3750463 	 SET Y,1123
   6 0010 C1754300 	 SET Y,\$67
   7 0014 C1751F00 	 SET Y,X
   8              	X IS \$31
   9              	Y IS \$117
GAS for MMIX .*/set\.s 			page 2


DEFINED SYMBOLS
.*/set\.s:2      \.text:0000000000000000 Main
                            \*REG\*:000000000000001f X
                            \*REG\*:0000000000000075 Y

NO UNDEFINED SYMBOLS
