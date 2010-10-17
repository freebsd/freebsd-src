GAS for MMIX .*/put-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 F604007B 	Main	PUT rJ,X
   4 0004 F613002D 		PUT rG,\$45
   5 0008 F61F00F5 		PUT rZZ,\$245
   6 000c F604006F 		PUT rJ,X0
   7 0010 F713002D 		PUT rG,45
   8 0014 F71F00F5 		PUT rZZ,245
   9 0018 F7040000 		PUT rJ,X00
  10 001c F7130000 		PUT rG,0
  11 0020 F71F0000 		PUT rZZ,0
  12              	X IS \$123
  13              	X0 IS \$111
  14              	X00 IS 0
GAS for MMIX .*/put-op\.s 			page 2


DEFINED SYMBOLS
.*/put-op\.s:3      \.text:0000000000000000 Main
                            \*REG\*:000000000000007b X
                            \*REG\*:000000000000006f X0
                            \*ABS\*:0000000000000000 X00

NO UNDEFINED SYMBOLS
