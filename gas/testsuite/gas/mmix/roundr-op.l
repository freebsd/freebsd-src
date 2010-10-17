GAS for MMIX .*/roundr-op\.s 			page 1


   1              	# Round-type "R"\.
   2 0000 178701F4 	Main	FINT X,ROUND_OFF,Z
   3 0004 058702E9 		FIX X,2,\$233
   4 0008 178500F4 		FINT \$133,0,Z
   5 000c 157B04F4 		FSQRT \$123,ROUND_NEAR,Z
   6 0010 17AD02E9 		FINT \$173,2,\$233
   7 0014 058700F4 		FIX X,0,Z
   8 0018 078700F4 		FIXU X,ROUND_CURRENT,Z
   9              	X IS \$135
  10              	Z IS \$244
GAS for MMIX .*/roundr-op\.s 			page 2


DEFINED SYMBOLS
.*/roundr-op\.s:2      \.text:0000000000000000 Main
                            \*REG\*:0000000000000087 X
                            \*REG\*:00000000000000f4 Z

NO UNDEFINED SYMBOLS
