GAS for MMIX .*/swym-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 FD840DEF 	Main	SWYM 132,3567
   4 0004 FF840DEF 		TRIP 132,3567
   5 0008 00845678 		TRAP 132,YZ
   6 000c 00170DEF 		TRAP X,3567
   7 0010 0023CACE 		TRAP 2345678
   8 0014 FF170C43 		TRIP X,Y,Z
   9 0018 FF175678 		TRIP X,YZ
  10 001c FD12D687 		SWYM XYZ
  11              	X IS 23
  12              	Y IS 12
  13              	Z IS 67
  14              	YZ IS #5678
  15              	XYZ IS 1234567
GAS for MMIX .*/swym-op\.s 			page 2


DEFINED SYMBOLS
.*/swym-op\.s:3      \.text:0000000000000000 Main
                            \*ABS\*:0000000000005678 YZ
                            \*ABS\*:0000000000000017 X
                            \*ABS\*:000000000000000c Y
                            \*ABS\*:0000000000000043 Z
                            \*ABS\*:000000000012d687 XYZ

NO UNDEFINED SYMBOLS
