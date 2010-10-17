GAS for MMIX .*/save-op.s 			page 1


   1              	#.*
   2 0000 FA2D0000 	Main SAVE \$45,0
   3 0004 FA1F0000 	 SAVE X,0
   4 0008 FA000000 	 SAVE X0,0
   5              	X IS \$31
   6              	X0 IS \$0
GAS for MMIX .*/save-op.s 			page 2


DEFINED SYMBOLS
.*/save-op.s:2      .text:0000000000000000 Main
                            \*REG\*:000000000000001f X
                            \*REG\*:0000000000000000 X0

NO UNDEFINED SYMBOLS
