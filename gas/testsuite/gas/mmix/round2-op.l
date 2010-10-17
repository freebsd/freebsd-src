GAS for MMIX .*/round2-op\.s 			page 1


   1              	#.*
   2 0000 088700F4 	Main	FLOT X,Z
   3 0004 0A8700E9 		FLOTU X,\$233
   4 0008 0D85005B 		SFLOT \$133,Z0
   5 000c 177B00F4 		FINT \$123,Z
   6 0010 0C8500F4 		SFLOT \$133,Z
   7 0014 0987005B 		FLOT X,Z0
   8 0018 0F7B005B 		SFLOTU \$123,Z0
   9 001c 05AD00E9 		FIX \$173,\$233
  10 0020 0BAD00D5 		FLOTU \$173,213
  11 0024 078700F4 		FIXU X,Z
  12 0028 0B870077 		FLOTU X,119
  13              	X IS \$135
  14              	Z IS \$244
  15              	Z0 IS 91
GAS for MMIX .*/round2-op\.s 			page 2


DEFINED SYMBOLS
.*/round2-op\.s:2      \.text:0000000000000000 Main
                            \*REG\*:0000000000000087 X
                            \*REG\*:00000000000000f4 Z
                            \*ABS\*:000000000000005b Z0

NO UNDEFINED SYMBOLS
