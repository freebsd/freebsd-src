GAS for MMIX .*/pop-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 F8840DEF 	Main	POP 132,3567
   4 0004 F8845678 		POP 132,YZ
   5 0008 F8170DEF 		POP X,3567
   6 000c F8175678 		POP X,YZ
   7              	X IS 23
   8              	YZ IS #5678
GAS for MMIX .*/pop-op\.s 			page 2


DEFINED SYMBOLS
.*/pop-op\.s:3      \.text:0000000000000000 Main
                            \*ABS\*:0000000000005678 YZ
                            \*ABS\*:0000000000000017 X

NO UNDEFINED SYMBOLS
