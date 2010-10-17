GAS for MMIX .*/sync-op\.s 			page 1


   1              	#.*
   2 0000 FC6E6406 	Main	SYNC 7234566
   3 0004 FC000000 		SYNC 0
   4 0008 FC000001 		SYNC 1
   5 000c FC000000 		SYNC Zz
   6 0010 FC000001 		SYNC Zo
   7 0014 FC7C2242 		SYNC Z
   8              	Zz IS 0
   9              	Zo IS 1
  10              	Z IS 8135234
GAS for MMIX .*/sync-op\.s 			page 2


DEFINED SYMBOLS
.*/sync-op\.s:2      \.text:0000000000000000 Main
                            \*ABS\*:0000000000000000 Zz
                            \*ABS\*:0000000000000001 Zo
                            \*ABS\*:00000000007c2242 Z

NO UNDEFINED SYMBOLS
