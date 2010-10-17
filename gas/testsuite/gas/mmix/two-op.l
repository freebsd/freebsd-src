GAS for MMIX .*/two-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 E0175840 	Main	SETH X,YZ
   4 0004 E12D5840 		SETMH \$45,YZ
   5 0008 E8171ED4 		ORH X,7892
   6 000c E92D3039 		ORMH \$45,12345
   7              	
   8 0010 E2175840 		SETML X,YZ
   9 0014 E32D5840 		SETL \$45,YZ
  10 0018 EA171ED4 		ORML X,7892
  11 001c EB2D3039 		ORL \$45,12345
  12              	
  13 0020 E42D3039 		INCH \$45,12345
  14 0024 E5171ED4 		INCMH X,7892
  15 0028 EC2D5840 		ANDNH \$45,YZ
  16 002c ED175840 		ANDNMH X,YZ
  17              	
  18 0030 E6175840 		INCML X,YZ
  19 0034 E72D5840 		INCL \$45,YZ
  20 0038 EE171ED4 		ANDNML X,7892
  21 003c EF2D3039 		ANDNL \$45,12345
  22              	X IS \$23
  23              	YZ IS #5678\+456
GAS for MMIX .*/two-op\.s 			page 2


DEFINED SYMBOLS
.*/two-op\.s:3      \.text:0000000000000000 Main
                            \*REG\*:0000000000000017 X
                            \*ABS\*:0000000000005840 YZ

NO UNDEFINED SYMBOLS
