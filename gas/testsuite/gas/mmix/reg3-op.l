GAS for MMIX .*/reg3-op\.s 			page 1


   1              	#.*
   2              	#.*
   3 0000 18170C43 	Main	MUL X,Y,Z
   4 0004 20200C43 		ADD \$32,Y,Z
   5 0008 2A0C2043 		4ADDU Y,\$32,Z
   6 000c 2CE88543 		8ADDU \$232,\$133,Z
   7 0010 2E170C49 		16ADDU X,Y,\$73
   8 0014 3E1F0CE9 		SRU \$31,Y,\$233
   9 0018 601726D4 		CSN X,\$38,\$212
  10 001c 7C04AFB5 		ZSNP \$4,\$175,\$181
  11              	
  12 0020 1B170CB0 		MULU X,Y,Z0
  13 0024 39200CB0 		SL \$32,Y,Z0
  14 0028 330C20B0 		CMPU Y,\$32,Z0
  15 002c 29E885B0 		2ADDU \$232,\$133,Z0
  16 0030 DF170CCB 		MXOR X,Y,203
  17 0034 C11F0CD5 		OR \$31,Y,213
  18 0038 CD1726D3 		NAND X,\$38,211
  19 003c D304AFA1 		WDIF \$4,\$175,161
  20              	
  21 0040 DB170C00 		SADD X,Y,0
  22 0044 DF200C00 		MXOR \$32,Y,0
  23 0048 C30C2000 		ORN Y,\$32,0
  24 004c CBE88500 		ANDN \$232,\$133,0
  25 0050 2F170C00 		16ADDU X,Y,0
  26 0054 391F0C00 		SL \$31,Y,0
  27 0058 23172600 		ADDU X,\$38,0
  28 005c 3104AF00 		CMP \$4,\$175,0
  29              	X IS \$23
  30              	Y IS \$12
  31              	Z IS \$67
  32              	Z0 IS 176
GAS for MMIX .*/reg3-op\.s 			page 2


DEFINED SYMBOLS
.*/reg3-op\.s:3      \.text:0000000000000000 Main
                            \*REG\*:0000000000000017 X
                            \*REG\*:000000000000000c Y
                            \*REG\*:0000000000000043 Z
                            \*ABS\*:00000000000000b0 Z0

NO UNDEFINED SYMBOLS
