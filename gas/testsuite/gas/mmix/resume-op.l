GAS for MMIX .*/resume-op\.s 			page 1


   1              	# Test the 'z'-type operand, RESUME\.
   2 0000 F9000001 	Main RESUME X
   3 0004 F9000000 	 RESUME 0
   4 0008 F9000001 	 RESUME 1
   5 000c F9000000 	 RESUME XX
   6              	X IS 1
   7              	XX IS 0
GAS for MMIX .*/resume-op\.s 			page 2


DEFINED SYMBOLS
.*/resume-op\.s:2      \.text:0000000000000000 Main
                            \*ABS\*:0000000000000001 X
                            \*ABS\*:0000000000000000 XX

NO UNDEFINED SYMBOLS
